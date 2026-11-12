/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dainvo Office language packs. See include/linguistic/dainvolangpacks.hxx and
// dainvo/docs/LANGUAGE_TOOLS.md.

#include <sal/config.h>

#include <config_version.h>

#include <linguistic/dainvolangpacks.hxx>

#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/XTerminateListener.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/lang/EventObject.hpp>
#include <com/sun/star/linguistic2/LinguServiceManager.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/util/XModifyListener.hpp>
#include <com/sun/star/util/XRefreshable.hpp>

#include <comphelper/base64.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/DirectoryHelper.hxx>
#include <comphelper/hash.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/sequence.hxx>
#include <cppuhelper/implbase.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <linguistic/misc.hxx>
#include <officecfg/Office/Linguistic.hxx>
#include <osl/file.hxx>
#include <rtl/bootstrap.hxx>
#include <rtl/character.hxx>
#include <rtl/random.h>
#include <rtl/ustrbuf.hxx>
#include <rtl/uri.hxx>
#include <sal/log.hxx>
#include <systools/curlinit.hxx>
#include <tools/link.hxx>
#include <tools/urlobj.hxx>
#include <ucbhelper/proxydecider.hxx>
#include <unotools/bootstrap.hxx>
#include <unotools/lingucfg.hxx>
#include <vcl/svapp.hxx>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <openssl/evp.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <thread>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>

using namespace css;

