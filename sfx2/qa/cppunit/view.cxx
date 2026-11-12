/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_poppler.h>
#include <test/unoapi_test.hxx>

#include <boost/property_tree/json_parser.hpp>

#include <com/sun/star/drawing/XDrawView.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/StartModule.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/util/XModifiable.hpp>

#include <sfx2/app.hxx>
#include <sfx2/sfxsids.hrc>
#include <sfx2/viewfrm.hxx>
#include <svl/itemset.hxx>
#include <svl/intitem.hxx>
#include <sfx2/request.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/lokhelper.hxx>
#include <sfx2/sfxbasemodel.hxx>
#include <sfx2/signaturestate.hxx>
#include <tools/json_writer.hxx>
#include <rtl/ustrbuf.hxx>
#include <comphelper/base64.hxx>
#include <comphelper/propertyvalue.hxx>
#include <vcl/scheduler.hxx>
#include <vcl/svapp.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <unotools/historyoptions.hxx>
#include <unotools/tempfile.hxx>
#include <recentfilemetadatacache.hxx>
#include <recentfilemetadataloader.hxx>
#include <sfx2/recentfilemetadata.hxx>
#include <osl/file.hxx>
#include <osl/process.h>
#include <comphelper/threadpool.hxx>
#include <comphelper/scopeguard.hxx>
#include <com/sun/star/awt/PosSize.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace com::sun::star;

/// Covers sfx2/source/view/ fixes.
class Sfx2ViewTest : public UnoApiTest
{
public:
    Sfx2ViewTest()
        : UnoApiTest(u"/sfx2/qa/cppunit/data/"_ustr)
    {
    }

    void setUp() override
    {
        UnoApiTest::setUp();
        MacrosTest::setUpX509(m_directories, "sfx2_view");
    }
};

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testDrawSaveKeepsOdg)
{
    utl::TempFileNamed aDirectory(nullptr, true);
    aDirectory.EnableKillingFile();
    const OUString aURL = aDirectory.GetURL() + "/drawing.odg";
    loadFromFile(u"reload-page.odg");

    uno::Reference<frame::XStorable> xStorable(mxComponent, uno::UNO_QUERY_THROW);
    xStorable->storeAsURL(aURL, {});
    CPPUNIT_ASSERT_EQUAL(aURL, xStorable->getLocation());

    uno::Reference<drawing::XDrawPagesSupplier> xPagesSupplier(mxComponent, uno::UNO_QUERY_THROW);
    const auto xPages = xPagesSupplier->getDrawPages();
    const auto nPages = xPages->getCount();
    xPages->insertNewByIndex(nPages);
    uno::Reference<util::XModifiable> xModified(mxComponent, uno::UNO_QUERY_THROW);
    CPPUNIT_ASSERT(xModified->isModified());
    xStorable->store();
    CPPUNIT_ASSERT(!xModified->isModified());

    dispose();
    {
        auto xZip = packages::zip::ZipFileAccess::createWithURL(m_xContext, aURL);
        uno::Reference<io::XInputStream> xMimeType(xZip->getByName(u"mimetype"_ustr),
                                                   uno::UNO_QUERY_THROW);
        uno::Sequence<sal_Int8> aBytes;
        constexpr OString aExpected = "application/vnd.oasis.opendocument.graphics"_ostr;
        CPPUNIT_ASSERT_EQUAL(aExpected.getLength(),
                             xMimeType->readBytes(aBytes, aExpected.getLength() + 1));
        CPPUNIT_ASSERT_EQUAL(aExpected, OString(reinterpret_cast<const char*>(aBytes.getConstArray()),
                                                aBytes.getLength()));
    }
    loadFromURL(aURL);
    xPagesSupplier.set(mxComponent, uno::UNO_QUERY_THROW);
    CPPUNIT_ASSERT_EQUAL(nPages + 1, xPagesSupplier->getDrawPages()->getCount());
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentFileMetadataSnapshot)
{
    sfx2::RecentFileMetadataCache aCache;
    const OUString aExisting = u"file:///existing.odt"_ustr;
    const OUString aMissing = u"file:///missing.odt"_ustr;
    const OUString aUnknown = u"file:///unknown.odt"_ustr;
    int nReads = 0;
    sal_uInt32 nTimestamp = 100;
    bool bMissing = true;
    const auto load = [&](const OUString& rURL)
        -> sfx2::RecentFileMetadataCache::ModificationTime {
        ++nReads;
        if (rURL == aMissing && bMissing)
            return {};
        return TimeValue{ nTimestamp, 0 };
    };

    aCache.refresh({ aExisting, aMissing, aExisting }, load);
    CPPUNIT_ASSERT_EQUAL(2, nReads);
    CPPUNIT_ASSERT_EQUAL(std::size_t(2), aCache.size());
    nTimestamp = 200;
    bMissing = false;
    // Presentation reads never retry missing/unknown files or reload changed timestamps.
    for (int i = 0; i < 1000; ++i)
    {
        CPPUNIT_ASSERT_EQUAL(sal_uInt32(100), aCache.lookup(aExisting)->Seconds);
        CPPUNIT_ASSERT(!aCache.lookup(aMissing));
        CPPUNIT_ASSERT(!aCache.lookup(aUnknown));
    }
    CPPUNIT_ASSERT_EQUAL(2, nReads);
    aCache.refresh({ aMissing }, load);
    CPPUNIT_ASSERT_EQUAL(3, nReads);
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), aCache.size());
    CPPUNIT_ASSERT(!aCache.lookup(aExisting));
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(200), aCache.lookup(aMissing)->Seconds);
    aCache.refresh({}, load);
    CPPUNIT_ASSERT_EQUAL(std::size_t(0), aCache.size());
    CPPUNIT_ASSERT_EQUAL(3, nReads);
}

