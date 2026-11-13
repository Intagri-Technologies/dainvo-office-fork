/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dainvo Office updates. See include/sfx2/dainvoupdate.hxx for the overview
// and dainvo/docs/RELEASES.md for how releases and the appcast are made.

#include <sal/config.h>

#include <config_version.h>

#include <sfx2/dainvoupdate.hxx>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstring>
#include <memory>

#ifdef ENABLE_DAINVO_UPDATER

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <com/sun/star/frame/Desktop.hpp>
#include <comphelper/configuration.hxx>
#include <comphelper/processfactory.hxx>
#include <officecfg/Office/Common.hxx>
#include <o3tl/test_info.hxx>
#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <sfx2/infobar.hxx>
#include <sfx2/sfxresid.hxx>
#include <sfx2/strings.hrc>
#include <sfx2/viewfrm.hxx>
#include <tools/link.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/MessageDialog.hxx>
#include <vcl/weld/WaitObject.hxx>

#include <openuriexternally.hxx>

#include <systools/curlinit.hxx>

#ifdef _WIN32
#include <o3tl/char16_t2wchar_t.hxx>
#include <prewin.h>
#include <postwin.h>
#endif

#endif // ENABLE_DAINVO_UPDATER

namespace
{
constexpr char SPARKLE_NS[] = "http://www.andymatuschak.org/xml-namespaces/sparkle";

bool isSparkle(const xmlNode* pNode, const char* pName)
{
    return pNode->type == XML_ELEMENT_NODE && pNode->ns && pNode->ns->href
           && std::strcmp(reinterpret_cast<const char*>(pNode->ns->href), SPARKLE_NS) == 0
           && std::strcmp(reinterpret_cast<const char*>(pNode->name), pName) == 0;
}

bool isPlain(const xmlNode* pNode, const char* pName)
{
    return pNode->type == XML_ELEMENT_NODE && !pNode->ns
           && std::strcmp(reinterpret_cast<const char*>(pNode->name), pName) == 0;
}

OUString nodeText(const xmlNode* pNode)
{
    xmlChar* pText = xmlNodeGetContent(pNode);
    if (!pText)
        return {};
    OUString aText = OUString::fromUtf8(reinterpret_cast<const char*>(pText)).trim();
    xmlFree(pText);
    return aText;
}

OUString attribute(const xmlNode* pNode, const char* pName, bool bSparkle)
{
    xmlChar* pValue
        = bSparkle ? xmlGetNsProp(pNode, reinterpret_cast<const xmlChar*>(pName),
                                  reinterpret_cast<const xmlChar*>(SPARKLE_NS))
                   : xmlGetNoNsProp(pNode, reinterpret_cast<const xmlChar*>(pName));
    if (!pValue)
        return {};
    OUString aValue = OUString::fromUtf8(reinterpret_cast<const char*>(pValue)).trim();
    xmlFree(pValue);
    return aValue;
}
}

namespace sfx2::DainvoUpdater
{
std::optional<Release> ParseAppcast(std::string_view aXml, std::u16string_view aOS)
{
    std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> pDoc(
        xmlReadMemory(aXml.data(), static_cast<int>(aXml.size()), "appcast.xml", nullptr,
                      XML_PARSE_NONET),
        xmlFreeDoc);
    if (!pDoc)
        return {};
    xmlNode* pRss = xmlDocGetRootElement(pDoc.get());
    if (!pRss || !isPlain(pRss, "rss"))
        return {};

    std::optional<Release> oBest;
    for (xmlNode* pChannel = pRss->children; pChannel; pChannel = pChannel->next)
    {
        if (!isPlain(pChannel, "channel"))
            continue;
        for (xmlNode* pItem = pChannel->children; pItem; pItem = pItem->next)
        {
            if (!isPlain(pItem, "item"))
                continue;

            Release aRelease;
            OUString aItemOS;
            bool bHasEnclosure = false;
            for (xmlNode* pChild = pItem->children; pChild; pChild = pChild->next)
            {
                if (isSparkle(pChild, "version"))
                    aRelease.nBuild = nodeText(pChild).toInt64();
                else if (isSparkle(pChild, "shortVersionString"))
                    aRelease.aVersion = nodeText(pChild);
                else if (isPlain(pChild, "link"))
                    aRelease.aPageURL = nodeText(pChild);
                else if (isPlain(pChild, "enclosure"))
                {
                    bHasEnclosure = true;
                    aItemOS = attribute(pChild, "os", true);
                    aRelease.aDownloadURL = attribute(pChild, "url", false);
                    // Older appcasts put the versions on the enclosure.
                    if (!aRelease.nBuild)
                        aRelease.nBuild = attribute(pChild, "version", true).toInt64();
                    if (aRelease.aVersion.isEmpty())
                        aRelease.aVersion = attribute(pChild, "shortVersionString", true);
                }
            }

            // Sparkle treats an enclosure without sparkle:os as macOS.
            if (bHasEnclosure && aItemOS.isEmpty())
                aItemOS = u"macos"_ustr;
            // "windows-x64" etc. count as "windows".
            if (bHasEnclosure && !aItemOS.startsWith(aOS))
                continue;
            if (aRelease.nBuild <= 0)
                continue;
            if (aRelease.aVersion.isEmpty())
                aRelease.aVersion = OUString::number(aRelease.nBuild);
            if (!oBest || aRelease.nBuild > oBest->nBuild)
                oBest = std::move(aRelease);
        }
    }
    return oBest;
}
}