namespace linguistic::langpacks
{
namespace
{
constexpr OUString PACK_JSON = u"pack.json"_ustr;
constexpr OUString REMOVED_MARKER = u".removed"_ustr;
constexpr OUString STAGING_DIR = u".staging"_ustr;
constexpr OUString INDEX_FILE = u"index.json"_ustr;
constexpr OUString INDEX_SIG_FILE = u"index.json.sig"_ustr;

constexpr sal_Int64 MAX_INDEX_SIZE = 8 * 1024 * 1024;
constexpr sal_Int64 MAX_SIG_SIZE = 4096;
constexpr sal_Int64 MAX_PACK_JSON_SIZE = 256 * 1024;

// ---------------------------------------------------------------- strings

bool isSafeChar(sal_Unicode c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'
           || c == '_' || c == '.';
}

/// Plain file or folder name: ASCII letters, digits, '-', '_', '.', not starting with '.'
bool isSafeName(std::u16string_view s)
{
    if (s.empty() || s.size() > 128 || s[0] == '.')
        return false;
    return std::all_of(s.begin(), s.end(), isSafeChar);
}

/// BCP 47-ish: 2-3 letter language, then subtags of 1-8 letters/digits.
bool isPlausibleLanguageTag(std::u16string_view s)
{
    if (s.empty() || s.size() > 35)
        return false;
    size_t nStart = 0;
    bool bFirst = true;
    while (nStart <= s.size())
    {
        size_t nEnd = s.find('-', nStart);
        if (nEnd == std::u16string_view::npos)
            nEnd = s.size();
        std::u16string_view aPart = s.substr(nStart, nEnd - nStart);
        if (aPart.empty() || aPart.size() > 8)
            return false;
        for (sal_Unicode c : aPart)
        {
            bool bAlpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            bool bDigit = c >= '0' && c <= '9';
            if (bFirst ? !bAlpha : !(bAlpha || bDigit))
                return false;
        }
        if (bFirst && (aPart.size() < 2 || aPart.size() > 3))
            return false;
        bFirst = false;
        nStart = nEnd + 1;
    }
    return true;
}

bool isKind(std::u16string_view s) { return s == u"spell" || s == u"hyph" || s == u"thes"; }

OUString formatForKind(std::u16string_view aKind)
{
    if (aKind == u"spell")
        return u"DICT_SPELL"_ustr;
    if (aKind == u"hyph")
        return u"DICT_HYPH"_ustr;
    return u"DICT_THES"_ustr;
}

/// "de-DE.hyph" -> ("de-DE", "hyph"); empty strings if malformed.
std::pair<OUString, OUString> splitId(const OUString& rId)
{
    sal_Int32 nDot = rId.lastIndexOf('.');
    if (nDot <= 0)
        return {};
    OUString aLang = rId.copy(0, nDot);
    OUString aKind = rId.copy(nDot + 1);
    if (!isPlausibleLanguageTag(aLang) || !isKind(aKind))
        return {};
    return { aLang, aKind };
}

/// Compare "2017.01.12" style versions: numeric parts numerically.
int compareVersions(std::u16string_view a, std::u16string_view b)
{
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size())
    {
        auto take = [](std::u16string_view s, size_t& k) {
            size_t nStart = k;
            while (k < s.size() && s[k] != '.' && s[k] != '-')
                ++k;
            std::u16string_view aPart = s.substr(nStart, k - nStart);
            if (k < s.size())
                ++k;
            return aPart;
        };
        std::u16string_view pa = take(a, i);
        std::u16string_view pb = take(b, j);
        auto isDigit = [](sal_Unicode c) { return rtl::isAsciiDigit(c); };
        bool bNumA = !pa.empty() && std::all_of(pa.begin(), pa.end(), isDigit);
        bool bNumB = !pb.empty() && std::all_of(pb.begin(), pb.end(), isDigit);
        if (bNumA && bNumB)
        {
            // strip leading zeros, then longer is bigger, else lexical
            while (pa.size() > 1 && pa[0] == '0')
                pa.remove_prefix(1);
            while (pb.size() > 1 && pb[0] == '0')
                pb.remove_prefix(1);
            if (pa.size() != pb.size())
                return pa.size() < pb.size() ? -1 : 1;
        }
        if (int n = pa.compare(pb); n != 0)
            return n < 0 ? -1 : 1;
    }
    return 0;
}

OUString fromUtf8(const std::string& s) { return OUString::fromUtf8(s); }

OUString appendSegment(const OUString& rDirUrl, std::u16string_view aName)
{
    return rDirUrl + "/"
           + rtl::Uri::encode(OUString(aName), rtl_UriCharClassPchar, rtl_UriEncodeIgnoreEscapes,
                              RTL_TEXTENCODING_UTF8);
}

// ---------------------------------------------------------------- files

struct DirEntry
{
    OUString aName;
    OUString aUrl;
    bool bDir;
};

std::vector<DirEntry> listDir(const OUString& rDirUrl)
{
    std::vector<DirEntry> aRes;
    osl::Directory aDir(rDirUrl);
    if (aDir.open() != osl::FileBase::E_None)
        return aRes;
    osl::DirectoryItem aItem;
    while (aDir.getNextItem(aItem) == osl::FileBase::E_None)
    {
        osl::FileStatus aStatus(osl_FileStatus_Mask_Type | osl_FileStatus_Mask_FileName
                                | osl_FileStatus_Mask_FileURL);
        if (aItem.getFileStatus(aStatus) != osl::FileBase::E_None)
            continue;
        if (aStatus.getFileType() == osl::FileStatus::Link)
            continue; // never follow links
        aRes.push_back(
            { aStatus.getFileName(), aStatus.getFileURL(), aStatus.isDirectory() });
    }
    std::sort(aRes.begin(), aRes.end(),
              [](const DirEntry& x, const DirEntry& y) { return x.aName < y.aName; });
    return aRes;
}

bool fileExists(const OUString& rUrl)
{
    osl::DirectoryItem aItem;
    if (osl::DirectoryItem::get(rUrl, aItem) != osl::FileBase::E_None)
        return false;
    osl::FileStatus aStatus(osl_FileStatus_Mask_Type);
    return aItem.getFileStatus(aStatus) == osl::FileBase::E_None && aStatus.isRegular();
}

bool readFile(const OUString& rUrl, sal_Int64 nMax, std::vector<char>& rData)
{
    osl::File aFile(rUrl);
    if (aFile.open(osl_File_OpenFlag_Read) != osl::FileBase::E_None)
        return false;
    sal_uInt64 nSize = 0;
    if (aFile.getSize(nSize) != osl::FileBase::E_None || nSize > sal_uInt64(nMax))
        return false;
    rData.resize(nSize);
    sal_uInt64 nRead = 0;
    if (nSize && (aFile.read(rData.data(), nSize, nRead) != osl::FileBase::E_None || nRead != nSize))
        return false;
    return true;
}

bool writeFile(const OUString& rUrl, const void* pData, size_t nSize)
{
    osl::File aFile(rUrl);
    if (aFile.open(osl_File_OpenFlag_Write | osl_File_OpenFlag_Create) != osl::FileBase::E_None)
    {
        osl::File::remove(rUrl);
        if (aFile.open(osl_File_OpenFlag_Write | osl_File_OpenFlag_Create)
            != osl::FileBase::E_None)
            return false;
    }
    sal_uInt64 nWritten = 0;
    bool bOk = !nSize
               || (aFile.write(pData, nSize, nWritten) == osl::FileBase::E_None && nWritten == nSize);
    return aFile.close() == osl::FileBase::E_None && bOk;
}

/// Write to a temporary name and rename over the target, so readers never see half a file.
bool writeFileAtomic(const OUString& rUrl, const std::vector<char>& rData)
{
    OUString aTmp = rUrl + ".tmp";
    osl::File::remove(aTmp);
    if (!writeFile(aTmp, rData.data(), rData.size()))
        return false;
    return osl::File::replace(aTmp, rUrl) == osl::FileBase::E_None;
}

/// Delete a folder and everything in it. Links are removed, never followed.
/// comphelper::DirectoryHelper is not used: it skips names like ".removed".
bool deleteTree(const OUString& rDirUrl)
{
    {
        osl::Directory aDir(rDirUrl);
        if (aDir.open() != osl::FileBase::E_None)
            return false;
        osl::DirectoryItem aItem;
        while (aDir.getNextItem(aItem) == osl::FileBase::E_None)
        {
            osl::FileStatus aStatus(osl_FileStatus_Mask_Type | osl_FileStatus_Mask_FileURL);
            if (aItem.getFileStatus(aStatus) != osl::FileBase::E_None)
                continue;
            if (aStatus.getFileType() == osl::FileStatus::Directory)
                deleteTree(aStatus.getFileURL());
            else
                osl::File::remove(aStatus.getFileURL());
        }
    }
    return osl::Directory::remove(rDirUrl) == osl::FileBase::E_None;
}

bool ensureDir(const OUString& rUrl)
{
    osl::FileBase::RC e = osl::Directory::createPath(rUrl);
    return e == osl::FileBase::E_None || e == osl::FileBase::E_EXIST;
}

OUString randomName()
{
    sal_uInt8 aBytes[8];
    rtl_random_getBytes(nullptr, aBytes, sizeof aBytes);
    OUStringBuffer aBuf;
    for (sal_uInt8 b : aBytes)
    {
        static const char aHex[] = "0123456789abcdef";
        aBuf.append(OUStringChar(aHex[b >> 4]) + OUStringChar(aHex[b & 15]));
    }
    return aBuf.makeStringAndClear();
}

// ---------------------------------------------------------------- pack.json

std::optional<boost::property_tree::ptree> parseJson(const std::vector<char>& rData)
{
    try
    {
        std::istringstream aStream(std::string(rData.data(), rData.size()));
        boost::property_tree::ptree aTree;
        boost::property_tree::read_json(aStream, aTree);
        return aTree;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("linguistic", "language packs: bad JSON: " << e.what());
        return {};
    }
}

std::vector<OUString> stringArray(const boost::property_tree::ptree& rTree, const char* pKey)
{
    std::vector<OUString> aRes;
    if (auto oChild = rTree.get_child_optional(pKey))
        for (const auto& rItem : *oChild)
            aRes.push_back(fromUtf8(rItem.second.get_value<std::string>()));
    return aRes;
}

/// Read and check <aDirUrl>/pack.json. Files must exist next to it.
std::optional<InstalledComponent> readComponent(const OUString& rDirUrl, bool bBundled)
{
    std::vector<char> aData;
    if (!readFile(appendSegment(rDirUrl, PACK_JSON), MAX_PACK_JSON_SIZE, aData))
        return {};
    auto oTree = parseJson(aData);
    if (!oTree)
        return {};
    try
    {
        InstalledComponent aComp;
        if (oTree->get<int>("schema", 0) != 1)
            return {};
        aComp.aId = fromUtf8(oTree->get<std::string>("id"));
        aComp.aKind = fromUtf8(oTree->get<std::string>("kind"));
        aComp.aVersion = fromUtf8(oTree->get<std::string>("version"));
        aComp.aLicense = fromUtf8(oTree->get<std::string>("license", ""));
        aComp.aDirUrl = rDirUrl;
        aComp.bBundled = bBundled;
        if (splitId(aComp.aId).second != aComp.aKind || !isSafeName(aComp.aVersion))
            return {};
        const OUString aFormat = formatForKind(aComp.aKind);
        for (const auto& rDictNode : oTree->get_child("dictionaries"))
        {
            const auto& rDict = rDictNode.second;
            PackDictionary aDict;
            aDict.aFormat = fromUtf8(rDict.get<std::string>("format"));
            if (aDict.aFormat != aFormat)
                return {};
            for (const OUString& rFile : stringArray(rDict, "files"))
            {
                if (!isSafeName(rFile))
                    return {};
                OUString aUrl = appendSegment(rDirUrl, rFile);
                if (!fileExists(aUrl))
                {
                    SAL_WARN("linguistic", "language packs: missing " << aUrl);
                    return {};
                }
                aDict.aFiles.push_back(aUrl);
            }
            for (const OUString& rLocale : stringArray(rDict, "locales"))
                if (isPlausibleLanguageTag(rLocale))
                    aDict.aLocales.push_back(rLocale);
            if (aDict.aFiles.empty() || aDict.aLocales.empty())
                return {};
            // MyThes needs the .idx next to the .dat
            if (aComp.aKind == u"thes")
            {
                const OUString& rDat = aDict.aFiles[0];
                if (!rDat.endsWith(".dat")
                    || !fileExists(rDat.copy(0, rDat.getLength() - 4) + ".idx"))
                    return {};
            }
            aComp.aDictionaries.push_back(std::move(aDict));
        }
        if (aComp.aDictionaries.empty())
            return {};
        return aComp;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("linguistic", "language packs: bad " << rDirUrl << "/pack.json: " << e.what());
        return {};
    }
}

/// Components in one root: per id, the highest version that is not marked removed.
/// If pAll is given, it receives every component folder (for cleanup).
std::vector<InstalledComponent> scanRoot(const OUString& rRootUrl, bool bBundled,
                                         std::vector<std::pair<OUString, bool>>* pAll = nullptr)
{
    std::map<OUString, InstalledComponent> aBest;
    for (const DirEntry& rLang : listDir(rRootUrl))
    {
        if (!rLang.bDir || !isPlausibleLanguageTag(rLang.aName))
            continue;
        for (const DirEntry& rComp : listDir(rLang.aUrl))
        {
            if (!rComp.bDir)
                continue;
            if (fileExists(appendSegment(rComp.aUrl, REMOVED_MARKER)))
            {
                if (pAll)
                    pAll->emplace_back(rComp.aUrl, false);
                continue;
            }
            auto oComp = readComponent(rComp.aUrl, bBundled);
            if (!oComp || splitId(oComp->aId).first != rLang.aName
                || rComp.aName != oComp->aKind + "-" + oComp->aVersion)
            {
                SAL_WARN_IF(!rComp.aName.startsWith("."), "linguistic",
                            "language packs: ignoring " << rComp.aUrl);
                continue;
            }
            if (pAll)
                pAll->emplace_back(rComp.aUrl, true);
            auto it = aBest.find(oComp->aId);
            if (it == aBest.end() || compareVersions(it->second.aVersion, oComp->aVersion) < 0)
                aBest[oComp->aId] = std::move(*oComp);
        }
    }
    std::vector<InstalledComponent> aRes;
    for (auto& rPair : aBest)
        aRes.push_back(std::move(rPair.second));
    return aRes;
}

// ---------------------------------------------------------------- download

size_t curlWriteToVector(char* pData, size_t nSize, size_t nCount, void* pUser)
{
    auto* pTarget = static_cast<std::pair<std::vector<char>*, sal_Int64>*>(pUser);
    size_t nBytes = nSize * nCount;
    if (sal_Int64(pTarget->first->size() + nBytes) > pTarget->second)
        return 0; // too big: abort
    pTarget->first->insert(pTarget->first->end(), pData, pData + nBytes);
    return nBytes;
}

struct FileDownload
{
    osl::File* pFile;
    comphelper::Hash* pHash;
    sal_Int64 nMax;
    sal_Int64 nDone = 0;
    bool bFailed = false;
};

size_t curlWriteToFile(char* pData, size_t nSize, size_t nCount, void* pUser)
{
    auto* pDl = static_cast<FileDownload*>(pUser);
    size_t nBytes = nSize * nCount;
    if (pDl->nDone + sal_Int64(nBytes) > pDl->nMax)
    {
        pDl->bFailed = true;
        return 0;
    }
    sal_uInt64 nWritten = 0;
    if (pDl->pFile->write(pData, nBytes, nWritten) != osl::FileBase::E_None || nWritten != nBytes)
    {
        pDl->bFailed = true;
        return 0;
    }
    pDl->pHash->update(pData, nBytes);
    pDl->nDone += nBytes;
    return nBytes;
}

struct ProgressData
{
    const ProgressFn* pFn;
    sal_Int64 nExpected;
};

int curlProgress(void* pUser, curl_off_t nTotal, curl_off_t nNow, curl_off_t, curl_off_t)
{
    auto* pData = static_cast<ProgressData*>(pUser);
    if (pData->pFn && *pData->pFn)
        return (*pData->pFn)(nNow, nTotal ? nTotal : pData->nExpected) ? 0 : 1;
    return 0;
}

std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> makeCurl(const OString& rUrl,
                                                              OUString& rError)
{
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> pCurl(curl_easy_init(),
                                                              curl_easy_cleanup);
    if (!pCurl)
    {
        rError = u"Could not start the download."_ustr;
        return pCurl;
    }
    try
    {
        ::InitCurl_easy(pCurl.get()); // https only, certificates
    }
    catch (const uno::Exception& e)
    {
        rError = e.Message;
        pCurl.reset();
        return pCurl;
    }
    static const char aUserAgent[] = "DainvoOffice-LanguagePacks/"
#ifdef DAINVO_VERSION
        DAINVO_VERSION
#else
        "dev"
#endif
        ;
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_URL, rUrl.getStr());
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_USERAGENT, aUserAgent);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_MAXREDIRS, 8L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_FAILONERROR, 1L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    // give up if slower than 1 KB/s for a minute
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_LOW_SPEED_TIME, 60L);

    // Proxy from Dainvo's settings (Options > Internet > Proxy), which follow
    // the operating system's proxy settings when set to "System".
    try
    {
        INetURLObject aUrl(OStringToOUString(rUrl, RTL_TEXTENCODING_UTF8));
        ucbhelper::InternetProxyDecider aDecider(comphelper::getProcessComponentContext());
        const OUString aProxy = aDecider.getProxy(
            u"https"_ustr, aUrl.GetHost(),
            aUrl.GetPort() ? sal_Int32(aUrl.GetPort()) : sal_Int32(443));
        if (!aProxy.isEmpty())
            (void)curl_easy_setopt(pCurl.get(), CURLOPT_PROXY,
                                   OUStringToOString(aProxy, RTL_TEXTENCODING_UTF8).getStr());
    }
    catch (const uno::Exception&)
    {
    }
    return pCurl;
}

