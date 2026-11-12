/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Tests for Dainvo Office language packs (include/linguistic/dainvolangpacks.hxx).

#include <sal/config.h>

#include <test/bootstrapfixture.hxx>

#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/linguistic2/LinguServiceManager.hpp>
#include <com/sun/star/linguistic2/XHyphenatedWord.hpp>
#include <com/sun/star/linguistic2/XHyphenator.hpp>
#include <com/sun/star/util/XRefreshable.hpp>

#include <comphelper/processfactory.hxx>
#include <linguistic/dainvolangpacks.hxx>
#include <officecfg/Office/Linguistic.hxx>
#include <osl/file.hxx>
#include <unotools/lingucfg.hxx>

#include <algorithm>
#include <cstdlib>

using namespace css;
namespace lp = linguistic::langpacks;

namespace
{
// RFC 8032 section 7.1, test 1 (empty message)
const unsigned char RFC_PUBLIC_KEY[32]
    = { 0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a };
const unsigned char RFC_SIGNATURE[64]
    = { 0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72, 0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
        0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74, 0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
        0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac, 0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
        0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24, 0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b };
// the same public key, base64
constexpr OUString TEST_KEY_BASE64 = u"11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo="_ustr;

class LangPacksTest : public test::BootstrapFixture
{
public:
    void setUp() override
    {
        test::BootstrapFixture::setUp();
        // start every test with an empty per-user root
        osl::Directory::createPath(lp::GetUserRootUrl());
        removeTree(lp::GetUserRootUrl());
    }

protected:
    OUString dataUrl(std::u16string_view aName)
    {
        return m_directories.getURLFromSrc(u"/linguistic/qa/data/langpacks/") + aName;
    }

    std::vector<char> readData(std::u16string_view aName)
    {
        std::vector<char> aData;
        osl::File aFile(dataUrl(aName));
        CPPUNIT_ASSERT_EQUAL(osl::FileBase::E_None, aFile.open(osl_File_OpenFlag_Read));
        sal_uInt64 nSize = 0;
        aFile.getSize(nSize);
        aData.resize(nSize);
        sal_uInt64 nRead = 0;
        aFile.read(aData.data(), nSize, nRead);
        return aData;
    }

    lp::Index loadTestIndex()
    {
        OUString aError;
        auto oIndex = lp::ParseVerifiedIndex(readData(u"index.json"), readData(u"index.json.sig"),
                                             TEST_KEY_BASE64, aError);
        CPPUNIT_ASSERT_MESSAGE(aError.toUtf8().getStr(), oIndex.has_value());
        return *oIndex;
    }

    static void removeTree(const OUString& rUrl)
    {
        osl::Directory aDir(rUrl);
        if (aDir.open() != osl::FileBase::E_None)
            return;
        osl::DirectoryItem aItem;
        while (aDir.getNextItem(aItem) == osl::FileBase::E_None)
        {
            osl::FileStatus aStatus(osl_FileStatus_Mask_Type | osl_FileStatus_Mask_FileURL);
            aItem.getFileStatus(aStatus);
            if (aStatus.isDirectory())
            {
                removeTree(aStatus.getFileURL());
                osl::Directory::remove(aStatus.getFileURL());
            }
            else
                osl::File::remove(aStatus.getFileURL());
        }
    }

    static bool hasComponent(std::u16string_view aId, bool bBundled)
    {
        auto aComps = lp::GetInstalledComponents();
        return std::any_of(aComps.begin(), aComps.end(), [&](const lp::InstalledComponent& r) {
            return r.aId == aId && r.bBundled == bBundled;
        });
    }

    static uno::Reference<linguistic2::XHyphenator> hyphenator()
    {
        uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext());
        return uno::Reference<linguistic2::XHyphenator>(
            xContext->getServiceManager()->createInstanceWithContext(
                u"org.openoffice.lingu.LibHnjHyphenator"_ustr, xContext),
            uno::UNO_QUERY_THROW);
    }
};