#ifndef ENABLE_DAINVO_UPDATER

namespace sfx2::DainvoUpdater
{
bool IsAvailable() { return false; }
void Start() {}
void Stop() {}
void CheckNow(weld::Window*) {}
void ShowPendingNotice(SfxViewFrame&) {}
}

#else

#ifdef MACOSX
// dainvoupdate_mac.mm
bool DainvoSparkleStart();
bool DainvoSparkleCheckNow();
#endif

namespace
{
constexpr OUString CURRENT_VERSION = u"" DAINVO_VERSION ""_ustr;
constexpr sal_Int64 CURRENT_BUILD = DAINVO_BUILD;
constexpr sal_Int64 CHECK_INTERVAL_SECONDS = 24 * 60 * 60;

#if defined _WIN32
constexpr std::u16string_view THIS_OS = u"windows";
#elif defined MACOSX
constexpr std::u16string_view THIS_OS = u"macos";
#else
constexpr std::u16string_view THIS_OS = u"linux";
#endif

enum class Backend
{
    None,
    BuiltIn,
    Sparkle,
    WinSparkle
};

Backend g_eBackend = Backend::None;
std::atomic<bool> g_bAbort(false);
std::thread g_aThread;
std::mutex g_aMutex; // guards g_oPending and g_bNoticeShown
std::optional<sfx2::DainvoUpdater::Release> g_oPending;
bool g_bNoticeShown = false;

OUString feedURL()
{
    // For testing a release before it is published, e.g.
    // DAINVO_UPDATE_FEED_OVERRIDE=https://github.com/.../releases/download/v0.1.1/appcast.xml
    // Sparkle uses `defaults write com.dainvo.office SUFeedURL <url>` instead.
    if (const char* pOverride = getenv("DAINVO_UPDATE_FEED_OVERRIDE"); pOverride && *pOverride)
        return OUString::fromUtf8(pOverride);
    return u"" DAINVO_UPDATE_FEED ""_ustr;
}

size_t writeCallback(char* pData, size_t nSize, size_t nCount, void* pUser)
{
    auto* pBuffer = static_cast<std::string*>(pUser);
    const size_t nBytes = nSize * nCount;
    if (pBuffer->size() + nBytes > 1024 * 1024) // an appcast is a few KB
        return 0;
    pBuffer->append(pData, nBytes);
    return nBytes;
}

int progressCallback(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return g_bAbort ? 1 : 0;
}

/// Download the appcast. Runs on any thread; no UI.
std::optional<std::string> fetchFeed()
{
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> pCurl(curl_easy_init(),
                                                               curl_easy_cleanup);
    if (!pCurl)
        return {};
    ::InitCurl_easy(pCurl.get());

    const OString aURL = feedURL().toUtf8();
    static constexpr char aUserAgent[]
        = "DainvoOffice/" DAINVO_VERSION " (build " LIBO_VERSION_STRINGIFY(DAINVO_BUILD) ")";
    std::string aBody;
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_URL, aURL.getStr());
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_USERAGENT, aUserAgent);
    // GitHub's releases/latest/download/... redirects to the asset host.
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_MAXREDIRS, 5L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_FAILONERROR, 1L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_TIMEOUT, 20L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEDATA, &aBody);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_NOPROGRESS, 0L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);

    const CURLcode eResult = curl_easy_perform(pCurl.get());
    if (eResult != CURLE_OK)
    {
        SAL_INFO("sfx.appl", "Dainvo update check failed: " << curl_easy_strerror(eResult));
        return {};
    }
    return aBody;
}