OUString curlError(CURL* pCurl, CURLcode eCode)
{
    long nHttp = 0;
    (void)curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &nHttp);
    OUString aMsg = OUString::createFromAscii(curl_easy_strerror(eCode));
    if (nHttp >= 400)
        aMsg += " (HTTP " + OUString::number(nHttp) + ")";
    return aMsg;
}

/// Read a small resource from https: or file: into memory.
bool fetchSmall(const OUString& rUrl, sal_Int64 nMax, std::vector<char>& rData, OUString& rError,
                const ProgressFn& rProgress)
{
    rData.clear();
    if (rUrl.startsWithIgnoreAsciiCase("file:"))
    {
        if (!readFile(rUrl, nMax, rData))
        {
            rError = "Could not read " + rUrl;
            return false;
        }
        return true;
    }
    if (!rUrl.startsWithIgnoreAsciiCase("https:"))
    {
        rError = u"Only https: and file: addresses are allowed."_ustr;
        return false;
    }
    auto pCurl = makeCurl(OUStringToOString(rUrl, RTL_TEXTENCODING_UTF8), rError);
    if (!pCurl)
        return false;
    std::pair<std::vector<char>*, sal_Int64> aTarget(&rData, nMax);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_TIMEOUT, 60L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEFUNCTION, curlWriteToVector);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEDATA, &aTarget);
    ProgressData aProgress{ &rProgress, 0 };
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_NOPROGRESS, 0L);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_XFERINFOFUNCTION, curlProgress);
    (void)curl_easy_setopt(pCurl.get(), CURLOPT_XFERINFODATA, &aProgress);
    CURLcode eCode = curl_easy_perform(pCurl.get());
    if (eCode != CURLE_OK)
    {
        rError = eCode == CURLE_ABORTED_BY_CALLBACK ? OUString() : curlError(pCurl.get(), eCode);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- index

std::vector<unsigned char> decodeBase64(std::u16string_view aText)
{
    // tolerate surrounding whitespace and line breaks
    OUStringBuffer aClean;
    for (sal_Unicode c : aText)
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
            aClean.append(c);
    uno::Sequence<sal_Int8> aSeq;
    try
    {
        comphelper::Base64::decode(aSeq, aClean);
    }
    catch (const uno::Exception&)
    {
        return {};
    }
    return std::vector<unsigned char>(aSeq.begin(), aSeq.end());
}

OUString userRootOrEmpty()
{
    OUString aUser;
    const utl::Bootstrap::PathStatus eStatus = utl::Bootstrap::locateUserData(aUser);
    if ((eStatus != utl::Bootstrap::PATH_EXISTS && eStatus != utl::Bootstrap::PATH_VALID)
        || aUser.isEmpty())
        return {};
    return aUser + "/languages";
}

/// Component URLs in the index may be relative to the index's own address,
/// so a whole folder of packs can move (e.g. to a CDN) with one index change.
void resolveRelativeUrls(Index& rIndex)
{
    const OUString aIndexUrl = officecfg::Office::Linguistic::DainvoLanguagePacks::IndexURL::get();
    const OUString aBase = aIndexUrl.copy(0, aIndexUrl.lastIndexOf('/') + 1);
    for (auto& rPair : rIndex.aComponents)
    {
        OUString& rUrl = rPair.second.aUrl;
        if (rUrl.indexOf(':') < 0 && isSafeName(rUrl) && !aBase.isEmpty())
            rUrl = aBase + rUrl;
    }
}

std::mutex& installMutex()
{
    static std::mutex aMutex;
    return aMutex;
}

/// This process's own staging folder. Cleanup() never deletes it, so a
/// download in progress is safe; folders of earlier runs are deleted.
const OUString& sessionName()
{
    static const OUString aName = randomName();
    return aName;
}

OUString sessionStaging(const OUString& rUser)
{
    return appendSegment(appendSegment(rUser, STAGING_DIR), sessionName());
}
}