CPPUNIT_TEST_FIXTURE(LangPacksTest, testEd25519)
{
    std::vector<unsigned char> aKey(std::begin(RFC_PUBLIC_KEY), std::end(RFC_PUBLIC_KEY));
    std::vector<unsigned char> aSig(std::begin(RFC_SIGNATURE), std::end(RFC_SIGNATURE));
    CPPUNIT_ASSERT(lp::VerifyEd25519({}, aSig, aKey));
    CPPUNIT_ASSERT(!lp::VerifyEd25519({ 'x' }, aSig, aKey));
    aSig[10] ^= 1;
    CPPUNIT_ASSERT(!lp::VerifyEd25519({}, aSig, aKey));
    CPPUNIT_ASSERT(!lp::VerifyEd25519({}, {}, aKey));
}

CPPUNIT_TEST_FIXTURE(LangPacksTest, testIndex)
{
    lp::Index aIndex = loadTestIndex();
    CPPUNIT_ASSERT_EQUAL(sal_Int64(5), aIndex.nSequence);
    CPPUNIT_ASSERT_EQUAL(size_t(1), aIndex.aLanguages.size());
    CPPUNIT_ASSERT_EQUAL(u"qaa"_ustr, aIndex.aLanguages[0].aTag);
    const lp::IndexComponent& rComp = aIndex.aComponents.at(u"qaa.hyph"_ustr);
    CPPUNIT_ASSERT_EQUAL(u"hyph"_ustr, rComp.aKind);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(64), rComp.aSha256.getLength());

    // a changed index or another key must be refused
    OUString aError;
    std::vector<char> aChanged = readData(u"index.json");
    aChanged[aChanged.size() / 2] ^= 1;
    CPPUNIT_ASSERT(
        !lp::ParseVerifiedIndex(aChanged, readData(u"index.json.sig"), TEST_KEY_BASE64, aError));
    CPPUNIT_ASSERT(!lp::ParseVerifiedIndex(
        readData(u"index.json"), readData(u"index.json.sig"),
        officecfg::Office::Linguistic::DainvoLanguagePacks::PublicKey::get(), aError));
}

CPPUNIT_TEST_FIXTURE(LangPacksTest, testBundledEnglish)
{
    CPPUNIT_ASSERT(hasComponent(u"en-US.spell", true));
    CPPUNIT_ASSERT(hasComponent(u"en-US.hyph", true));
    CPPUNIT_ASSERT(hasComponent(u"en-GB.hyph", true));

    auto aHyph = lp::GetDictionaryEntries(u"DICT_HYPH");
    auto it = std::find_if(aHyph.begin(), aHyph.end(), [](const SvtLinguConfigDictionaryEntry& r) {
        return std::find(r.aLocaleNames.begin(), r.aLocaleNames.end(), u"en-AU")
               != r.aLocaleNames.end();
    });
    CPPUNIT_ASSERT(it != aHyph.end());

    uno::Reference<linguistic2::XHyphenator> xHyph = hyphenator();
    CPPUNIT_ASSERT(xHyph->hasLocale(lang::Locale(u"en"_ustr, u"US"_ustr, u""_ustr)));
    uno::Reference<linguistic2::XHyphenatedWord> xWord = xHyph->hyphenate(
        u"hyphenation"_ustr, lang::Locale(u"en"_ustr, u"US"_ustr, u""_ustr), 9, {});
    CPPUNIT_ASSERT(xWord.is());
}

CPPUNIT_TEST_FIXTURE(LangPacksTest, testInstallReloadRemove)
{
    uno::Reference<linguistic2::XHyphenator> xHyph = hyphenator();
    const lang::Locale aQaa(u"qaa"_ustr, u""_ustr, u""_ustr);
    CPPUNIT_ASSERT(!xHyph->hasLocale(aQaa));

    lp::IndexComponent aComp = loadTestIndex().aComponents.at(u"qaa.hyph"_ustr);
    aComp.aUrl = dataUrl(u"qaa-hyph.zip");
    OUString aError;
    CPPUNIT_ASSERT_MESSAGE(aError.toUtf8().getStr(), lp::InstallComponent(aComp, {}, aError));
    CPPUNIT_ASSERT(hasComponent(u"qaa.hyph", false));
    // installing the same version again is fine
    CPPUNIT_ASSERT(lp::InstallComponent(aComp, {}, aError));

    // the hyphenator sees it after refresh(), without a restart
    uno::Reference<util::XRefreshable>(xHyph, uno::UNO_QUERY_THROW)->refresh();
    CPPUNIT_ASSERT(xHyph->hasLocale(aQaa));
    uno::Reference<linguistic2::XHyphenatedWord> xWord
        = xHyph->hyphenate(u"aaabbb"_ustr, aQaa, 5, {});
    CPPUNIT_ASSERT(xWord.is());
    CPPUNIT_ASSERT_EQUAL(sal_Int16(2), xWord->getHyphenationPos());

    CPPUNIT_ASSERT(lp::RemoveComponent(u"qaa.hyph"_ustr, aError));
    CPPUNIT_ASSERT(!hasComponent(u"qaa.hyph", false));
    uno::Reference<util::XRefreshable>(xHyph, uno::UNO_QUERY_THROW)->refresh();
    CPPUNIT_ASSERT(!xHyph->hasLocale(aQaa));
    CPPUNIT_ASSERT(!lp::RemoveComponent(u"qaa.hyph"_ustr, aError));
}