OUString availableText(const sfx2::DainvoUpdater::Release& rRelease)
{
    return SfxResId(STR_DAINVO_UPDATE_AVAILABLE)
        .replaceAll("%VERSION", rRelease.aVersion)
        .replaceAll("%CURRENT", CURRENT_VERSION);
}

OUString releasePage(const sfx2::DainvoUpdater::Release& rRelease)
{
    return rRelease.aPageURL.isEmpty() ? rRelease.aDownloadURL : rRelease.aPageURL;
}

void rememberCheck()
{
    if (officecfg::Office::Common::DainvoUpdate::LastCheck::isReadOnly())
        return;
    const sal_Int64 nNow = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    auto xBatch(comphelper::ConfigurationChanges::create());
    officecfg::Office::Common::DainvoUpdate::LastCheck::set(nNow, xBatch);
    xBatch->commit();
}

class Notice
{
public:
    DECL_STATIC_LINK(Notice, FoundHdl, void*, void);
    DECL_STATIC_LINK(Notice, NothingFoundHdl, void*, void);
#ifdef _WIN32
    DECL_STATIC_LINK(Notice, QuitHdl, void*, void);
#endif
    DECL_STATIC_LINK(Notice, DownloadHdl, weld::Button&, void);
    DECL_STATIC_LINK(Notice, SkipHdl, weld::Button&, void);
};

IMPL_STATIC_LINK_NOARG(Notice, FoundHdl, void*, void)
{
    rememberCheck();
    if (SfxViewFrame* pFrame = SfxViewFrame::Current())
        sfx2::DainvoUpdater::ShowPendingNotice(*pFrame);
    // Otherwise (only the Start Center is open) it shows with the next document.
}

IMPL_STATIC_LINK_NOARG(Notice, NothingFoundHdl, void*, void) { rememberCheck(); }

#ifdef _WIN32
IMPL_STATIC_LINK_NOARG(Notice, QuitHdl, void*, void)
{
    css::frame::Desktop::create(comphelper::getProcessComponentContext())->terminate();
}
#endif

IMPL_STATIC_LINK_NOARG(Notice, DownloadHdl, weld::Button&, void)
{
    OUString aURL;
    {
        std::scoped_lock aGuard(g_aMutex);
        if (g_oPending)
            aURL = releasePage(*g_oPending);
    }
    if (!aURL.isEmpty())
        sfx2::openUriExternally(aURL, false, nullptr);
    if (SfxViewFrame* pFrame = SfxViewFrame::Current())
        pFrame->RemoveInfoBar(u"dainvoupdate");
}

IMPL_STATIC_LINK_NOARG(Notice, SkipHdl, weld::Button&, void)
{
    sal_Int64 nBuild = 0;
    {
        std::scoped_lock aGuard(g_aMutex);
        if (g_oPending)
            nBuild = g_oPending->nBuild;
    }
    if (nBuild && !officecfg::Office::Common::DainvoUpdate::SkippedBuild::isReadOnly())
    {
        auto xBatch(comphelper::ConfigurationChanges::create());
        officecfg::Office::Common::DainvoUpdate::SkippedBuild::set(nBuild, xBatch);
        xBatch->commit();
    }
    if (SfxViewFrame* pFrame = SfxViewFrame::Current())
        pFrame->RemoveInfoBar(u"dainvoupdate");
}

void automaticCheckThread()
{
    std::optional<std::string> oFeed = fetchFeed();
    if (!oFeed || g_bAbort)
        return;
    std::optional<sfx2::DainvoUpdater::Release> oRelease
        = sfx2::DainvoUpdater::ParseAppcast(*oFeed, THIS_OS);
    if (!oRelease || oRelease->nBuild <= CURRENT_BUILD)
    {
        Application::PostUserEvent(LINK(nullptr, Notice, NothingFoundHdl));
        return;
    }
    {
        std::scoped_lock aGuard(g_aMutex);
        g_oPending = std::move(oRelease);
    }
    Application::PostUserEvent(LINK(nullptr, Notice, FoundHdl));
}

