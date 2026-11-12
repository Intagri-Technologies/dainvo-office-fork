/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <recentfilemetadataloader.hxx>

#include <comphelper/threadpool.hxx>
#include <config_folders.h>
#include <recentfilemetadataprotocol.hxx>
#include <sfx2/recentfilemetadata.hxx>
#include <rtl/bootstrap.hxx>
#include <osl/process.h>
#include <osl/file.hxx>
#include <sal/log.hxx>
#include <vcl/svapp.hxx>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <utility>

namespace sfx2
{
namespace
{
// The child is expendable: cancellation requests termination but never joins it.
// In particular a kernel-stalled child must not keep Office's worker pool alive.
struct MetadataProcess
{
    oslProcess hProcess = nullptr;
    oslFileHandle hOutput = nullptr;
    bool bExited = false;
    ~MetadataProcess()
    {
        if (hProcess)
        {
            if (!bExited)
                osl_terminateProcess(hProcess);
            osl_freeProcessHandle(hProcess);
        }
        if (hOutput)
            osl_closeFile(hOutput);
    }
};

struct SupersededRequest {};
}

struct RecentFileMetadataLoader::Impl : public std::enable_shared_from_this<Impl>
{
    struct Request
    {
        std::uint64_t nGeneration;
        std::vector<OUString> aURLs;
    };
    struct Result
    {
        std::uint64_t nGeneration;
        RecentFileMetadataCache aMetadata;
    };
    class Task final : public comphelper::ThreadTask
    {
        std::shared_ptr<Impl> mpState;
        void doWork() override { mpState->run(); }
    public:
        explicit Task(std::shared_ptr<Impl> pState)
            : ThreadTask(pState->mxTaskTag)
            , mpState(std::move(pState))
        {
        }
    };

    std::mutex maMutex;
    const Loader maLoad; // injectable reader used by unit tests only
    const OUString maHelperURL;
    const std::shared_ptr<comphelper::ThreadTaskTag> mxTaskTag
        = comphelper::ThreadPool::createThreadTaskTag();
    Completion maComplete;
    std::uint64_t mnGeneration = 0;
    bool mbCancelled = false;
    bool mbRunning = false;
    std::optional<Request> moPending;
    std::optional<Result> moResult;
    ImplSVEvent* mpEvent = nullptr;

    // Accessed only on the UI thread; workers never access the registry.
    static std::vector<std::weak_ptr<Impl>>& instances()
    {
        static std::vector<std::weak_ptr<Impl>> aInstances;
        return aInstances;
    }

    Impl(Completion complete, Loader load, OUString aHelperURL)
        : maLoad(std::move(load)), maHelperURL(std::move(aHelperURL))
        , maComplete(std::move(complete))
    {
    }

    // Called only on the UI thread. The worker holds this mutex only for short
    // state transitions, never during file I/O or while executing a UI callback.
    void clearDelivery()
    {
        if (mpEvent)
            Application::RemoveUserEvent(mpEvent);
        mpEvent = nullptr;
        moResult.reset();
    }

    void request(std::vector<OUString> aURLs)
    {
        std::scoped_lock aGuard(maMutex);
        if (mbCancelled)
            return;
        clearDelivery();
        moPending = Request{ ++mnGeneration, std::move(aURLs) };
        if (mbRunning)
            return;
        mbRunning = true;
        comphelper::ThreadPool::getSharedOptimalPool().pushTask(
            std::make_unique<Task>(shared_from_this()));
    }

    void cancel()
    {
        std::scoped_lock aGuard(maMutex);
        mbCancelled = true;
        ++mnGeneration;
        moPending.reset();
        clearDelivery();
        maComplete = {};
    }

    bool isCurrent(std::uint64_t nGeneration)
    {
        std::scoped_lock aGuard(maMutex);
        return !mbCancelled && nGeneration == mnGeneration;
    }