CPPUNIT_TEST_FIXTURE(LangPacksTest, testBadDownloads)
{
    lp::IndexComponent aComp = loadTestIndex().aComponents.at(u"qaa.hyph"_ustr);
    OUString aError;

    // wrong checksum
    aComp.aUrl = dataUrl(u"qaa-hyph.zip");
    aComp.aSha256 = OUString::Concat(aComp.aSha256.subView(0, 63)) + "0";
    if (aComp.aSha256 == loadTestIndex().aComponents.at(u"qaa.hyph"_ustr).aSha256)
        aComp.aSha256 = OUString::Concat(aComp.aSha256.subView(0, 63)) + "1";
    CPPUNIT_ASSERT(!lp::InstallComponent(aComp, {}, aError));
    CPPUNIT_ASSERT(!hasComponent(u"qaa.hyph", false));

    // a zip with "../" in an entry name is refused and leaves nothing behind
    lp::IndexComponent aEvil = aComp;
    aEvil.aId = u"qab.hyph"_ustr;
    CPPUNIT_ASSERT(!lp::InstallComponentFromZip(dataUrl(u"evil-hyph.zip"), aEvil, aError));
    CPPUNIT_ASSERT(!hasComponent(u"qab.hyph", false));
    osl::DirectoryItem aItem;
    CPPUNIT_ASSERT(osl::DirectoryItem::get(lp::GetUserRootUrl() + "/evil.dic", aItem)
                   != osl::FileBase::E_None);

    // a zip whose pack.json does not match the index entry is refused
    lp::IndexComponent aOther = aComp;
    aOther.aId = u"qac.hyph"_ustr;
    CPPUNIT_ASSERT(!lp::InstallComponentFromZip(dataUrl(u"qaa-hyph.zip"), aOther, aError));
    CPPUNIT_ASSERT(!hasComponent(u"qac.hyph", false));

    // only https: and file: are allowed
    aComp.aUrl = u"http://example.com/qaa-hyph.zip"_ustr;
    CPPUNIT_ASSERT(!lp::InstallComponent(aComp, {}, aError));
}

// Network test against the published index; runs only with
// DAINVO_LANGPACKS_LIVE_TEST=1 so normal builds stay offline.
CPPUNIT_TEST_FIXTURE(LangPacksTest, testLiveIndex)
{
    if (!getenv("DAINVO_LANGPACKS_LIVE_TEST"))
        return;
    OUString aError;
    auto oIndex = lp::FetchIndex(aError);
    CPPUNIT_ASSERT_MESSAGE(aError.toUtf8().getStr(), oIndex.has_value());
    CPPUNIT_ASSERT(oIndex->nSequence >= 1);
    const lp::IndexComponent& rComp = oIndex->aComponents.at(u"en-ZA.spell"_ustr);
    CPPUNIT_ASSERT(rComp.aUrl.startsWith("https://"));
    sal_Int64 nLastDone = 0;
    CPPUNIT_ASSERT_MESSAGE(aError.toUtf8().getStr(),
                           lp::InstallComponent(
                               rComp,
                               [&nLastDone](sal_Int64 nDone, sal_Int64) {
                                   nLastDone = nDone;
                                   return true;
                               },
                               aError));
    CPPUNIT_ASSERT_EQUAL(rComp.nSize, nLastDone);
    CPPUNIT_ASSERT(hasComponent(u"en-ZA.spell", false));
    CPPUNIT_ASSERT(lp::RemoveComponent(u"en-ZA.spell"_ustr, aError));
}
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