namespace
{
bool finishMetadataTasks()
{
    // shutdown() can wait for a queue-change notification when work is still
    // queued. Join only once idle, without pumping the completion events.
    const auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    auto& rPool = comphelper::ThreadPool::getSharedOptimalPool();
    do
    {
        if (rPool.joinThreadsIfIdle())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < aDeadline);
    return false;
}

class MetadataReadGate
{
    std::mutex maMutex;
    std::condition_variable maCondition;
    bool mbEntered = false;
    bool mbReleased = false;
    bool mbTimedOut = false;

public:
    void block()
    {
        std::unique_lock aGuard(maMutex);
        mbEntered = true;
        maCondition.notify_all();
        mbTimedOut = !maCondition.wait_for(aGuard, std::chrono::seconds(10),
                                           [&] { return mbReleased; });
    }

    bool waitUntilEntered()
    {
        std::unique_lock aGuard(maMutex);
        return maCondition.wait_for(aGuard, std::chrono::seconds(5), [&] { return mbEntered; });
    }

    void release()
    {
        std::scoped_lock aGuard(maMutex);
        mbReleased = true;
        maCondition.notify_all();
    }

    bool timedOut()
    {
        std::scoped_lock aGuard(maMutex);
        return mbTimedOut;
    }
};

struct MetadataEventProbe
{
    bool bDelivered = false;
    DECL_LINK(Deliver, void*, void);
};

IMPL_LINK_NOARG(MetadataEventProbe, Deliver, void*, void) { bDelivered = true; }
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentMetadataBackgroundRefresh)
{
    auto xGate = std::make_shared<MetadataReadGate>();
    std::vector<OUString> aReads;
    std::optional<sfx2::RecentFileMetadataCache> oResult;
    int nCallbacks = 0;
    bool bReadOnUIThread = false;
    bool bCompletedOnUIThread = false;
    auto xLoader = std::make_unique<sfx2::RecentFileMetadataLoader>(
        [&](sfx2::RecentFileMetadataCache aMetadata) {
            ++nCallbacks;
            bCompletedOnUIThread = Application::IsMainThread();
            oResult = std::move(aMetadata);
        },
        [&](const OUString& rURL) -> sfx2::RecentFileMetadataCache::ModificationTime {
            bReadOnUIThread |= Application::IsMainThread();
            aReads.push_back(rURL);
            if (rURL == "slow")
                xGate->block();
            if (rURL == "missing")
                return {};
            if (rURL == "error")
                throw std::runtime_error("simulated metadata failure");
            return TimeValue{ 100, 0 };
        });
    comphelper::ScopeGuard aCleanup([&] {
        xLoader.reset();
        xGate->release();
        finishMetadataTasks();
        Scheduler::ProcessEventsToIdle();
    });

    xLoader->refresh({ u"slow"_ustr, u"obsolete-tail"_ustr });
    CPPUNIT_ASSERT(xGate->waitUntilEntered());
    // A stalled file query must not stop normal UI event delivery.
    MetadataEventProbe aProbe;
    Application::PostUserEvent(LINK(&aProbe, MetadataEventProbe, Deliver));
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(aProbe.bDelivered);
    CPPUNIT_ASSERT_EQUAL(0, nCallbacks);

    // Rapid refreshes replace pending work instead of growing a task queue.
    for (int i = 0; i < 100; ++i)
        xLoader->refresh({ OUString::number(i) });
    xLoader->refresh({ u"latest"_ustr, u"missing"_ustr, u"error"_ustr });
    xGate->release();
    CPPUNIT_ASSERT(finishMetadataTasks());
    CPPUNIT_ASSERT_EQUAL(0, nCallbacks); // delivery is queued, never on the worker
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, nCallbacks);
    CPPUNIT_ASSERT(!bReadOnUIThread);
    CPPUNIT_ASSERT(bCompletedOnUIThread);
    CPPUNIT_ASSERT_EQUAL(std::size_t(4), aReads.size());
    CPPUNIT_ASSERT_EQUAL(u"slow"_ustr, aReads[0]);
    CPPUNIT_ASSERT_EQUAL(u"latest"_ustr, aReads[1]);
    CPPUNIT_ASSERT(oResult.has_value());
    CPPUNIT_ASSERT_EQUAL(std::size_t(3), oResult->size());
    CPPUNIT_ASSERT(oResult->lookup(u"latest"_ustr));
    CPPUNIT_ASSERT(!oResult->lookup(u"slow"_ustr));
    CPPUNIT_ASSERT(!oResult->lookup(u"missing"_ustr));
    CPPUNIT_ASSERT(!oResult->lookup(u"error"_ustr));

    // A newer request also invalidates a completion already queued on the UI.
    xLoader->refresh({ u"queued-old"_ustr });
    CPPUNIT_ASSERT(finishMetadataTasks());
    xLoader->refresh({});
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(2, nCallbacks);
    CPPUNIT_ASSERT_EQUAL(std::size_t(0), oResult->size());
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentMetadataCloseDuringRefresh)
{
    auto xGate = std::make_shared<MetadataReadGate>();
    int nCallbacks = 0;
    int nReads = 0;
    auto xLoader = std::make_unique<sfx2::RecentFileMetadataLoader>(
        [&](sfx2::RecentFileMetadataCache) { ++nCallbacks; },
        [&](const OUString&) -> sfx2::RecentFileMetadataCache::ModificationTime {
            ++nReads;
            xGate->block();
            return TimeValue{ 100, 0 };
        });
    comphelper::ScopeGuard aCleanup([&] {
        xLoader.reset();
        xGate->release();
        finishMetadataTasks();
        Scheduler::ProcessEventsToIdle();
    });
    xLoader->refresh({ u"slow"_ustr, u"cancelled-tail"_ustr });
    CPPUNIT_ASSERT(xGate->waitUntilEntered());
    xLoader->refresh({ u"cancelled-pending"_ustr });
    // Destruction must return before the test releases the blocked query.
    xLoader.reset();
    CPPUNIT_ASSERT(!xGate->timedOut());
    xGate->release();
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, nReads);
    CPPUNIT_ASSERT_EQUAL(0, nCallbacks);

    // Closing after the worker finishes must remove queued delivery as well.
    xLoader = std::make_unique<sfx2::RecentFileMetadataLoader>(
        [&](sfx2::RecentFileMetadataCache) { ++nCallbacks; },
        [](const OUString&) -> sfx2::RecentFileMetadataCache::ModificationTime {
            return TimeValue{ 100, 0 };
        });
    xLoader->refresh({ u"ready"_ustr });
    CPPUNIT_ASSERT(finishMetadataTasks());
    xLoader.reset();
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(0, nCallbacks);

    // UI delivery may itself destroy the loader (for example by opening a document).
    xLoader = std::make_unique<sfx2::RecentFileMetadataLoader>(
        [&](sfx2::RecentFileMetadataCache) {
            ++nCallbacks;
            xLoader.reset();
        },
        [](const OUString&) -> sfx2::RecentFileMetadataCache::ModificationTime {
            return TimeValue{ 100, 0 };
        });
    xLoader->refresh({ u"ready"_ustr });
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, nCallbacks);
    CPPUNIT_ASSERT(!xLoader);
}