    RecentFileMetadataCache loadInProcess(const Request& rRequest)
    {
        // Deduplicate and bound each response below the minimum pipe capacity:
        // the child can exit before we read, so the supervisor never blocks on IPC.
        std::map<OUString, RecentFileMetadataCache::ModificationTime> aTimes;
        std::vector<OUString> aURLs;
        for (const auto& rURL : rRequest.aURLs)
            if (aTimes.emplace(rURL, std::nullopt).second)
                aURLs.push_back(rURL);
        for (std::size_t nStart = 0; nStart < aURLs.size(); nStart += metadata::MaxURLs)
        {
            if (!isCurrent(rRequest.nGeneration))
                throw SupersededRequest();
            const auto nCount = std::min(metadata::MaxURLs, aURLs.size() - nStart);
            std::array<rtl_uString*, metadata::MaxURLs> aArgs{};
            for (std::size_t i = 0; i < nCount; ++i)
                aArgs[i] = aURLs[nStart + i].pData;
            MetadataProcess aChild;
            if (osl_executeProcess_WithRedirectedIO(
                    maHelperURL.pData, aArgs.data(), nCount, osl_Process_HIDDEN,
                    nullptr, nullptr, nullptr, 0, &aChild.hProcess,
                    nullptr, &aChild.hOutput, nullptr) != osl_Process_E_None)
                break; // unavailable helper: retain unknown dates, never fall back to stat

            const auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            const TimeValue aPoll(0, 20'000'000);
            while (isCurrent(rRequest.nGeneration)
                   && std::chrono::steady_clock::now() < aDeadline)
            {
                const auto eWait = osl_joinProcessWithTimeout(aChild.hProcess, &aPoll);
                if (eWait == osl_Process_E_None)
                {
                    aChild.bExited = true;
                    break;
                }
                if (eWait != osl_Process_E_TimedOut)
                    break;
            }
            if (!isCurrent(rRequest.nGeneration))
                throw SupersededRequest();
            if (!aChild.bExited)
                break; // one timeout budget per refresh, no retry storm on slow mounts
            oslProcessInfo aInfo{};
            aInfo.Size = sizeof(aInfo);
            if (osl_getProcessInfo(aChild.hProcess, osl_Process_EXITCODE, &aInfo)
                    != osl_Process_E_None || aInfo.Code != 0)
                continue;
            std::array<metadata::Record, metadata::MaxURLs> aRecords{};
            sal_uInt64 nRead = 0;
            const auto nBytes = nCount * sizeof(metadata::Record);
            if (osl_readFile(aChild.hOutput, aRecords.data(), nBytes, &nRead)
                    != osl_File_E_None || nRead != nBytes)
                continue;
            for (std::size_t i = 0; i < nCount; ++i)
            {
                const auto& rRecord = aRecords[i];
                if (rRecord[0] == 1 && rRecord[2] < 1'000'000'000)
                    aTimes[aURLs[nStart + i]] = TimeValue(rRecord[1], rRecord[2]);
            }
        }
        RecentFileMetadataCache aMetadata;
        aMetadata.refresh(rRequest.aURLs, [&](const OUString& rURL) { return aTimes.at(rURL); });
        return aMetadata;
    }

    void run()
    {
        for (;;)
        {
            Request aRequest;
            {
                std::scoped_lock aGuard(maMutex);
                if (mbCancelled || !moPending)
                {
                    mbRunning = false;
                    return;
                }
                aRequest = std::move(*moPending);
                moPending.reset();
            }

            RecentFileMetadataCache aMetadata;
            try
            {
                if (!maLoad)
                    aMetadata = loadInProcess(aRequest);
                else
                {
                    aMetadata.refresh(aRequest.aURLs, [&](const OUString& rURL) {
                        if (!isCurrent(aRequest.nGeneration))
                            throw SupersededRequest();
                        try
                        {
                            return maLoad(rURL);
                        }
                        catch (...)
                        {
                            SAL_WARN("sfx.dialog", "Could not read recent-file metadata");
                            return RecentFileMetadataCache::ModificationTime();
                        }
                    });
                }
            }
            catch (const SupersededRequest&)
            {
                continue;
            }
            catch (...)
            {
                SAL_WARN("sfx.dialog", "Could not build recent-file metadata snapshot");
            }

            std::scoped_lock aGuard(maMutex);
            if (mbCancelled || aRequest.nGeneration != mnGeneration)
                continue;
            moResult = Result{ aRequest.nGeneration, std::move(aMetadata) };
            // The client keeps Impl alive until it cancels this event; the task
            // also owns Impl. No callback or worker dereferences a widget here.
            mpEvent = Application::PostUserEvent(LINK(this, Impl, CompleteHdl));
            if (!mpEvent)
                moResult.reset();
        }
    }

    DECL_LINK(CompleteHdl, void*, void);
};

IMPL_LINK_NOARG(RecentFileMetadataLoader::Impl, CompleteHdl, void*, void)
{
    // Completion can destroy the client, so retain the state through the callback.
    auto xKeepAlive = shared_from_this();
    Completion aComplete;
    std::optional<Result> oResult;
    {
        std::scoped_lock aGuard(maMutex);
        mpEvent = nullptr;
        if (mbCancelled || !moResult || moResult->nGeneration != mnGeneration)
            return;
        oResult = std::move(moResult);
        moResult.reset();
        aComplete = maComplete;
    }
    if (aComplete)
        aComplete(std::move(oResult->aMetadata));
}

RecentFileMetadataLoader::RecentFileMetadataLoader(Completion complete, Loader load,
                                                 OUString aHelperURL)
{
    if (aHelperURL.isEmpty())
    {
        aHelperURL = u"$BRAND_BASE_DIR/" LIBO_BIN_FOLDER "/recentfilemetadata"_ustr;
#if defined(_WIN32)
        aHelperURL += ".exe";
#endif
        rtl::Bootstrap::expandMacros(aHelperURL);
    }
    mpImpl = std::make_shared<Impl>(std::move(complete), std::move(load), std::move(aHelperURL));
    auto& rInstances = Impl::instances();
    std::erase_if(rInstances, [](const auto& rWeak) { return rWeak.expired(); });
    rInstances.push_back(mpImpl);
    // Initialize VCL's event target before the supervisor can post completion.
    Application::GetDefaultDevice();
}

void RecentFileMetadataLoader::cancelAll()
{
    std::vector<std::shared_ptr<Impl>> aActive;
    for (const auto& rWeak : Impl::instances())
        if (auto xState = rWeak.lock())
        {
            xState->cancel();
            aActive.push_back(std::move(xState));
        }
    // Only supervisors run in this pool. They observe cancellation at the next
    // 20 ms process-status wait; filesystem syscalls live in disposable children.
    for (const auto& xState : aActive)
        comphelper::ThreadPool::getSharedOptimalPool().waitUntilDone(xState->mxTaskTag, false);
}

void cancelRecentFileMetadataRequests() { RecentFileMetadataLoader::cancelAll(); }

RecentFileMetadataLoader::~RecentFileMetadataLoader() { mpImpl->cancel(); }

void RecentFileMetadataLoader::refresh(std::vector<OUString> aURLs)
{
    mpImpl->request(std::move(aURLs));
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