// ==================================================================== API

OUString GetUserRootUrl() { return userRootOrEmpty(); }

OUString GetBundledRootUrl()
{
    OUString aUrl(u"$BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/languages"_ustr);
    rtl::Bootstrap::expandMacros(aUrl);
    return aUrl;
}

std::vector<InstalledComponent> GetInstalledComponents()
{
    std::vector<InstalledComponent> aRes;
    const OUString aUser = GetUserRootUrl();
    if (!aUser.isEmpty())
        aRes = scanRoot(aUser, false);
    std::vector<InstalledComponent> aBundled = scanRoot(GetBundledRootUrl(), true);
    aRes.insert(aRes.end(), std::make_move_iterator(aBundled.begin()),
                std::make_move_iterator(aBundled.end()));
    return aRes;
}

std::vector<SvtLinguConfigDictionaryEntry> GetDictionaryEntries(std::u16string_view aFormat)
{
    static std::once_flag aCleanupOnce;
    std::call_once(aCleanupOnce, [] { Cleanup(); });

    std::vector<SvtLinguConfigDictionaryEntry> aRes;
    std::set<OUString> aCovered;
    for (const InstalledComponent& rComp : GetInstalledComponents())
    {
        for (const PackDictionary& rDict : rComp.aDictionaries)
        {
            if (rDict.aFormat != aFormat)
                continue;
            std::vector<OUString> aLocales;
            for (const OUString& rLocale : rDict.aLocales)
                if (aCovered.insert(rLocale).second)
                    aLocales.push_back(rLocale);
            if (aLocales.empty())
                continue;
            SvtLinguConfigDictionaryEntry aEntry;
            aEntry.aFormatName = rDict.aFormat;
            aEntry.aLocations = comphelper::containerToSequence(rDict.aFiles);
            aEntry.aLocaleNames = comphelper::containerToSequence(aLocales);
            aRes.push_back(std::move(aEntry));
        }
    }
    return aRes;
}