namespace
{
oslProcess waitForMetadataChild(const OUString& rMarkerURL)
{
    const auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do
    {
        osl::File aMarker(rMarkerURL);
        if (aMarker.open(osl_File_OpenFlag_Read) == osl::FileBase::E_None)
        {
            oslProcessIdentifier nPID = 0;
            sal_uInt64 nRead = 0;
            if (aMarker.read(&nPID, sizeof(nPID), nRead) == osl::FileBase::E_None
                && nRead == sizeof(nPID) && nPID)
                return osl_getProcess(nPID);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < aDeadline);
    return nullptr;
}
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentMetadataProcessShutdown)
{
    OUString aHelper = m_directories.getURLFromWorkdir(u"/LinkTarget/Executable/sfx2_metadatastall");
#if defined(_WIN32)
    aHelper += ".exe";
#endif
    for (bool bCloseFirst : { false, true })
    {
        utl::TempFileNamed aMarker;
        aMarker.EnableKillingFile();
        int nCallbacks = 0;
        auto xLoader = std::make_unique<sfx2::RecentFileMetadataLoader>(
            [&](sfx2::RecentFileMetadataCache) { ++nCallbacks; },
            sfx2::RecentFileMetadataLoader::Loader(), aHelper);
        comphelper::ScopeGuard aCleanup([&] {
            xLoader.reset();
            sfx2::cancelRecentFileMetadataRequests();
            Scheduler::ProcessEventsToIdle();
        });
        xLoader->refresh({ aMarker.GetURL() });
        auto hChild = waitForMetadataChild(aMarker.GetURL());
        CPPUNIT_ASSERT(hChild);
        comphelper::ScopeGuard aChildCleanup([&] { osl_freeProcessHandle(hChild); });
        const TimeValue aZero(0, 0);
        CPPUNIT_ASSERT_EQUAL(osl_Process_E_TimedOut, osl_joinProcessWithTimeout(hChild, &aZero));

        const auto aStart = std::chrono::steady_clock::now();
        if (bCloseFirst)
            xLoader.reset();
        // This is the actual application shutdown order, with a child still blocked.
        sfx2::cancelRecentFileMetadataRequests();
        comphelper::ThreadPool::getSharedOptimalPool().shutdown();
        CPPUNIT_ASSERT(std::chrono::steady_clock::now() - aStart < std::chrono::seconds(1));
        const TimeValue aExitDeadline(1, 0);
        CPPUNIT_ASSERT_EQUAL(osl_Process_E_None,
                             osl_joinProcessWithTimeout(hChild, &aExitDeadline));
        if (xLoader)
            xLoader->refresh({ aMarker.GetURL() }); // shutdown-cancelled clients stay cancelled
        CPPUNIT_ASSERT(finishMetadataTasks());
        Scheduler::ProcessEventsToIdle();
        CPPUNIT_ASSERT_EQUAL(0, nCallbacks);
    }
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentMetadataProcessTimeout)
{
    OUString aHelper = m_directories.getURLFromWorkdir(u"/LinkTarget/Executable/sfx2_metadatastall");
#if defined(_WIN32)
    aHelper += ".exe";
#endif
    utl::TempFileNamed aMarker;
    aMarker.EnableKillingFile();
    std::optional<sfx2::RecentFileMetadataCache> oResult;
    sfx2::RecentFileMetadataLoader aLoader(
        [&](sfx2::RecentFileMetadataCache aMetadata) { oResult = std::move(aMetadata); }, {}, aHelper);
    comphelper::ScopeGuard aCleanup([&] { sfx2::cancelRecentFileMetadataRequests(); });
    aLoader.refresh({ aMarker.GetURL() });
    auto hChild = waitForMetadataChild(aMarker.GetURL());
    CPPUNIT_ASSERT(hChild);
    comphelper::ScopeGuard aChildCleanup([&] { osl_freeProcessHandle(hChild); });
    CPPUNIT_ASSERT(finishMetadataTasks());
    const TimeValue aExitDeadline(1, 0);
    CPPUNIT_ASSERT_EQUAL(osl_Process_E_None, osl_joinProcessWithTimeout(hChild, &aExitDeadline));
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(oResult);
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), oResult->size());
    CPPUNIT_ASSERT(!oResult->lookup(aMarker.GetURL()));
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testRecentMetadataProcessResults)
{
    const OUString aDocument = createFileURL(u"reload-page.odg");
    osl::DirectoryItem aItem;
    osl::FileStatus aStatus(osl_FileStatus_Mask_ModifyTime);
    CPPUNIT_ASSERT_EQUAL(osl::FileBase::E_None, osl::DirectoryItem::get(aDocument, aItem));
    CPPUNIT_ASSERT_EQUAL(osl::FileBase::E_None, aItem.getFileStatus(aStatus));
    std::vector<OUString> aURLs;
    // Cross a batch boundary and include duplicate, missing and non-file URLs.
    for (int i = 0; i < 33; ++i)
        aURLs.push_back(createFileURL(OUString(u"absent metadata " + OUString::number(i))));
    aURLs.push_back(aDocument);
    aURLs.push_back(aDocument);
    aURLs.push_back(u"https://example.invalid/document.odt"_ustr);
    std::optional<sfx2::RecentFileMetadataCache> oResult;
    sfx2::RecentFileMetadataLoader aLoader(
        [&](sfx2::RecentFileMetadataCache aMetadata) { oResult = std::move(aMetadata); });
    comphelper::ScopeGuard aCleanup([&] { sfx2::cancelRecentFileMetadataRequests(); });
    aLoader.refresh(aURLs);
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(oResult);
    CPPUNIT_ASSERT_EQUAL(std::size_t(35), oResult->size());
    CPPUNIT_ASSERT(oResult->lookup(aDocument));
    CPPUNIT_ASSERT_EQUAL(aStatus.getModifyTime().Seconds, oResult->lookup(aDocument)->Seconds);
    CPPUNIT_ASSERT_EQUAL(aStatus.getModifyTime().Nanosec, oResult->lookup(aDocument)->Nanosec);
    CPPUNIT_ASSERT(!oResult->lookup(aURLs.front()));
    CPPUNIT_ASSERT(!oResult->lookup(aURLs.back()));

    // Helper startup failure must yield unknown dates without doing parent-side stat.
    oResult.reset();
    sfx2::RecentFileMetadataLoader aMissingHelper(
        [&](sfx2::RecentFileMetadataCache aMetadata) { oResult = std::move(aMetadata); }, {},
        createFileURL(u"missing-metadata-helper"));
    aMissingHelper.refresh({ aDocument });
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(oResult);
    CPPUNIT_ASSERT(!oResult->lookup(aDocument));
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testStartCenterRecentList)
{
    const OUString aDocument = createFileURL(u"reload-page.odg");
    const OUString aSpreadsheet = createFileURL(u"missing-spreadsheet.ods");
    auto xDesktop = frame::Desktop::create(m_xContext);

    // Opening from the list must retain the history entry's read-only flag.
    for (bool bReadOnly : { false, true })
    {
        SvtHistoryOptions::Clear(EHistoryType::PickList);
        SvtHistoryOptions::AppendItem(EHistoryType::PickList, aDocument, u"draw8"_ustr,
                                     u"Drawing"_ustr, {}, bReadOnly);
        SvtHistoryOptions::AppendItem(EHistoryType::PickList, aSpreadsheet,
                                     u"calc8"_ustr, u"Spreadsheet"_ustr, {}, false);
        auto xFrame = xDesktop->findFrame(u"_blank"_ustr, 0);
        auto xStart = frame::StartModule::createWithParentWindow(m_xContext,
                                                               xFrame->getContainerWindow());
        uno::Reference<awt::XWindow> xWindow(xStart, uno::UNO_QUERY_THROW);
        xFrame->setComponent(xWindow, xStart);
        xStart->attachFrame(xFrame);
        xFrame->getContainerWindow()->setVisible(true);
        Scheduler::ProcessEventsToIdle();
        auto pWindow = VCLUnoHelper::GetWindow(xWindow);
        auto xUI = pWindow->GetUITestFactory()(pWindow);
        const auto aChildren = xUI->get_children();
        CPPUNIT_ASSERT(!aChildren.contains(u"all_recent"_ustr));
        CPPUNIT_ASSERT(!aChildren.contains(u"scrollrecent"_ustr));
        auto xList = xUI->get_child(u"dv_recent_list"_ustr);
        CPPUNIT_ASSERT_EQUAL(u"2"_ustr, xList->get_state()[u"Children"_ustr]);

        // History changes become visible at refresh, not during typing or layout.
        const OUString aNewDocument = createFileURL(u"new-document.odt");
        SvtHistoryOptions::AppendItem(EHistoryType::PickList, aNewDocument,
                                     u"writer8"_ustr, u"New document"_ustr, {}, false);
        xList->execute(u"SELECT"_ustr, { { u"POS"_ustr, u"0"_ustr } });
        const OUString aSelected = xList->get_state()[u"SelectedItemId"_ustr];
        const auto aSize = xFrame->getContainerWindow()->getPosSize();
        xFrame->getContainerWindow()->setPosSize(0, 0, aSize.Width + 120, aSize.Height,
                                                 awt::PosSize::SIZE);
        Scheduler::ProcessEventsToIdle();
        CPPUNIT_ASSERT_EQUAL(u"2"_ustr, xList->get_state()[u"Children"_ustr]);
        CPPUNIT_ASSERT_EQUAL(aSelected, xList->get_state()[u"SelectedItemId"_ustr]);

        auto xFilter = xUI->get_child(u"cbFilter"_ustr);
        xFilter->execute(u"SELECT"_ustr, { { u"POS"_ustr, u"2"_ustr } });
        CPPUNIT_ASSERT_EQUAL(u"1"_ustr, xList->get_state()[u"Children"_ustr]);
        xList->execute(u"SELECT"_ustr, { { u"POS"_ustr, u"0"_ustr } });
        CPPUNIT_ASSERT_EQUAL(aSpreadsheet, xList->get_state()[u"SelectedItemId"_ustr]);
        xFilter->execute(u"SELECT"_ustr, { { u"POS"_ustr, u"0"_ustr } });

        auto xSearch = xUI->get_child(u"dv_recent_search"_ustr);
        xSearch->execute(u"TYPE"_ustr, { { u"TEXT"_ustr, u"reload-page"_ustr } });
        CPPUNIT_ASSERT_EQUAL(u"1"_ustr, xList->get_state()[u"Children"_ustr]);
        xSearch->execute(u"CLEAR"_ustr, {});
        CPPUNIT_ASSERT_EQUAL(u"2"_ustr, xList->get_state()[u"Children"_ustr]);

        uno::Reference<frame::XDispatch> xRefresh(xStart, uno::UNO_QUERY_THROW);
        util::URL aRefresh;
        aRefresh.Path = "ClearRecentFileList";
        xRefresh->dispatch(aRefresh, {});
        CPPUNIT_ASSERT_EQUAL(u"3"_ustr, xList->get_state()[u"Children"_ustr]);

        // External history refresh must update the visible list and keep pinned entries.
        SvtHistoryOptions::TogglePinItem(EHistoryType::PickList, aDocument);
        SvtHistoryOptions::Clear(EHistoryType::PickList, false);
        xRefresh->dispatch(aRefresh, {});
        CPPUNIT_ASSERT_EQUAL(u"1"_ustr, xList->get_state()[u"Children"_ustr]);
        xList->get_child(u"0"_ustr)->execute(u"DOUBLECLICK"_ustr, {});
        xSearch.reset();
        xFilter.reset();
        xList.reset();
        xUI.reset();
        Scheduler::ProcessEventsToIdle();
        auto xController = xFrame->getController();
        CPPUNIT_ASSERT(xController.is());
        mxComponent.set(xController->getModel(), uno::UNO_QUERY_THROW);
        uno::Reference<frame::XStorable> xStorable(mxComponent, uno::UNO_QUERY_THROW);
        CPPUNIT_ASSERT_EQUAL(bReadOnly, bool(xStorable->isReadonly()));
        CPPUNIT_ASSERT_EQUAL(aDocument, xController->getModel()->getURL());
        mxComponent->dispose();
        mxComponent.clear();
    }
    SvtHistoryOptions::Clear(EHistoryType::PickList);
    CPPUNIT_ASSERT(finishMetadataTasks());
    Scheduler::ProcessEventsToIdle();
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testReloadPage)
{
    // Load a document, which has 2 pages.
    loadFromFile(u"reload-page.odg");

    // Reload, and request to start on page 2.
    SfxViewFrame* pFrame = SfxViewFrame::Current();
    SfxAllItemSet aSet(SfxGetpApp()->GetPool());
    aSet.Put(SfxInt32Item(SID_PAGE_NUMBER, 1));
    SfxRequest aReq(SID_RELOAD, SfxCallMode::SLOT, aSet);
    pFrame->ExecReload_Impl(aReq);
    SfxObjectShell* pDocSh = SfxObjectShell::Current();
    CPPUNIT_ASSERT(pDocSh);
    uno::Reference<frame::XModel> xModel = pDocSh->GetBaseModel();
    mxComponent = xModel;

    // Check the current page after reload.
    uno::Reference<drawing::XDrawView> xController(xModel->getCurrentController(), uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xPage(xController->getCurrentPage(), uno::UNO_QUERY);
    sal_Int32 nPage{};
    xPage->getPropertyValue(u"Number"_ustr) >>= nPage;

    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 2
    // - Actual  : 1
    // i.e. the document was opened on page 1, not page 2, SID_PAGE_NUMBER was ignored.
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(2), nPage);
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testLokHelperExtractCertificates)
{
    std::string signatureCa = R"(-----BEGIN CERTIFICATE-----
foo
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
bar
-----END CERTIFICATE-----)";

    std::vector<std::string> aRet = SfxLokHelper::extractCertificates(signatureCa);

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aRet.size());
    CPPUNIT_ASSERT_EQUAL(std::string("\nfoo\n"), aRet[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("\nbar\n"), aRet[1]);
}