void startBuiltIn()
{
    g_eBackend = Backend::BuiltIn;
    if (!officecfg::Office::Common::DainvoUpdate::AutoCheck::get())
        return;
    const sal_Int64 nNow = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    const sal_Int64 nLast = officecfg::Office::Common::DainvoUpdate::LastCheck::get();
    if (nLast > 0 && nNow - nLast < CHECK_INTERVAL_SECONDS && nLast <= nNow)
        return;
    g_aThread = std::thread(automaticCheckThread);
}

void checkNowBuiltIn(weld::Window* pParent)
{
    std::optional<std::string> oFeed;
    {
        weld::WaitObject aWait(pParent);
        oFeed = fetchFeed();
    }
    std::optional<sfx2::DainvoUpdater::Release> oRelease;
    if (oFeed)
        oRelease = sfx2::DainvoUpdater::ParseAppcast(*oFeed, THIS_OS);

    if (!oFeed || !oRelease)
    {
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(
            pParent, VclMessageType::Warning, VclButtonsType::Ok,
            SfxResId(STR_DAINVO_UPDATE_FAILED)));
        xBox->run();
        return;
    }
    rememberCheck();
    if (oRelease->nBuild <= CURRENT_BUILD)
    {
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(
            pParent, VclMessageType::Info, VclButtonsType::Ok,
            SfxResId(STR_DAINVO_UPDATE_NONE).replaceAll("%CURRENT", CURRENT_VERSION)));
        xBox->run();
        return;
    }

    std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(
        pParent, VclMessageType::Question, VclButtonsType::Cancel, availableText(*oRelease)));
    xBox->add_button(SfxResId(STR_DAINVO_UPDATE_DOWNLOAD), RET_YES);
    xBox->set_default_response(RET_YES);
    if (xBox->run() == RET_YES)
        sfx2::openUriExternally(releasePage(*oRelease), false, pParent);
}

#ifdef _WIN32
// WinSparkle is loaded at run time, so builds without it still start.
struct WinSparkleApi
{
    HMODULE hModule = nullptr;
    void(__cdecl* init)() = nullptr;
    void(__cdecl* cleanup)() = nullptr;
    void(__cdecl* check_update_with_ui)() = nullptr;
};
WinSparkleApi g_aWinSparkle;

int __cdecl winSparkleCanShutdown() { return 1; }

void __cdecl winSparkleShutdownRequest()
{
    // Called on a WinSparkle thread when the installer is about to run.
    Application::PostUserEvent(LINK(nullptr, Notice, QuitHdl));
}

bool startWinSparkle()
{
    // WinSparkle.dll sits next to soffice.exe and this library.
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&startWinSparkle), &hSelf);
    wchar_t aPath[MAX_PATH];
    DWORD nLen = GetModuleFileNameW(hSelf, aPath, MAX_PATH);
    if (!nLen || nLen >= MAX_PATH)
        return false;
    std::wstring aDll(aPath, nLen);
    aDll = aDll.substr(0, aDll.find_last_of(L'\\') + 1) + L"WinSparkle.dll";
    HMODULE h = LoadLibraryExW(aDll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h)
        return false;

    auto get = [h](const char* pName) { return GetProcAddress(h, pName); };
    auto set_appcast_url = reinterpret_cast<void(__cdecl*)(const char*)>(get("win_sparkle_set_appcast_url"));
    auto set_eddsa_public_key = reinterpret_cast<int(__cdecl*)(const char*)>(get("win_sparkle_set_eddsa_public_key"));
    auto set_app_details = reinterpret_cast<void(__cdecl*)(const wchar_t*, const wchar_t*, const wchar_t*)>(get("win_sparkle_set_app_details"));
    auto set_app_build_version = reinterpret_cast<void(__cdecl*)(const wchar_t*)>(get("win_sparkle_set_app_build_version"));
    auto set_can_shutdown_callback = reinterpret_cast<void(__cdecl*)(int(__cdecl*)())>(get("win_sparkle_set_can_shutdown_callback"));
    auto set_shutdown_request_callback = reinterpret_cast<void(__cdecl*)(void(__cdecl*)())>(get("win_sparkle_set_shutdown_request_callback"));
    g_aWinSparkle.init = reinterpret_cast<void(__cdecl*)()>(get("win_sparkle_init"));
    g_aWinSparkle.cleanup = reinterpret_cast<void(__cdecl*)()>(get("win_sparkle_cleanup"));
    g_aWinSparkle.check_update_with_ui = reinterpret_cast<void(__cdecl*)()>(get("win_sparkle_check_update_with_ui"));
    if (!set_appcast_url || !set_eddsa_public_key || !set_app_details || !set_app_build_version
        || !set_can_shutdown_callback || !set_shutdown_request_callback || !g_aWinSparkle.init
        || !g_aWinSparkle.cleanup || !g_aWinSparkle.check_update_with_ui)
    {
        FreeLibrary(h);
        g_aWinSparkle = {};
        return false;
    }
    g_aWinSparkle.hModule = h;

    set_appcast_url(feedURL().toUtf8().getStr());
    if (!set_eddsa_public_key(DAINVO_UPDATE_KEY))
        SAL_WARN("sfx.appl", "WinSparkle rejected the update public key");
    set_app_details(L"Dainvo", L"Dainvo Office", L"" DAINVO_VERSION);
    set_app_build_version(o3tl::toW(OUString::number(CURRENT_BUILD).getStr()));
    set_can_shutdown_callback(winSparkleCanShutdown);
    set_shutdown_request_callback(winSparkleShutdownRequest);
    g_aWinSparkle.init(); // asks the user about automatic checks on the 2nd run
    return true;
}
#endif
}

