/* This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "dainvofonts.hxx"

#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <comphelper/diagnose_ex.hxx>
#include <comphelper/hash.hxx>
#include <comphelper/processfactory.hxx>
#include <config_folders.h>
#include <dialmgr.hxx>
#include <strings.hrc>
#include <osl/file.hxx>
#include <osl/process.h>
#include <osl/security.hxx>
#include <rtl/bootstrap.hxx>
#include <sal/log.hxx>
#include <sfx2/dainvoformats.hxx>
#include <sfx2/filedlghelper.hxx>
#include <tools/stream.hxx>
#include <tools/urlobj.hxx>
#include <unotools/tempfile.hxx>
#include <vcl/event.hxx>
#include <vcl/outdev.hxx>
#include <vcl/svapp.hxx>
#include <vcl/timer.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/CheckButton.hxx>
#include <vcl/weld/Dialog.hxx>
#include <vcl/weld/DialogController.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/LinkButton.hxx>
#include <vcl/weld/TreeView.hxx>
#include <vcl/weld/Window.hxx>
#include <vcl/vclenum.hxx>

#include <curl/curl.h>
#include <hb.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#if defined(MACOSX)
#include <premac.h>
#include <CoreText/CoreText.h>
#include <postmac.h>
#elif defined(_WIN32)
#include <o3tl/char16_t2wchar_t.hxx>
#include <windows.h>
#elif defined(LINUX)
#include <fontconfig/fontconfig.h>
#endif

namespace
{
struct Source
{
    const char* pURL;
    const char* pSha256;
    size_t nSize;
    std::u16string_view aPrefix;
    size_t nFonts;
};

// The verified vendor releases keep the license and font files in sync.
constexpr Source AptosSource{
    "https://download.microsoft.com/download/8/6/0/860a94fa-7feb-44ef-ac79-c072d9113d69/"
    "Microsoft%20Aptos%20Fonts.zip",
    "6528fd120e719a9f985e94214eca6887d1653b88456916a792a630b02e95b025", 2979784, u"Aptos", 28
};
constexpr Source SelawikSource{
    "https://github.com/microsoft/Selawik/releases/download/1.01/Selawik_Release.zip",
    "3f62c51e05e3b5a1e6241cf92a371f0be2ea1183aa87b30718bbd40832a8d423", 530652, u"selawk", 5
};

struct FontRow
{
    OUString aName;
    OUString aAlternative;
    OUString aSource;
    const Source* pDownload = nullptr;
    std::array<const char*, 4> aAppleFaces{};
    bool bLinuxCoreFont = false;
    std::u16string_view aBundledPrefix{};
    OUString aLicense{};
};

const FontRow Rows[] = {
    { u"Aptos"_ustr, u"Intos"_ustr, u"https://www.microsoft.com/en-us/download/details.aspx?id=106087"_ustr,
      &AptosSource, {}, false, {}, OUString::createFromAscii(AptosSource.pURL) },
    { u"Arial"_ustr, u"Liberation Sans"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/arial"_ustr,
      nullptr, { "ArialMT", "Arial-BoldMT", "Arial-ItalicMT", "Arial-BoldItalicMT" }, true },
    { u"Arial Narrow"_ustr, u"Liberation Sans Narrow"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/arial-narrow"_ustr,
      nullptr, { "ArialNarrow", "ArialNarrow-Bold", "ArialNarrow-Italic", "ArialNarrow-BoldItalic" } },
    { u"Calibri"_ustr, u"Carlito"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/calibri"_ustr },
    { u"Cambria"_ustr, u"Caladea"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/cambria"_ustr },
    { u"Courier New"_ustr, u"Liberation Mono"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/courier-new"_ustr,
      nullptr, { "CourierNewPSMT", "CourierNewPS-BoldMT", "CourierNewPS-ItalicMT", "CourierNewPS-BoldItalicMT" }, true },
    { u"Georgia"_ustr, u"Gelasio"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/georgia"_ustr,
      nullptr, { "Georgia", "Georgia-Bold", "Georgia-Italic", "Georgia-BoldItalic" }, true },
    { u"Segoe UI"_ustr, u"Selawik"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/segoe-ui"_ustr },
    { u"Times New Roman"_ustr, u"Liberation Serif"_ustr, u"https://learn.microsoft.com/en-us/typography/font-list/times-new-roman"_ustr,
      nullptr, { "TimesNewRomanPSMT", "TimesNewRomanPS-BoldMT", "TimesNewRomanPS-ItalicMT", "TimesNewRomanPS-BoldItalicMT" }, true },
    { u"Selawik"_ustr, u"Segoe UI"_ustr, u"https://github.com/microsoft/Selawik/releases/tag/1.01"_ustr,
      &SelawikSource, {}, false, {}, u"https://github.com/microsoft/Selawik/blob/89362e84731d1f5777fa078fd4d5ebbd339e4378/LICENSE.txt"_ustr },
    { u"Carlito"_ustr, u"Calibri"_ustr, u"https://github.com/googlefonts/carlito"_ustr,
      nullptr, {}, false, u"Carlito-", u"https://github.com/googlefonts/carlito/blob/main/OFL.txt"_ustr },
    { u"Caladea"_ustr, u"Cambria"_ustr, u"https://github.com/huertatipografica/Caladea"_ustr,
      nullptr, {}, false, u"Caladea-", u"https://www.apache.org/licenses/LICENSE-2.0"_ustr },
    { u"Liberation Sans"_ustr, u"Arial"_ustr, u"https://github.com/liberationfonts/liberation-fonts"_ustr,
      nullptr, {}, false, u"LiberationSans-", u"https://github.com/liberationfonts/liberation-fonts/blob/2.1.5/LICENSE"_ustr },
    { u"Liberation Sans Narrow"_ustr, u"Arial Narrow"_ustr, u"https://github.com/liberationfonts/liberation-sans-narrow"_ustr,
      nullptr, {}, false, u"LiberationSansNarrow-", u"https://github.com/liberationfonts/liberation-sans-narrow/blob/master/License.txt"_ustr },
    { u"Liberation Serif"_ustr, u"Times New Roman"_ustr, u"https://github.com/liberationfonts/liberation-fonts"_ustr,
      nullptr, {}, false, u"LiberationSerif-", u"https://github.com/liberationfonts/liberation-fonts/blob/2.1.5/LICENSE"_ustr },
    { u"Liberation Mono"_ustr, u"Courier New"_ustr, u"https://github.com/liberationfonts/liberation-fonts"_ustr,
      nullptr, {}, false, u"LiberationMono-", u"https://github.com/liberationfonts/liberation-fonts/blob/2.1.5/LICENSE"_ustr },
    { u"Gelasio"_ustr, u"Georgia"_ustr, u"https://github.com/SorkinType/Gelasio"_ustr,
      nullptr, {}, false, u"Gelasio-", u"https://github.com/SorkinType/Gelasio/blob/7ab20e7e5c42791e603b9ee3201a0b49849cfdb2/OFL.txt"_ustr },
    { u"Intos"_ustr, u"Aptos"_ustr, u"https://github.com/muglug/intos"_ustr,
      nullptr, {}, false, u"Intos-", u"https://github.com/muglug/intos/blob/fef9315c14da9e4b23b4c3cac8e718998d4e4736/LICENSE.txt"_ustr },
    { u"Intos Display"_ustr, u"Aptos Display"_ustr, u"https://github.com/muglug/intos"_ustr,
      nullptr, {}, false, u"IntosDisplay-", u"https://github.com/muglug/intos/blob/fef9315c14da9e4b23b4c3cac8e718998d4e4736/LICENSE.txt"_ustr },
    { u"Intos Narrow"_ustr, u"Aptos Narrow"_ustr, u"https://github.com/muglug/intos"_ustr,
      nullptr, {}, false, u"IntosNarrow-", u"https://github.com/muglug/intos/blob/fef9315c14da9e4b23b4c3cac8e718998d4e4736/LICENSE.txt"_ustr },
    { u"Intos Serif"_ustr, u"Aptos Serif"_ustr, u"https://github.com/muglug/intos"_ustr,
      nullptr, {}, false, u"IntosSerif-", u"https://github.com/muglug/intos/blob/fef9315c14da9e4b23b4c3cac8e718998d4e4736/LICENSE.txt"_ustr }
};

struct Download
{
    size_t nRow;
    std::vector<unsigned char> aBytes;
    bool bAppleAvailable = false;
};

std::vector<unsigned char> fetch(const Source& rSource, const std::atomic<bool>& rCancelled)
{
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> xCurl(curl_easy_init(), curl_easy_cleanup);
    if (!xCurl)
        return {};
    std::vector<unsigned char> aBytes;
    aBytes.reserve(rSource.nSize);
    auto write = +[](char* pData, size_t nSize, size_t nCount, void* pTarget) -> size_t {
        auto& rBytes = *static_cast<std::vector<unsigned char>*>(pTarget);
        const size_t nBytes = nSize * nCount;
        if (nBytes > rBytes.capacity() - rBytes.size())
            return 0;
        rBytes.insert(rBytes.end(), pData, pData + nBytes);
        return nBytes;
    };
    curl_easy_setopt(xCurl.get(), CURLOPT_URL, rSource.pURL);
    curl_easy_setopt(xCurl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(xCurl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(xCurl.get(), CURLOPT_MAXREDIRS, 4L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(xCurl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(xCurl.get(), CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
#endif
    curl_easy_setopt(xCurl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(xCurl.get(), CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(xCurl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(xCurl.get(), CURLOPT_WRITEFUNCTION, write);
    curl_easy_setopt(xCurl.get(), CURLOPT_WRITEDATA, &aBytes);
    auto progress = +[](void* pState, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
        return static_cast<const std::atomic<bool>*>(pState)->load() ? 1 : 0;
    };
    curl_easy_setopt(xCurl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(xCurl.get(), CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(xCurl.get(), CURLOPT_XFERINFODATA, &rCancelled);
    if (curl_easy_perform(xCurl.get()) != CURLE_OK || aBytes.size() != rSource.nSize
        || comphelper::hashToString(comphelper::Hash::calculateHash(
               aBytes.data(), aBytes.size(), comphelper::HashType::SHA256)) != rSource.pSha256)
        return {};
    return aBytes;
}

OUString installDirectory()
{
    OUString aHome;
#if defined(_WIN32)
    const OUString aVariable(u"LOCALAPPDATA"_ustr);
    OUString aSystemPath;
    if (osl_getEnvironment(aVariable.pData, &aSystemPath.pData) == osl_Process_E_None
        && osl::File::getFileURLFromSystemPath(aSystemPath, aHome) == osl::File::E_None
        && aHome.startsWith("file:///"))
        return aHome + "/Microsoft/Windows/Fonts";
    return {};
#else
    if (!osl::Security().getHomeDir(aHome))
        return {};
#if defined(MACOSX)
    return aHome + "/Library/Application%20Support/Dainvo%20Office/Fonts";
#else
    const OUString aVariable(u"XDG_DATA_HOME"_ustr);
    OUString aData;
    OUString aURL;
    if (osl_getEnvironment(aVariable.pData, &aData.pData) == osl_Process_E_None
        && aData.startsWith("/")
        && osl::File::getFileURLFromSystemPath(aData, aURL) == osl::File::E_None)
        return aURL + "/fonts/dainvo";
    return aHome + "/.local/share/fonts/dainvo";
#endif
#endif
}

bool registerFont(const OUString& rURL, const OUString& rName)
{
#if defined(MACOSX)
    (void)rName;
    OString aURL = OUStringToOString(rURL, RTL_TEXTENCODING_UTF8);
    CFURLRef pURL = CFURLCreateWithBytes(nullptr,
        reinterpret_cast<const UInt8*>(aURL.getStr()), aURL.getLength(), kCFStringEncodingUTF8,
        nullptr);
    if (!pURL)
        return false;
    CFErrorRef pError = nullptr;
    bool bRegistered = CTFontManagerRegisterFontsForURL(pURL, kCTFontManagerScopePersistent, &pError);
    if (pError)
    {
        bRegistered = CFEqual(CFErrorGetDomain(pError), kCTFontManagerErrorDomain)
            && (CFErrorGetCode(pError) == kCTFontManagerErrorAlreadyRegistered
                || CFErrorGetCode(pError) == kCTFontManagerErrorDuplicatedName);
        CFRelease(pError);
    }
    CFRelease(pURL);
    return bRegistered;
#elif defined(_WIN32)
    OUString aPath;
    if (osl::File::getSystemPathFromFileURL(rURL, aPath) != osl::File::E_None)
        return false;
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    const OUString aValue = rName + (rURL.endsWith(".otf") ? " (OpenType)" : " (TrueType)");
    const bool bLoaded = AddFontResourceExW(o3tl::toW(aPath.getStr()), 0, nullptr) > 0;
    const bool bSaved = bLoaded && RegSetValueExW(
        hKey, o3tl::toW(aValue.getStr()), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(aPath.getStr()),
        (aPath.getLength() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
    if (bLoaded && !bSaved)
        RemoveFontResourceExW(o3tl::toW(aPath.getStr()), 0, nullptr);
    RegCloseKey(hKey);
    if (bSaved)
        Application::GetDefaultDevice()->AddTempDevFont(rURL, OUString());
    return bSaved;
#else
    (void)rName;
    return Application::GetDefaultDevice()->AddTempDevFont(rURL, OUString());
#endif
}

constexpr size_t MaxFontFileSize = 32 * 1024 * 1024;

std::vector<unsigned char> readFont(const OUString& rURL)
{
    if (!rURL.startsWith("file:///"))
        return {};
    osl::File aFile(rURL);
    sal_uInt64 nSize = 0;
    if (aFile.open(osl_File_OpenFlag_Read) != osl::File::E_None
        || aFile.getSize(nSize) != osl::File::E_None || nSize < 12 || nSize > MaxFontFileSize)
        return {};
    std::vector<unsigned char> aBytes(nSize);
    sal_uInt64 nRead = 0;
    if (aFile.read(aBytes.data(), nSize, nRead) != osl::File::E_None || nRead != nSize)
        return {};
    return aBytes;
}

bool installFont(const OUString& rDirectory, const OUString& rName,
                 std::span<const unsigned char> aBytes)
{
    if (aBytes.empty())
        return false;
    const OUString aURL = rDirectory + "/" + rName;
    osl::File aFile(aURL);
    const auto eOpen = aFile.open(osl_File_OpenFlag_Create | osl_File_OpenFlag_Write);
    if (eOpen == osl::File::E_EXIST)
    {
        // A retry can reuse identical bytes from an earlier installation.
        const auto aExisting = readFont(aURL);
        if (aExisting.size() != aBytes.size()
            || !std::equal(aExisting.begin(), aExisting.end(), aBytes.begin()))
            return false;
    }
    else
    {
        if (eOpen != osl::File::E_None)
            return false;
        sal_uInt64 nWritten = 0;
        const auto eWrite = aFile.write(aBytes.data(), aBytes.size(), nWritten);
        const auto eClose = aFile.close();
        if (eWrite != osl::File::E_None || eClose != osl::File::E_None || nWritten != aBytes.size())
        {
            osl::File::remove(aURL);
            return false;
        }
    }
    if (registerFont(aURL, rName.copy(0, rName.lastIndexOf('.'))))
        return true;
    if (eOpen != osl::File::E_EXIST)
        osl::File::remove(aURL);
    return false;
}

bool ensureDirectory(const OUString& rDirectory)
{
    if (rDirectory.isEmpty())
        return false;
    const auto eResult = osl::Directory::createPath(rDirectory);
    return eResult == osl::File::E_None || eResult == osl::File::E_EXIST;
}

bool installBundled(const std::vector<OUString>& rFiles)
{
    const OUString aDirectory = installDirectory();
    if (rFiles.empty() || !ensureDirectory(aDirectory))
        return false;
    bool bInstalled = true;
    for (const auto& rURL : rFiles)
    {
        const OUString aName = INetURLObject(rURL).GetLastName();
        bInstalled &= installFont(aDirectory, aName, readFont(rURL));
    }
    return bInstalled;
}

bool installLocalFile(const OUString& rDirectory, const OUString& rURL)
{
    const auto aBytes = readFont(rURL);
    if (aBytes.empty())
        return false;
    const auto* pData = reinterpret_cast<const char*>(aBytes.data());
    const std::string_view aMagic(pData, 4);
    std::u16string_view aExtension;
    if (aMagic == std::string_view("\0\1\0\0", 4) || aMagic == "true")
        aExtension = u".ttf";
    else if (aMagic == "OTTO")
        aExtension = u".otf";
    else if (aMagic == "ttcf")
        aExtension = u".ttc";
    else
        return false;
    std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)> xBlob(
        hb_blob_create(pData, aBytes.size(), HB_MEMORY_MODE_READONLY, nullptr, nullptr),
        hb_blob_destroy);
    const auto nFaces = hb_face_count(xBlob.get());
    if (!nFaces || nFaces > 256)
        return false;
    for (unsigned i = 0; i < nFaces; ++i)
    {
        std::unique_ptr<hb_face_t, decltype(&hb_face_destroy)> xFace(hb_face_create(xBlob.get(), i),
                                                                    hb_face_destroy);
        if (!hb_face_get_glyph_count(xFace.get()))
            return false;
    }
    // Name the copy by content so a different font with the same original filename
    // cannot overwrite one already installed.
    const auto aDigest = comphelper::Hash::calculateHash(
        aBytes.data(), aBytes.size(), comphelper::HashType::SHA256);
    const OUString aName = "Dainvo-" + OUString::createFromAscii(comphelper::hashToString(aDigest).c_str())
        + aExtension;
    return installFont(rDirectory, aName, aBytes);
}

#if defined(MACOSX)
bool requestAppleFonts(const FontRow& rRow, const std::atomic<bool>& rCancelled)
{
    CFMutableArrayRef pDescriptors = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);
    const void* pAttribute = kCTFontNameAttribute;
    for (const auto* pFace : rRow.aAppleFaces)
    {
        CFStringRef pName = CFStringCreateWithCString(nullptr, pFace, kCFStringEncodingUTF8);
        const void* pValue = pName;
        CFDictionaryRef pAttributes = CFDictionaryCreate(nullptr, &pAttribute, &pValue, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CTFontDescriptorRef pDescriptor = CTFontDescriptorCreateWithAttributes(pAttributes);
        CFArrayAppendValue(pDescriptors, pDescriptor);
        CFRelease(pDescriptor);
        CFRelease(pAttributes);
        CFRelease(pName);
    }
    CFSetRef pMandatory = CFSetCreate(nullptr, &pAttribute, 1, &kCFTypeSetCallBacks);
    // Core Text may deliver another callback after the dialog has closed.
    struct Request
    {
        std::atomic<bool> bFinished{ false };
        std::atomic<bool> bCancelled{ false };
    };
    auto xRequest = std::make_shared<Request>();
    const bool bStarted = CTFontDescriptorMatchFontDescriptorsWithProgressHandler(
        pDescriptors, pMandatory, ^bool(CTFontDescriptorMatchingState eState, CFDictionaryRef) {
            if (eState == kCTFontDescriptorMatchingDidFinish)
                xRequest->bFinished = true;
            return !xRequest->bCancelled;
        });
    CFRelease(pMandatory);
    CFRelease(pDescriptors);
    if (!bStarted)
        return false;
    const auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!xRequest->bFinished && !rCancelled && std::chrono::steady_clock::now() < aDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    xRequest->bCancelled = !xRequest->bFinished;
    return xRequest->bFinished;
}
#endif

bool install(const Download& rDownload)
{
    if (rDownload.aBytes.empty())
        return false;
    utl::TempFileNamed aArchive;
    aArchive.EnableKillingFile();
    SvStream* pStream = aArchive.GetStream(StreamMode::WRITE);
    if (!pStream || pStream->WriteBytes(rDownload.aBytes.data(), rDownload.aBytes.size())
                        != rDownload.aBytes.size() || pStream->GetError() != ERRCODE_NONE)
        return false;
    aArchive.CloseStream();

    const auto xZip = css::packages::zip::ZipFileAccess::createWithURL(
        comphelper::getProcessComponentContext(), aArchive.GetURL());
    const Source& rSource = *Rows[rDownload.nRow].pDownload;
    const OUString aDirectory = installDirectory();
    if (!ensureDirectory(aDirectory))
        return false;

    size_t nInstalled = 0;
    for (const OUString& rName : xZip->getElementNames())
    {
        if (!rName.startsWith(rSource.aPrefix) || !rName.endsWith(".ttf")
            || rName.indexOf('/') >= 0 || rName.indexOf('\\') >= 0)
            continue;
        const css::uno::Reference<css::io::XInputStream> xEntry(xZip->getByName(rName), css::uno::UNO_QUERY_THROW);
        css::uno::Sequence<sal_Int8> aBytes;
        const sal_Int32 nBytes = xEntry->readBytes(aBytes, 1024 * 1024);
        xEntry->closeInput();
        if (nBytes <= 0 || nBytes >= 1024 * 1024)
            return false;
#if defined(_WIN32)
        const OUString aFileName = "Dainvo-" + rName;
#else
        const OUString& aFileName = rName;
#endif
        if (!installFont(aDirectory, aFileName,
                         { reinterpret_cast<const unsigned char*>(aBytes.getConstArray()),
                           static_cast<size_t>(nBytes) }))
            return false;
        ++nInstalled;
    }
    return nInstalled == rSource.nFonts;
}

std::array<std::vector<OUString>, std::size(Rows)> findBundledFonts()
{
    std::array<std::vector<OUString>, std::size(Rows)> aFiles;
    OUString aPath("$BRAND_BASE_DIR/" LIBO_SHARE_FOLDER "/fonts/truetype");
    rtl::Bootstrap::expandMacros(aPath);
    osl::Directory aDirectory(aPath);
    if (aDirectory.open() != osl::File::E_None)
        return aFiles;
    osl::DirectoryItem aItem;
    while (aDirectory.getNextItem(aItem) == osl::File::E_None)
    {
        osl::FileStatus aStatus(osl_FileStatus_Mask_FileName | osl_FileStatus_Mask_FileURL);
        if (aItem.getFileStatus(aStatus) != osl::File::E_None)
            continue;
        const OUString aName = aStatus.getFileName();
        if (!aName.endsWith(".ttf"))
            continue;
        for (size_t i = 0; i < std::size(Rows); ++i)
            if (!Rows[i].aBundledPrefix.empty() && aName.startsWith(Rows[i].aBundledPrefix))
                aFiles[i].push_back(aStatus.getFileURL());
    }
    return aFiles;
}

bool canRequestAppleFont(const FontRow& rRow)
{
#if defined(MACOSX)
    return rRow.aAppleFaces.front() != nullptr;
#else
    (void)rRow;
    return false;
#endif
}

#if defined(LINUX)
constexpr OUStringLiteral AptUrlExecutable = u"file:///usr/bin/apturl";
constexpr OUStringLiteral CoreFontsPackage = u"apt:ttf-mscorefonts-installer";
#endif

void refreshFonts()
{
#if defined(_WIN32)
    SendNotifyMessageW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
#endif
    OutputDevice::ImplClearAllFontData(true);
    OutputDevice::ImplRefreshAllFontData(true);
    DataChangedEvent aChanged(DataChangedEventType::FONTS);
    Application::NotifyAllWindows(aChanged);
}

class FontDialog final : public weld::GenericDialogController
{
    std::unique_ptr<weld::TreeView> m_xList;
    std::unique_ptr<weld::LinkButton> m_xSource;
    std::unique_ptr<weld::LinkButton> m_xLicense;
    std::unique_ptr<weld::CheckButton> m_xAccept;
    std::unique_ptr<weld::Button> m_xInstall;
    std::unique_ptr<weld::Button> m_xImport;
    std::unique_ptr<weld::Button> m_xScan;
    std::unique_ptr<weld::Button> m_xFormats;
    std::unique_ptr<weld::Button> m_xClose;
    std::unique_ptr<weld::Label> m_xStatus;
    const std::array<std::vector<OUString>, std::size(Rows)> m_aBundled = findBundledFonts();
    std::array<bool, std::size(Rows)> m_aAvailable{};
    std::array<bool, std::size(Rows)> m_aInstalled{};
    std::atomic<bool> m_bCancelled{ false };
    std::future<std::vector<Download>> m_aDownload;
    AutoTimer m_aPoll{ "Dainvo font downloads" };

    void scan();
    void update();
    bool canInstall(size_t nRow) const;
    DECL_LINK(Toggle, const weld::TreeView::iter_col&, void);
    DECL_LINK(Selection, weld::ItemView&, void);
    DECL_LINK(Accept, weld::Toggleable&, void);
    DECL_LINK(Install, weld::Button&, void);
    DECL_LINK(Scan, weld::Button&, void);
    DECL_LINK(Import, weld::Button&, void);
    DECL_LINK(Formats, weld::Button&, void);
    DECL_LINK(Poll, Timer*, void);
    DECL_LINK(OpenSource, weld::LinkButton&, bool);

public:
    explicit FontDialog(weld::Window* pParent);
    ~FontDialog() override;
};

FontDialog::FontDialog(weld::Window* pParent)
    : GenericDialogController(pParent, u"cui/ui/dainvofontsdialog.ui"_ustr, u"FontDialog"_ustr)
    , m_xList(m_xBuilder->weld_tree_view(u"fonts"_ustr))
    , m_xSource(m_xBuilder->weld_link_button(u"source"_ustr))
    , m_xLicense(m_xBuilder->weld_link_button(u"license"_ustr))
    , m_xAccept(m_xBuilder->weld_check_button(u"accept"_ustr))
    , m_xInstall(m_xBuilder->weld_button(u"install"_ustr))
    , m_xImport(m_xBuilder->weld_button(u"import"_ustr))
    , m_xScan(m_xBuilder->weld_button(u"scan"_ustr))
    , m_xFormats(m_xBuilder->weld_button(u"formats"_ustr))
    , m_xClose(m_xBuilder->weld_button(u"close"_ustr))
    , m_xStatus(m_xBuilder->weld_label(u"status"_ustr))
{
    m_xList->set_size_request(m_xList->get_approximate_digit_width() * 83,
                             m_xList->get_height_rows(14));
    m_xList->set_column_fixed_widths({ 70, 165, 245 });
    m_xList->connect_toggled(LINK(this, FontDialog, Toggle));
    m_xList->connect_selection_changed(LINK(this, FontDialog, Selection));
    m_xAccept->connect_toggled(LINK(this, FontDialog, Accept));
    m_xInstall->connect_clicked(LINK(this, FontDialog, Install));
    m_xImport->connect_clicked(LINK(this, FontDialog, Import));
    m_xScan->connect_clicked(LINK(this, FontDialog, Scan));
    m_xFormats->connect_clicked(LINK(this, FontDialog, Formats));
    m_xSource->connect_activate_link(LINK(this, FontDialog, OpenSource));
    m_xLicense->connect_activate_link(LINK(this, FontDialog, OpenSource));
    m_aPoll.SetTimeout(150);
    m_aPoll.SetInvokeHandler(LINK(this, FontDialog, Poll));
    scan();
}

FontDialog::~FontDialog()
{
    m_aPoll.Stop();
    if (m_aDownload.valid())
    {
        m_bCancelled = true;
        SolarMutexReleaser aUnlock;
        m_aDownload.wait();
    }
}

void FontDialog::scan()
{
    const int nPrevious = m_xList->get_selected_index();
    auto* pDevice = Application::GetDefaultDevice();
#if defined(LINUX)
    // VCL can keep its startup font list after another application installs a font.
    std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)> xConfig(FcInitLoadConfigAndFonts(),
                                                                FcConfigDestroy);
    FcFontSet* pSystemFonts = xConfig ? FcConfigGetFonts(xConfig.get(), FcSetSystem) : nullptr;
    auto hasSystemFont = [pSystemFonts](std::u16string_view rName, const char* pProperty) {
        if (!pSystemFonts)
            return false;
        const OString aName = OUStringToOString(OUString(rName), RTL_TEXTENCODING_UTF8);
        const auto* pName = reinterpret_cast<const FcChar8*>(aName.getStr());
        for (int i = 0; i < pSystemFonts->nfont; ++i)
        {
            FcChar8* pValue = nullptr;
            for (int j = 0; FcPatternGetString(pSystemFonts->fonts[i], pProperty, j, &pValue)
                            == FcResultMatch; ++j)
            {
                if (FcStrCmpIgnoreCase(pValue, pName) == 0)
                    return true;
            }
        }
        return false;
    };
#endif
    auto hasFamily = [&](std::u16string_view rName) {
        OUString aFamily, aStyle;
        if (pDevice->GetTypographicFontName(rName, WEIGHT_NORMAL, WIDTH_DONTKNOW,
                                            ITALIC_NONE, aFamily, aStyle))
            return true;
#if defined(LINUX)
        return hasSystemFont(rName, FC_FAMILY);
#else
        return false;
#endif
    };
    const OUString aDirectory = installDirectory();
    m_xList->freeze();
    m_xList->clear();
    for (size_t i = 0; i < std::size(Rows); ++i)
    {
        const auto& rRow = Rows[i];
        bool bAvailable = hasFamily(rRow.aName);
        if (rRow.pDownload == &AptosSource)
        {
            bAvailable = bAvailable && hasFamily(u"Aptos Narrow") && hasFamily(u"Aptos Mono")
                && hasFamily(u"Aptos Serif");
#if defined(MACOSX)
            CFArrayRef pNames = CTFontManagerCopyAvailablePostScriptNames();
            bAvailable = bAvailable && pNames
                && CFArrayContainsValue(pNames, CFRangeMake(0, CFArrayGetCount(pNames)),
                                        CFSTR("AptosDisplay"));
            if (pNames)
                CFRelease(pNames);
#elif defined(LINUX)
            bAvailable = bAvailable && (hasFamily(u"Aptos Display")
                || hasSystemFont(u"AptosDisplay", FC_POSTSCRIPT_NAME));
#else
            // The typographic family is Aptos for both the body and display faces.
            bAvailable = bAvailable && hasFamily(u"Aptos Display");
#endif
        }
        m_aAvailable[i] = bAvailable;
        m_aInstalled[i] = !aDirectory.isEmpty() && !m_aBundled[i].empty()
            && std::all_of(m_aBundled[i].begin(), m_aBundled[i].end(), [&](const OUString& rURL) {
                osl::DirectoryItem aItem;
                return osl::DirectoryItem::get(aDirectory + "/" + INetURLObject(rURL).GetLastName(),
                                                aItem) == osl::File::E_None;
            });
        const int nRow = static_cast<int>(i);
        m_xList->append();
        if (canInstall(i))
            m_xList->set_toggle(nRow, TRISTATE_FALSE, 0);
        m_xList->set_text(nRow, rRow.aName, 1);
        OUString aStatus;
        if (!m_aBundled[i].empty())
            aStatus = CuiResId(m_aInstalled[i] ? RID_CUISTR_FONTS_INSTALLED_FOR_USER
                                               : RID_CUISTR_FONTS_BUNDLED);
        else if (bAvailable)
            aStatus = CuiResId(RID_CUISTR_FONTS_AVAILABLE);
        else if (rRow.pDownload)
            aStatus = CuiResId(RID_CUISTR_FONTS_DOWNLOAD);
        else if (canRequestAppleFont(rRow))
            aStatus = CuiResId(RID_CUISTR_FONTS_FROM_MACOS);
        else
            aStatus = CuiResId(!rRow.aBundledPrefix.empty() ? RID_CUISTR_FONTS_GET_UPSTREAM
                                                         : RID_CUISTR_FONTS_GET_VENDOR);
        m_xList->set_text(nRow, aStatus, 2);
        m_xList->set_text(nRow, rRow.aAlternative, 3);
    }
    m_xList->thaw();
    const auto it = std::find(m_aAvailable.begin(), m_aAvailable.end(), false);
    m_xList->select(nPrevious >= 0 ? nPrevious
        : it == m_aAvailable.end() ? 0 : static_cast<int>(std::distance(m_aAvailable.begin(), it)));
    Selection(*m_xList);
    update();
}

bool FontDialog::canInstall(size_t nRow) const
{
    const auto& rRow = Rows[nRow];
    if (!m_aBundled[nRow].empty())
        return !m_aInstalled[nRow];
    return !m_aAvailable[nRow] && (rRow.pDownload || canRequestAppleFont(rRow));
}

void FontDialog::update()
{
    const bool bBusy = m_aDownload.valid();
    bool bAny = false;
    bool bNeedsAcceptance = false;
    for (size_t i = 0; i < std::size(Rows); ++i)
    {
        if (!canInstall(i)
            || m_xList->get_toggle(static_cast<int>(i), 0) != TRISTATE_TRUE)
            continue;
        bAny = true;
        bNeedsAcceptance |= Rows[i].pDownload == &AptosSource;
    }
    m_xAccept->set_sensitive(!bBusy && bNeedsAcceptance);
    m_xInstall->set_sensitive(!bBusy && bAny && (!bNeedsAcceptance || m_xAccept->get_active()));
    m_xList->set_sensitive(!bBusy);
    m_xScan->set_sensitive(!bBusy);
    m_xImport->set_sensitive(!bBusy);
    m_xFormats->set_sensitive(!bBusy);
    m_xClose->set_label(CuiResId(bBusy ? RID_CUISTR_FONTS_CANCEL_DOWNLOAD
                                      : RID_CUISTR_FONTS_CLOSE));
}

IMPL_LINK_NOARG(FontDialog, Toggle, const weld::TreeView::iter_col&, void)
{
    m_xAccept->set_active(false);
    update();
}

IMPL_LINK_NOARG(FontDialog, Selection, weld::ItemView&, void)
{
    const int n = m_xList->get_selected_index();
    if (n < 0)
    {
        m_xSource->hide();
        m_xLicense->hide();
        return;
    }
    const auto& rRow = Rows[n];
    OUString aURL = rRow.aSource;
    OUString aLicense = rRow.aLicense;
    auto aSourceLabel = RID_CUISTR_FONTS_OPEN_SOURCE;
#if defined(MACOSX)
    if (canRequestAppleFont(rRow) && !m_aAvailable[n])
    {
        aURL = u"file:///System/Applications/Font%20Book.app"_ustr;
        aSourceLabel = RID_CUISTR_FONTS_OPEN_FONTBOOK;
    }
#elif defined(LINUX)
    if (rRow.bLinuxCoreFont && !m_aAvailable[n])
    {
        aURL = u"https://packages.ubuntu.com/noble/ttf-mscorefonts-installer"_ustr;
        aLicense = u"https://corefonts.sourceforge.net/eula.htm"_ustr;
        osl::DirectoryItem aAptUrl;
        if (osl::DirectoryItem::get(AptUrlExecutable, aAptUrl) == osl::File::E_None)
        {
            aURL = CoreFontsPackage;
            aSourceLabel = RID_CUISTR_FONTS_OPEN_PACKAGE_MANAGER;
        }
    }
#endif
    m_xSource->set_visible(!aURL.isEmpty());
    m_xSource->set_uri(aURL);
    m_xSource->set_label(CuiResId(aSourceLabel));
    m_xLicense->set_visible(!aLicense.isEmpty());
    m_xLicense->set_uri(aLicense);
    m_xLicense->set_label(CuiResId(rRow.pDownload == &AptosSource
        ? RID_CUISTR_FONTS_APTOS_LICENSE : RID_CUISTR_FONTS_ROW_LICENSE));
}

IMPL_LINK_NOARG(FontDialog, Accept, weld::Toggleable&, void) { update(); }

IMPL_LINK_NOARG(FontDialog, Scan, weld::Button&, void)
{
    refreshFonts();
    scan();
    m_xStatus->set_label(OUString());
}

IMPL_LINK_NOARG(FontDialog, Formats, weld::Button&, void)
{
    try
    {
        sfx2::SetDainvoSaveFormats(true);
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_FORMATS_SET));
    }
    catch (const css::uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("cui.options", "Setting Office save formats");
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_FORMATS_FAILED));
    }
}

IMPL_LINK_NOARG(FontDialog, Install, weld::Button&, void)
{
    std::vector<size_t> aSelection;
    for (size_t i = 0; i < std::size(Rows); ++i)
    {
        if (canInstall(i)
            && m_xList->get_toggle(static_cast<int>(i), 0) == TRISTATE_TRUE)
        {
            if (Rows[i].pDownload == &AptosSource && !m_xAccept->get_active())
                return;
            aSelection.push_back(i);
        }
    }
    if (aSelection.empty())
        return;

    m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_INSTALLING));
    try
    {
        m_bCancelled = false;
        m_aDownload = std::async(std::launch::async, [this, aSelection = std::move(aSelection)] {
            std::vector<Download> aDownloads;
            for (size_t i : aSelection)
            {
                if (m_bCancelled)
                    break;
                const auto& rRow = Rows[i];
                Download aDownload{ i, {} };
                if (rRow.pDownload)
                    aDownload.aBytes = fetch(*rRow.pDownload, m_bCancelled);
#if defined(MACOSX)
                else if (canRequestAppleFont(rRow))
                    aDownload.bAppleAvailable = requestAppleFonts(rRow, m_bCancelled);
#endif
                aDownloads.push_back(std::move(aDownload));
            }
            return aDownloads;
        });
        update();
        m_aPoll.Start();
    }
    catch (const std::exception& e)
    {
        SAL_WARN("cui.options", "Font download: " << e.what());
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_FAILED));
    }
}

IMPL_LINK_NOARG(FontDialog, Poll, Timer*, void)
{
    if (m_aDownload.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    m_aPoll.Stop();
    try
    {
        const auto aDownloads = m_aDownload.get();
        std::vector<size_t> aFailedRows;
        for (const auto& rDownload : aDownloads)
        {
            const auto nRow = rDownload.nRow;
            bool bInstalled = false;
            try
            {
                if (!m_aBundled[nRow].empty())
                    bInstalled = installBundled(m_aBundled[nRow]);
                else if (canRequestAppleFont(Rows[nRow]))
                    bInstalled = rDownload.bAppleAvailable;
                else
                    bInstalled = install(rDownload);
            }
            catch (const css::uno::Exception&)
            {
                TOOLS_WARN_EXCEPTION("cui.options", "Installing font package");
            }
            if (!bInstalled)
                aFailedRows.push_back(nRow);
        }
        refreshFonts();
        scan();
        OUString aFailed;
        for (const auto& rDownload : aDownloads)
        {
            const auto nRow = rDownload.nRow;
            if (std::find(aFailedRows.begin(), aFailedRows.end(), nRow) == aFailedRows.end()
                && (canRequestAppleFont(Rows[nRow]) ? m_aAvailable[nRow] : !canInstall(nRow)))
                continue;
            if (!aFailed.isEmpty())
                aFailed += ", ";
            aFailed += Rows[nRow].aName;
        }
        m_xStatus->set_label(aFailed.isEmpty() ? CuiResId(RID_CUISTR_FONTS_INSTALLED)
            : CuiResId(RID_CUISTR_FONTS_FAILED_NAMES).replaceFirst("%1", aFailed));
    }
    catch (const std::exception& e)
    {
        SAL_WARN("cui.options", "Installing font packages: " << e.what());
        scan();
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_FAILED));
    }
    m_xAccept->set_active(false);
    update();
}

IMPL_LINK_NOARG(FontDialog, Import, weld::Button&, void)
{
    sfx2::FileDialogHelper aPicker(css::ui::dialogs::TemplateDescription::FILEOPEN_SIMPLE,
                                  FileDialogFlags::MultiSelection, m_xDialog.get());
    aPicker.SetTitle(CuiResId(RID_CUISTR_FONTS_PICKER_TITLE));
    const auto aFilter = CuiResId(RID_CUISTR_FONTS_PICKER_FILTER);
    aPicker.AddFilter(aFilter, u"*.ttf;*.otf;*.ttc;*.otc"_ustr);
    aPicker.SetCurrentFilter(aFilter);
    if (aPicker.Execute() != ERRCODE_NONE)
        return;

    const OUString aDirectory = installDirectory();
    if (!ensureDirectory(aDirectory))
    {
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_FAILED));
        return;
    }
    OUString aFailed;
    for (const auto& rURL : aPicker.GetSelectedFiles())
    {
        if (installLocalFile(aDirectory, rURL))
            continue;
        if (!aFailed.isEmpty())
            aFailed += ", ";
        aFailed += INetURLObject(rURL).GetLastName();
    }
    refreshFonts();
    scan();
    m_xStatus->set_label(aFailed.isEmpty() ? CuiResId(RID_CUISTR_FONTS_INSTALLED)
        : CuiResId(RID_CUISTR_FONTS_FAILED_NAMES).replaceFirst("%1", aFailed));
}

IMPL_LINK(FontDialog, OpenSource, weld::LinkButton&, rLink, bool)
{
    try
    {
#if defined(LINUX)
        if (rLink.get_uri() == CoreFontsPackage)
        {
            const OUString aExecutable(AptUrlExecutable);
            const OUString aPackage(CoreFontsPackage);
            rtl_uString* aArgs[] = { aPackage.pData };
            oslProcess aProcess = nullptr;
            if (osl_executeProcess(aExecutable.pData, aArgs, std::size(aArgs), osl_Process_NORMAL,
                                   nullptr, nullptr, nullptr, 0, &aProcess)
                != osl_Process_E_None)
                throw css::uno::RuntimeException(u"Could not open the package manager"_ustr);
            osl_freeProcessHandle(aProcess);
            return true;
        }
#endif
#if defined(MACOSX)
        if (rLink.get_uri().startsWith("file:"))
        {
            const OUString aOpen(u"file:///usr/bin/open"_ustr);
            const OUString aAppFlag(u"-a"_ustr);
            const OUString aFontBook(u"/System/Applications/Font Book.app"_ustr);
            rtl_uString* aArgs[] = { aAppFlag.pData, aFontBook.pData };
            oslProcess aProcess = nullptr;
            if (osl_executeProcess(aOpen.pData, aArgs, std::size(aArgs), osl_Process_NORMAL,
                                   nullptr, nullptr, nullptr, 0, &aProcess)
                != osl_Process_E_None)
                throw css::uno::RuntimeException(u"Could not open Font Book"_ustr);
            osl_freeProcessHandle(aProcess);
            return true;
        }
#endif
        const auto xShell = css::system::SystemShellExecute::create(
            comphelper::getProcessComponentContext());
        xShell->execute(rLink.get_uri(), OUString(), css::system::SystemShellExecuteFlags::URIS_ONLY);
    }
    catch (const css::uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("cui.options", "Opening font source");
        m_xStatus->set_label(CuiResId(RID_CUISTR_FONTS_LINK_FAILED));
    }
    return true;
}
}

void ShowDainvoFonts(weld::Window* pParent)
{
    FontDialog aDialog(pParent);
    aDialog.run();
}