#ifdef UNX
CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testLokHelperAddCertifices)
{
    // Given a loaded and signed document, CA is not trusted by default:
    loadFromFile(u"signed.odt");
    auto pBaseModel = dynamic_cast<SfxBaseModel*>(mxComponent.get());
    SfxObjectShell* pObjectShell = pBaseModel->GetObjectShell();
    CPPUNIT_ASSERT_EQUAL(SignatureState::NOTVALIDATED, pObjectShell->GetDocumentSignatureState());

    // When trusting the CA:
    OUString aCaUrl = createFileURL(u"ca.pem");
    SvFileStream aCaStream(aCaUrl, StreamMode::READ);
    std::string aCa;
    aCa = read_uInt8s_ToOString(aCaStream, aCaStream.remainingSize());
    std::vector<std::string> aCerts = SfxLokHelper::extractCertificates(aCa);
    SfxLokHelper::addCertificates(aCerts);

    // Then make sure the signature state is updated:
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 1 (OK)
    // - Actual  : 4 (SignatureState::NOTVALIDATED)
    // i.e. the signature status for an opened document was not updated when trusting a CA.
    CPPUNIT_ASSERT_EQUAL(SignatureState::OK, pObjectShell->GetDocumentSignatureState());
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testLokHelperCommandValuesSignature)
{
#if ENABLE_PDFIMPORT
    // Given an unsigned PDF file:
    createTempCopy(u"unsigned.pdf");
    loadFromURL(maTempFile.GetURL());

    // When extracting hashes:
    tools::JsonWriter aWriter;
    SfxLokHelper::getCommandValues(aWriter, ".uno:Signature");
    OString aJson = aWriter.finishAndGetAsOString();

    // Then make sure that we get a signature time and a hash:
    CPPUNIT_ASSERT(SfxLokHelper::supportsCommand(u"Signature"));
    std::stringstream aStream{ std::string(aJson) };
    boost::property_tree::ptree aTree;
    boost::property_tree::read_json(aStream, aTree);
    auto it = aTree.find("commandName");
    CPPUNIT_ASSERT(it != aTree.not_found());
    CPPUNIT_ASSERT_EQUAL(std::string(".uno:Signature"), it->second.get_value<std::string>());
    it = aTree.find("commandValues");
    CPPUNIT_ASSERT(it != aTree.not_found());
    aTree = it->second;
    // Non-zero timestamp:
    it = aTree.find("signatureTime");
    CPPUNIT_ASSERT(it != aTree.not_found());
    auto nSignatureTime = it->second.get_value<sal_Int64>();
    CPPUNIT_ASSERT(nSignatureTime != 0);
    // Base64 encoded hash, that has the SHA-256 length:
    it = aTree.find("digest");
    CPPUNIT_ASSERT(it != aTree.not_found());
    auto aDigest = OUString::fromUtf8(it->second.get_value<std::string>());
    uno::Sequence<sal_Int8> aBytes;
    comphelper::Base64::decode(aBytes, aDigest);
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(32), aBytes.getLength());
#endif
}