void Cleanup()
{
    const OUString aUser = GetUserRootUrl();
    if (aUser.isEmpty())
        return;
    std::scoped_lock aGuard(installMutex());
    for (const DirEntry& rEntry : listDir(appendSegment(aUser, STAGING_DIR)))
        if (rEntry.aName != sessionName())
            deleteTree(rEntry.aUrl);

    std::vector<std::pair<OUString, bool>> aAll;
    std::vector<InstalledComponent> aBest = scanRoot(aUser, false, &aAll);
    std::set<OUString> aKeep;
    for (const InstalledComponent& rComp : aBest)
        aKeep.insert(rComp.aDirUrl);
    for (const auto& [rUrl, bValid] : aAll)
        if (!bValid || aKeep.find(rUrl) == aKeep.end())
        {
            // Mark first, so a half-deleted folder (a file still open on
            // Windows) is never used again.
            const OUString aMarker = appendSegment(rUrl, REMOVED_MARKER);
            if (!fileExists(aMarker))
                writeFile(aMarker, "", 0);
            if (!deleteTree(rUrl))
                SAL_INFO("linguistic", "language packs: could not delete " << rUrl << " yet");
        }
    // remove empty language folders
    for (const DirEntry& rLang : listDir(aUser))
        if (rLang.bDir && isPlausibleLanguageTag(rLang.aName) && listDir(rLang.aUrl).empty())
            osl::Directory::remove(rLang.aUrl);
}

bool IsDownloadEnabled()
{
    try
    {
        return officecfg::Office::Linguistic::DainvoLanguagePacks::AllowDownloads::get()
               && !officecfg::Office::Linguistic::DainvoLanguagePacks::IndexURL::get().isEmpty()
               && !officecfg::Office::Linguistic::DainvoLanguagePacks::PublicKey::get().isEmpty();
    }
    catch (const uno::Exception&)
    {
        return false;
    }
}

bool VerifyEd25519(const std::vector<char>& rData, const std::vector<unsigned char>& rSignature,
                   const std::vector<unsigned char>& rPublicKey)
{
    if (rSignature.size() != 64 || rPublicKey.size() != 32)
        return false;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pKey(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, rPublicKey.data(),
                                    rPublicKey.size()),
        EVP_PKEY_free);
    if (!pKey)
        return false;
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> pCtx(EVP_MD_CTX_new(),
                                                                 EVP_MD_CTX_free);
    if (!pCtx || EVP_DigestVerifyInit(pCtx.get(), nullptr, nullptr, nullptr, pKey.get()) != 1)
        return false;
    return EVP_DigestVerify(pCtx.get(), rSignature.data(), rSignature.size(),
                            reinterpret_cast<const unsigned char*>(rData.data()), rData.size())
           == 1;
}

std::optional<Index> ParseVerifiedIndex(const std::vector<char>& rIndex,
                                        const std::vector<char>& rSignature,
                                        const OUString& rPublicKeyBase64, OUString& rError)
{
    const std::vector<unsigned char> aKey = decodeBase64(rPublicKeyBase64);
    const std::vector<unsigned char> aSig = decodeBase64(
        OUString::fromUtf8(std::string_view(rSignature.data(), rSignature.size())));
    if (!VerifyEd25519(rIndex, aSig, aKey))
    {
        rError = u"The list of languages is not correctly signed."_ustr;
        return {};
    }
    auto oTree = parseJson(rIndex);
    if (!oTree)
    {
        rError = u"The list of languages could not be read."_ustr;
        return {};
    }
    try
    {
        Index aIndex;
        if (oTree->get<int>("schema", 0) != 1)
        {
            rError = u"The list of languages needs a newer version of Dainvo Office."_ustr;
            return {};
        }
        aIndex.nSequence = oTree->get<sal_Int64>("sequence");
        aIndex.nMinAppBuild = oTree->get<sal_Int64>("min_app_build", 0);
        for (const auto& rNode : oTree->get_child("components"))
        {
            const auto& r = rNode.second;
            IndexComponent aComp;
            aComp.aId = fromUtf8(rNode.first);
            aComp.aKind = fromUtf8(r.get<std::string>("kind"));
            aComp.aVersion = fromUtf8(r.get<std::string>("version"));
            aComp.aUrl = fromUtf8(r.get<std::string>("url"));
            aComp.aSha256 = fromUtf8(r.get<std::string>("sha256")).toAsciiLowerCase();
            aComp.aLicense = fromUtf8(r.get<std::string>("license", ""));
            aComp.nSize = r.get<sal_Int64>("size", 0);
            aComp.nInstalledSize = r.get<sal_Int64>("installed_size", 0);
            aComp.aLocales = stringArray(r, "locales");
            if (splitId(aComp.aId).second != aComp.aKind || !isSafeName(aComp.aVersion)
                || aComp.aSha256.getLength() != 64)
            {
                SAL_WARN("linguistic", "language packs: skipping bad index entry " << aComp.aId);
                continue;
            }
            aIndex.aComponents[aComp.aId] = std::move(aComp);
        }
        for (const auto& rNode : oTree->get_child("languages"))
        {
            const auto& r = rNode.second;
            IndexLanguage aLang;
            aLang.aTag = fromUtf8(r.get<std::string>("tag"));
            aLang.aName = fromUtf8(r.get<std::string>("name", ""));
            aLang.aAutonym = fromUtf8(r.get<std::string>("autonym", ""));
            for (const OUString& rId : stringArray(r, "components"))
                if (aIndex.aComponents.count(rId))
                    aLang.aComponents.push_back(rId);
            if (isPlausibleLanguageTag(aLang.aTag) && !aLang.aComponents.empty())
                aIndex.aLanguages.push_back(std::move(aLang));
        }
        return aIndex;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("linguistic", "language packs: bad index: " << e.what());
        rError = u"The list of languages could not be read."_ustr;
        return {};
    }
}

std::optional<Index> LoadCachedIndex()
{
    const OUString aUser = GetUserRootUrl();
    if (aUser.isEmpty())
        return {};
    std::vector<char> aIndex, aSig;
    if (!readFile(appendSegment(aUser, INDEX_FILE), MAX_INDEX_SIZE, aIndex)
        || !readFile(appendSegment(aUser, INDEX_SIG_FILE), MAX_SIG_SIZE, aSig))
        return {};
    OUString aError;
    auto oIndex = ParseVerifiedIndex(
        aIndex, aSig, officecfg::Office::Linguistic::DainvoLanguagePacks::PublicKey::get(), aError);
    if (oIndex)
        resolveRelativeUrls(*oIndex);
    return oIndex;
}