namespace sfx2::DainvoUpdater
{
bool IsAvailable() { return true; }

void Start()
{
    if (g_eBackend != Backend::None || Application::IsHeadlessModeEnabled()
        || o3tl::IsRunningUnitTest() || o3tl::IsRunningUITest() || comphelper::IsFuzzing())
        return;
#if defined MACOSX
    if (DainvoSparkleStart())
    {
        g_eBackend = Backend::Sparkle;
        return;
    }
    SAL_WARN("sfx.appl", "Sparkle.framework not found, using the built-in update check");
#elif defined _WIN32
    if (startWinSparkle())
    {
        g_eBackend = Backend::WinSparkle;
        return;
    }
    SAL_WARN("sfx.appl", "WinSparkle.dll not found, using the built-in update check");
#endif
    startBuiltIn();
}

void Stop()
{
    g_bAbort = true;
    if (g_aThread.joinable())
        g_aThread.join();
#ifdef _WIN32
    if (g_eBackend == Backend::WinSparkle)
        g_aWinSparkle.cleanup();
#endif
}

void CheckNow(weld::Window* pParent)
{
    if (g_eBackend == Backend::None)
        Start();
#ifdef MACOSX
    if (g_eBackend == Backend::Sparkle && DainvoSparkleCheckNow())
        return;
#elif defined _WIN32
    if (g_eBackend == Backend::WinSparkle)
    {
        g_aWinSparkle.check_update_with_ui();
        return;
    }
#endif
    checkNowBuiltIn(pParent);
}

void ShowPendingNotice(SfxViewFrame& rFrame)
{
    std::optional<Release> oRelease;
    {
        std::scoped_lock aGuard(g_aMutex);
        if (!g_oPending || g_bNoticeShown)
            return;
        if (g_oPending->nBuild == officecfg::Office::Common::DainvoUpdate::SkippedBuild::get())
            return;
        g_bNoticeShown = true;
        oRelease = g_oPending;
    }
    VclPtr<SfxInfoBarWindow> pInfoBar = rFrame.AppendInfoBar(
        u"dainvoupdate"_ustr, u""_ustr, availableText(*oRelease), InfobarType::INFO);
    if (!pInfoBar)
        return;
    weld::Button& rDownload = pInfoBar->addButton();
    rDownload.set_label(SfxResId(STR_DAINVO_UPDATE_DOWNLOAD));
    rDownload.connect_clicked(LINK(nullptr, Notice, DownloadHdl));
    weld::Button& rSkip = pInfoBar->addButton();
    rSkip.set_label(SfxResId(STR_DAINVO_UPDATE_SKIP));
    rSkip.connect_clicked(LINK(nullptr, Notice, SkipHdl));
}
}

#endif // ENABLE_DAINVO_UPDATER

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