namespace
{
#if ENABLE_PDFIMPORT
OUString GetSignatureHash()
{
    tools::JsonWriter aWriter;
    // Provide the current time, so the system timer is not contacted:
    SfxLokHelper::getCommandValues(aWriter, ".uno:Signature?signatureTime=1731329053152");
    OString aJson = aWriter.finishAndGetAsOString();
    std::stringstream aStream{ std::string(aJson) };
    boost::property_tree::ptree aTree;
    boost::property_tree::read_json(aStream, aTree);
    auto it = aTree.find("commandValues");
    CPPUNIT_ASSERT(it != aTree.not_found());
    aTree = it->second;
    it = aTree.find("digest");
    CPPUNIT_ASSERT(it != aTree.not_found());
    return OUString::fromUtf8(it->second.get_value<std::string>());
}
#endif
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testLokHelperCommandValuesSignatureHash)
{
#if ENABLE_PDFIMPORT
    // Given an unsigned PDF file:
    loadFromFile(u"unsigned.pdf");

    // When extracting hashes, two times:
    OUString aHash1 = GetSignatureHash();
    OUString aHash2 = GetSignatureHash();

    // Then make sure that we get the same hash, since the same system time is provided:
    // In case the test was slow enough that there was 1ms system time difference between the two
    // calls, then this failed.
    CPPUNIT_ASSERT_EQUAL(aHash1, aHash2);
#endif
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testSignatureSerialize)
{
    // Given an unsigned PDF file:
    std::shared_ptr<vcl::pdf::PDFium> pPDFium = vcl::pdf::PDFiumLibrary::get();
    if (!pPDFium)
        return;
    createTempCopy(u"unsigned.pdf");
    loadFromURL(maTempFile.GetURL());

    // When signing by serializing an externally provided signature based on an earlier extracted
    // timestamp & document hash:
    OUString aSigUrl = createFileURL(u"signature.pkcs7");
    SvFileStream aSigStream(aSigUrl, StreamMode::READ);
    auto aSigValue
        = OUString::fromUtf8(read_uInt8s_ToOString(aSigStream, aSigStream.remainingSize()));
    uno::Sequence<beans::PropertyValue> aArgs = {
        comphelper::makePropertyValue(u"SignatureTime"_ustr, u"1643201995722"_ustr),
        comphelper::makePropertyValue(u"SignatureValue"_ustr, aSigValue),
    };
    dispatchCommand(mxComponent, u".uno:Signature"_ustr, aArgs);

    // Then make sure the document has a signature:
    SvMemoryStream aStream;
    aStream.WriteStream(*maTempFile.GetStream(StreamMode::READ));
    std::unique_ptr<vcl::pdf::PDFiumDocument> pPdfDocument
        = pPDFium->openDocument(aStream.GetData(), aStream.GetSize(), OString());
    CPPUNIT_ASSERT(pPdfDocument);
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 1
    // - Actual  : 0
    // i.e. no signature was added, since we tried to sign interactively instead of based on
    // provided parameters.
    CPPUNIT_ASSERT_EQUAL(1, pPdfDocument->getSignatureCount());
}

CPPUNIT_TEST_FIXTURE(Sfx2ViewTest, testScheduler)
{
    // Given an empty document:
    mxComponent = loadFromDesktop("private:factory/swriter", "com.sun.star.text.TextDocument");

    // When asking for the state of the scheduler:
    int nRet = Scheduler::GetMostUrgentTaskPriority();

    // Then make sure we get a priority:
    // This returns TaskPriority::HIGH_IDLE, but just make sure we get a positive priority.
    CPPUNIT_ASSERT(nRet != -1);
}
#endif

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