std::optional<Index> FetchIndex(OUString& rError, const ProgressFn& rProgress)
{
    if (!IsDownloadEnabled())
    {
        rError = u"Downloading languages is turned off."_ustr;
        return {};
    }
    const OUString aUrl = officecfg::Office::Linguistic::DainvoLanguagePacks::IndexURL::get();
    std::vector<char> aIndex, aSig;
    if (!fetchSmall(aUrl, MAX_INDEX_SIZE, aIndex, rError, rProgress)
        || !fetchSmall(aUrl + ".sig", MAX_SIG_SIZE, aSig, rError, rProgress))
        return {};
    auto oIndex = ParseVerifiedIndex(
        aIndex, aSig, officecfg::Office::Linguistic::DainvoLanguagePacks::PublicKey::get(), rError);
    if (!oIndex)
        return {};
    resolveRelativeUrls(*oIndex);

    const sal_Int64 nLast = officecfg::Office::Linguistic::DainvoLanguagePacks::LastSequence::get();
    if (oIndex->nSequence < nLast)
    {
        rError = u"The server offered an older list of languages than before."_ustr;
        return {};
    }
#ifdef DAINVO_BUILD
    if (oIndex->nMinAppBuild > DAINVO_BUILD)
    {
        rError = u"The list of languages needs a newer version of Dainvo Office."_ustr;
        return {};
    }
#endif
    if (oIndex->nSequence > nLast)
    {
        std::shared_ptr<comphelper::ConfigurationChanges> xChanges(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Linguistic::DainvoLanguagePacks::LastSequence::set(oIndex->nSequence,
                                                                             xChanges);
        xChanges->commit();
    }
    const OUString aUser = GetUserRootUrl();
    if (!aUser.isEmpty() && ensureDir(aUser))
    {
        writeFileAtomic(appendSegment(aUser, INDEX_FILE), aIndex);
        writeFileAtomic(appendSegment(aUser, INDEX_SIG_FILE), aSig);
    }
    return oIndex;
}

bool InstallComponentFromZip(const OUString& rZipUrl, const IndexComponent& rComponent,
                             OUString& rError)
{
    const auto [aLang, aKind] = splitId(rComponent.aId);
    if (aLang.isEmpty() || aKind != rComponent.aKind || !isSafeName(rComponent.aVersion))
    {
        rError = u"The language pack has an invalid name."_ustr;
        return false;
    }
    const OUString aUser = GetUserRootUrl();
    if (aUser.isEmpty())
    {
        rError = u"The user profile folder was not found."_ustr;
        return false;
    }
    const OUString aWork = appendSegment(sessionStaging(aUser), randomName());
    if (!ensureDir(aWork))
    {
        rError = "Could not create " + aWork;
        return false;
    }
    auto failed = [&](const OUString& rMsg) {
        rError = rMsg;
        deleteTree(aWork);
        return false;
    };

    // Unpack. Only flat names and licenses/<name> are accepted.
    const sal_Int64 nLimit = rComponent.nInstalledSize > 0
                                 ? rComponent.nInstalledSize + rComponent.nInstalledSize / 10 + 65536
                                 : sal_Int64(512) * 1024 * 1024;
    sal_Int64 nTotal = 0;
    try
    {
        uno::Reference<container::XNameAccess> xZip(packages::zip::ZipFileAccess::createWithURL(
            comphelper::getProcessComponentContext(), rZipUrl));
        for (const OUString& rName : xZip->getElementNames())
        {
            if (rName.endsWith("/"))
                continue; // folder entry
            OUString aTarget;
            if (rName.startsWith("licenses/"))
            {
                std::u16string_view aFile = rName.subView(9);
                if (!isSafeName(aFile))
                    return failed("The language pack contains an unexpected file: " + rName);
                const OUString aLicDir = appendSegment(aWork, u"licenses");
                ensureDir(aLicDir);
                aTarget = appendSegment(aLicDir, aFile);
            }
            else if (isSafeName(rName))
                aTarget = appendSegment(aWork, rName);
            else
                return failed("The language pack contains an unexpected file: " + rName);

            uno::Reference<io::XInputStream> xIn(xZip->getByName(rName), uno::UNO_QUERY_THROW);
            osl::File aOut(aTarget);
            if (aOut.open(osl_File_OpenFlag_Write | osl_File_OpenFlag_Create)
                != osl::FileBase::E_None)
                return failed("Could not write " + aTarget);
            uno::Sequence<sal_Int8> aBuf;
            for (;;)
            {
                sal_Int32 nRead = xIn->readBytes(aBuf, 65536);
                if (nRead <= 0)
                    break;
                nTotal += nRead;
                if (nTotal > nLimit)
                {
                    aOut.close();
                    return failed(u"The language pack is larger than expected."_ustr);
                }
                sal_uInt64 nWritten = 0;
                if (aOut.write(aBuf.getConstArray(), nRead, nWritten) != osl::FileBase::E_None
                    || nWritten != sal_uInt64(nRead))
                {
                    aOut.close();
                    return failed("Could not write " + aTarget);
                }
            }
            xIn->closeInput();
            if (aOut.close() != osl::FileBase::E_None)
                return failed("Could not write " + aTarget);
        }
    }
    catch (const uno::Exception& e)
    {
        return failed("The language pack could not be unpacked: " + e.Message);
    }

    // Check pack.json against the index entry.
    auto oComp = readComponent(aWork, false);
    if (!oComp || oComp->aId != rComponent.aId || oComp->aKind != rComponent.aKind
        || oComp->aVersion != rComponent.aVersion)
        return failed(u"The language pack does not match the list of languages."_ustr);

    // Move into place: <user>/<lang>/<kind>-<version>
    std::scoped_lock aGuard(installMutex());
    const OUString aLangDir = appendSegment(aUser, aLang);
    const OUString aTargetDir = appendSegment(aLangDir, OUString(aKind + "-" + rComponent.aVersion));
    if (!ensureDir(aLangDir))
        return failed("Could not create " + aLangDir);
    if (comphelper::DirectoryHelper::dirExists(aTargetDir))
    {
        // Same version again (for example after Remove without a restart).
        const OUString aMarker = appendSegment(aTargetDir, REMOVED_MARKER);
        if (fileExists(aMarker))
        {
            if (!deleteTree(aTargetDir))
            {
                // Still in use (Windows): keep using it instead.
                osl::File::remove(aMarker);
                deleteTree(aWork);
                return true;
            }
        }
        else
        {
            deleteTree(aWork);
            return true; // already installed
        }
    }
    if (osl::File::move(aWork, aTargetDir) != osl::FileBase::E_None)
        return failed("Could not move the language pack to " + aTargetDir);

    // Older versions of the same component are superseded; mark them.
    for (const DirEntry& rEntry : listDir(aLangDir))
        if (rEntry.bDir && rEntry.aUrl != aTargetDir && rEntry.aName.startsWith(OUString(aKind + "-")))
            writeFile(appendSegment(rEntry.aUrl, REMOVED_MARKER), "", 0);
    return true;
}

bool InstallComponent(const IndexComponent& rComponent, const ProgressFn& rProgress,
                      OUString& rError)
{
    if (!IsDownloadEnabled())
    {
        rError = u"Downloading languages is turned off."_ustr;
        return false;
    }
    const OUString aUser = GetUserRootUrl();
    if (aUser.isEmpty())
    {
        rError = u"The user profile folder was not found."_ustr;
        return false;
    }
    const OUString aStaging = sessionStaging(aUser);
    if (!ensureDir(aStaging))
    {
        rError = "Could not create " + aStaging;
        return false;
    }
    const OUString aZip = appendSegment(aStaging, OUString(randomName() + ".zip"));
    const sal_Int64 nMax
        = rComponent.nSize > 0 ? rComponent.nSize + 1024 : sal_Int64(256) * 1024 * 1024;

    comphelper::Hash aHash(comphelper::HashType::SHA256);
    if (rComponent.aUrl.startsWithIgnoreAsciiCase("file:"))
    {
        std::vector<char> aData;
        if (!readFile(rComponent.aUrl, nMax, aData))
        {
            rError = "Could not read " + rComponent.aUrl;
            return false;
        }
        aHash.update(aData.data(), aData.size());
        if (!writeFile(aZip, aData.data(), aData.size()))
        {
            rError = "Could not write " + aZip;
            return false;
        }
        if (rProgress)
            rProgress(aData.size(), aData.size());
    }
    else if (rComponent.aUrl.startsWithIgnoreAsciiCase("https:"))
    {
        auto pCurl = makeCurl(OUStringToOString(rComponent.aUrl, RTL_TEXTENCODING_UTF8), rError);
        if (!pCurl)
            return false;
        osl::File aFile(aZip);
        if (aFile.open(osl_File_OpenFlag_Write | osl_File_OpenFlag_Create)
            != osl::FileBase::E_None)
        {
            rError = "Could not write " + aZip;
            return false;
        }
        FileDownload aDl{ &aFile, &aHash, nMax };
        ProgressData aProgress{ &rProgress, rComponent.nSize };
        (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEFUNCTION, curlWriteToFile);
        (void)curl_easy_setopt(pCurl.get(), CURLOPT_WRITEDATA, &aDl);
        (void)curl_easy_setopt(pCurl.get(), CURLOPT_NOPROGRESS, 0L);
        (void)curl_easy_setopt(pCurl.get(), CURLOPT_XFERINFOFUNCTION, curlProgress);
        (void)curl_easy_setopt(pCurl.get(), CURLOPT_XFERINFODATA, &aProgress);
        CURLcode eCode = curl_easy_perform(pCurl.get());
        aFile.close();
        if (eCode != CURLE_OK)
        {
            osl::File::remove(aZip);
            if (eCode == CURLE_ABORTED_BY_CALLBACK)
                rError.clear(); // cancelled
            else if (aDl.bFailed)
                rError = u"The language pack is larger than expected."_ustr;
            else
                rError = curlError(pCurl.get(), eCode);
            return false;
        }
    }
    else
    {
        rError = u"Only https: and file: addresses are allowed."_ustr;
        return false;
    }

    const OUString aDigest
        = OUString::fromUtf8(comphelper::hashToString(aHash.finalize()));
    if (aDigest != rComponent.aSha256)
    {
        osl::File::remove(aZip);
        rError = u"The downloaded file is damaged (checksum mismatch). Try again."_ustr;
        return false;
    }
    bool bOk = InstallComponentFromZip(aZip, rComponent, rError);
    osl::File::remove(aZip);
    return bOk;
}

bool RemoveComponent(const OUString& rId, OUString& rError)
{
    const auto [aLang, aKind] = splitId(rId);
    const OUString aUser = GetUserRootUrl();
    if (aLang.isEmpty() || aUser.isEmpty())
    {
        rError = u"The language pack has an invalid name."_ustr;
        return false;
    }
    std::scoped_lock aGuard(installMutex());
    const OUString aLangDir = appendSegment(aUser, aLang);
    bool bFound = false;
    for (const DirEntry& rEntry : listDir(aLangDir))
        if (rEntry.bDir && rEntry.aName.startsWith(OUString(aKind + "-")))
        {
            bFound = true;
            if (!writeFile(appendSegment(rEntry.aUrl, REMOVED_MARKER), "", 0))
            {
                rError = "Could not remove " + rEntry.aUrl;
                return false;
            }
            deleteTree(rEntry.aUrl); // may fail on Windows
        }
    if (!bFound)
    {
        rError = u"This language pack is not installed."_ustr;
        return false;
    }
    if (listDir(aLangDir).empty())
        osl::Directory::remove(aLangDir);
    return true;
}

// ------------------------------------------------------------ auto download

namespace
{
struct AutoQueue
{
    std::mutex aMutex;
    std::condition_variable aCond;
    std::deque<std::pair<OUString, int>> aJobs;
    std::set<std::pair<OUString, int>> aSeen;
    std::thread aWorker;
    std::atomic<bool> bStop{ false };
    bool bBusy = false;
    std::optional<Index> oIndex;
    bool bIndexTried = false;
};

// Deliberately leaked: the worker is stopped by the terminate listener
// below, not by a static destructor running after UNO is gone.
AutoQueue& autoQueue()
{
    static AutoQueue* pQueue = new AutoQueue;
    return *pQueue;
}

void stopAutoWorker()
{
    AutoQueue& rQ = autoQueue();
    {
        std::scoped_lock aGuard(rQ.aMutex);
        rQ.bStop = true;
    }
    rQ.aCond.notify_all();
    if (rQ.aWorker.joinable() && rQ.aWorker.get_id() != std::this_thread::get_id())
        rQ.aWorker.join();
}

class AutoTerminateListener : public cppu::WeakImplHelper<frame::XTerminateListener>
{
public:
    void SAL_CALL queryTermination(const lang::EventObject&) override {}
    void SAL_CALL notifyTermination(const lang::EventObject&) override { stopAutoWorker(); }
    void SAL_CALL disposing(const lang::EventObject&) override {}
};

/// The index row for a language: exact tag, else the bare language.
const IndexLanguage* findLanguage(const Index& rIndex, const OUString& rTag)
{
    for (const IndexLanguage& rLang : rIndex.aLanguages)
        if (rLang.aTag == rTag)
            return &rLang;
    const sal_Int32 nDash = rTag.indexOf('-');
    if (nDash > 0)
    {
        const OUString aBase = rTag.copy(0, nDash);
        for (const IndexLanguage& rLang : rIndex.aLanguages)
            if (rLang.aTag == aBase)
                return &rLang;
    }
    return nullptr;
}

bool isServed(const OUString& rTag, std::u16string_view aKind)
{
    for (const InstalledComponent& rComp : GetInstalledComponents())
        if (rComp.aKind == aKind)
            for (const PackDictionary& rDict : rComp.aDictionaries)
                if (std::find(rDict.aLocales.begin(), rDict.aLocales.end(), rTag)
                    != rDict.aLocales.end())
                    return true;
    return false;
}

/// Download what the language needs. Returns true if anything was installed.
bool runAutoJob(const OUString& rTag, int nKinds)
{
    AutoQueue& rQ = autoQueue();
    auto keepGoing = [&rQ](sal_Int64, sal_Int64) { return !rQ.bStop; };
    if (!rQ.oIndex && !rQ.bIndexTried)
    {
        rQ.bIndexTried = true;
        OUString aError;
        rQ.oIndex = FetchIndex(aError, keepGoing);
        if (!rQ.oIndex)
        {
            SAL_INFO("linguistic", "language packs: no index for auto download: " << aError);
            rQ.oIndex = LoadCachedIndex();
        }
    }
    if (!rQ.oIndex)
        return false;
    const IndexLanguage* pLang = findLanguage(*rQ.oIndex, rTag);
    if (!pLang)
        return false;
    bool bInstalled = false;
    for (const OUString& rId : pLang->aComponents)
    {
        const IndexComponent& rComp = rQ.oIndex->aComponents.at(rId);
        const int nKind = rComp.aKind == u"spell" ? AUTO_SPELL
                          : rComp.aKind == u"hyph" ? AUTO_HYPH
                                                   : AUTO_THES;
        if (!(nKinds & nKind) || rQ.bStop || isServed(pLang->aTag, rComp.aKind))
            continue;
        OUString aError;
        if (InstallComponent(rComp, keepGoing, aError))
        {
            SAL_INFO("linguistic", "language packs: installed " << rId << " automatically");
            bInstalled = true;
        }
        else
            SAL_WARN_IF(!aError.isEmpty(), "linguistic",
                        "language packs: auto download of " << rId << " failed: " << aError);
    }
    return bInstalled;
}

struct AutoReload
{
    DECL_STATIC_LINK(AutoReload, Reload, void*, void);
};

IMPL_STATIC_LINK_NOARG(AutoReload, Reload, void*, void)
{
    if (!autoQueue().bStop)
        ReloadLinguServices();
}

void autoWorker()
{
    osl_setThreadName("DainvoLanguagesAuto");
    AutoQueue& rQ = autoQueue();
    for (;;)
    {
        std::pair<OUString, int> aJob;
        {
            std::unique_lock aGuard(rQ.aMutex);
            rQ.aCond.wait(aGuard, [&rQ] { return rQ.bStop || !rQ.aJobs.empty(); });
            if (rQ.bStop)
                return;
            aJob = rQ.aJobs.front();
            rQ.aJobs.pop_front();
            rQ.bBusy = true;
        }
        const bool bInstalled = runAutoJob(aJob.first, aJob.second);
        if (bInstalled && !rQ.bStop)
            Application::PostUserEvent(LINK(nullptr, AutoReload, Reload));
        {
            std::scoped_lock aGuard(rQ.aMutex);
            rQ.bBusy = false;
        }
        rQ.aCond.notify_all();
    }
}
}

void RequestAutoDownload(const lang::Locale& rLocale, int nKinds)
{
    if (LinguIsUnspecified(LinguLocaleToLanguage(rLocale)))
        return;
    const OUString aTag = LanguageTag::convertToBcp47(rLocale, false);
    if (aTag.isEmpty())
        return;
    AutoQueue& rQ = autoQueue();
    {
        std::scoped_lock aGuard(rQ.aMutex);
        if (rQ.bStop || !rQ.aSeen.insert({ aTag, nKinds }).second)
            return; // once per language, parts and session
    }
    try
    {
        if (!officecfg::Office::Linguistic::DainvoLanguagePacks::AutoDownload::get()
            || !IsDownloadEnabled())
            return;
        // Only interactive sessions download on their own: headless
        // conversions and unit tests never reach the network.
        if ((Application::IsHeadlessModeEnabled() || getenv("LO_RUNNING_UNIT_TEST"))
            && !officecfg::Office::Linguistic::DainvoLanguagePacks::IndexURL::get()
                    .startsWithIgnoreAsciiCase("file:"))
            return;
    }
    catch (const uno::Exception&)
    {
        return;
    }
    bool bStart = false;
    {
        std::scoped_lock aGuard(rQ.aMutex);
        rQ.aJobs.emplace_back(aTag, nKinds);
        bStart = !rQ.aWorker.joinable();
        if (bStart)
            rQ.aWorker = std::thread(autoWorker);
    }
    rQ.aCond.notify_all();
    if (bStart)
    {
        try
        {
            frame::Desktop::create(comphelper::getProcessComponentContext())
                ->addTerminateListener(new AutoTerminateListener);
        }
        catch (const uno::Exception&)
        {
        }
    }
}

void WaitForAutoDownloads()
{
    AutoQueue& rQ = autoQueue();
    for (;;)
    {
        {
            std::unique_lock aGuard(rQ.aMutex);
            if (rQ.bStop || (rQ.aJobs.empty() && !rQ.bBusy))
                return;
            rQ.aCond.wait_for(aGuard, std::chrono::milliseconds(50));
        }
        // let the worker post its reload to the main thread
        if (Application::IsMainThread())
            Application::Reschedule(true);
    }
}

void ReloadLinguServices()
{
    try
    {
        uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext());
        for (const OUString& rName :
             { u"org.openoffice.lingu.MySpellSpellChecker"_ustr,
               u"org.openoffice.lingu.LibHnjHyphenator"_ustr,
               u"org.openoffice.lingu.new.Thesaurus"_ustr })
        {
            try
            {
                uno::Reference<util::XRefreshable> xRefresh(
                    xContext->getServiceManager()->createInstanceWithContext(rName, xContext),
                    uno::UNO_QUERY);
                if (xRefresh.is())
                    xRefresh->refresh();
            }
            catch (const uno::Exception&)
            {
            }
        }
        Cleanup(); // the services no longer hold old files open
        // Same path as installing a dictionary extension: drop the cached
        // service lists, rescan on idle and ask documents to check again.
        uno::Reference<util::XModifyListener> xMgr(
            linguistic2::LinguServiceManager::create(xContext), uno::UNO_QUERY);
        if (xMgr.is())
            xMgr->modified(lang::EventObject());
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("linguistic", "language packs: reload failed: " << e.Message);
    }
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
