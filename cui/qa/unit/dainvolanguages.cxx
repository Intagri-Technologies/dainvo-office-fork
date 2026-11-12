/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dainvo Office Languages dialog, driven headless against a local signed
// index (linguistic/qa/data/langpacks): fetch, install, reload, remove.

#include <sal/config.h>

#include <test/bootstrapfixture.hxx>

#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/linguistic2/XHyphenator.hpp>

#include <comphelper/configuration.hxx>
#include <comphelper/processfactory.hxx>
#include <dainvolanguages.hxx>
#include <officecfg/Office/Linguistic.hxx>
#include <osl/thread.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld/Window.hxx>

#include <chrono>

using namespace css;

struct DainvoLanguagesDialogTestAccess
{
    static void waitForJob(DainvoLanguagesDialog& rDlg)
    {
        const auto aEnd = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (rDlg.m_pJob && std::chrono::steady_clock::now() < aEnd)
        {
            Application::Reschedule(true);
            osl::Thread::wait(std::chrono::milliseconds(20));
        }
        CPPUNIT_ASSERT_MESSAGE("job did not finish", !rDlg.m_pJob);
    }
    static bool hasRow(DainvoLanguagesDialog& rDlg, std::u16string_view aTag)
    {
        return rDlg.m_xList->find_id(OUString(aTag)) >= 0;
    }
    static void setPart(DainvoLanguagesDialog& rDlg, std::u16string_view aTag, int nKind, bool bOn)
    {
        rDlg.m_xList->select_id(OUString(aTag));
        rDlg.UpdateChecks();
        CPPUNIT_ASSERT(rDlg.m_aChecks[nKind]->get_sensitive());
        rDlg.m_aChecks[nKind]->set_active(bOn);
        rDlg.ToggleHdl(*rDlg.m_aChecks[nKind]);
    }
    static void apply(DainvoLanguagesDialog& rDlg)
    {
        CPPUNIT_ASSERT(rDlg.m_xApply->get_sensitive());
        rDlg.ApplyHdl(*rDlg.m_xApply);
        waitForJob(rDlg);
    }
    static OUString status(DainvoLanguagesDialog& rDlg) { return rDlg.m_xStatus->get_label(); }
    static OUString rowStatus(DainvoLanguagesDialog& rDlg, std::u16string_view aTag)
    {
        return rDlg.m_xList->get_text(rDlg.m_xList->find_id(OUString(aTag)), 1);
    }
};

namespace
{
using Access = DainvoLanguagesDialogTestAccess;

class DainvoLanguagesDialogTest : public test::BootstrapFixture
{
public:
    void setUp() override
    {
        test::BootstrapFixture::setUp();
        std::shared_ptr<comphelper::ConfigurationChanges> xChanges(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Linguistic::DainvoLanguagePacks::IndexURL::set(
            m_directories.getURLFromSrc(u"/linguistic/qa/data/langpacks/index.json"), xChanges);
        // RFC 8032 test key, which signed the test index
        officecfg::Office::Linguistic::DainvoLanguagePacks::PublicKey::set(
            u"11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo="_ustr, xChanges);
        officecfg::Office::Linguistic::DainvoLanguagePacks::LastSequence::set(0, xChanges);
        xChanges->commit();
    }
};

uno::Reference<linguistic2::XHyphenator> hyphenator()
{
    uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext());
    return uno::Reference<linguistic2::XHyphenator>(
        xContext->getServiceManager()->createInstanceWithContext(
            u"org.openoffice.lingu.LibHnjHyphenator"_ustr, xContext),
        uno::UNO_QUERY_THROW);
}

CPPUNIT_TEST_FIXTURE(DainvoLanguagesDialogTest, testInstallAndRemove)
{
    const lang::Locale aQaa(u"qaa"_ustr, u""_ustr, u""_ustr);
    {
        DainvoLanguagesDialog aDlg(nullptr, u"Dictionary"_ustr);
        Access::waitForJob(aDlg); // index fetch
        CPPUNIT_ASSERT_EQUAL(OUString(), Access::status(aDlg));
        CPPUNIT_ASSERT(Access::hasRow(aDlg, u"qaa"));
        CPPUNIT_ASSERT(Access::hasRow(aDlg, u"en-US")); // bundled
        CPPUNIT_ASSERT(!hyphenator()->hasLocale(aQaa));

        Access::setPart(aDlg, u"qaa", 1, true); // hyphenation
        Access::apply(aDlg);
        CPPUNIT_ASSERT_EQUAL(u"Done. The changes are already in use."_ustr, Access::status(aDlg));
        // in use straight away, without a restart
        CPPUNIT_ASSERT(hyphenator()->hasLocale(aQaa));
        CPPUNIT_ASSERT_EQUAL(u"Hyphenation"_ustr, Access::rowStatus(aDlg, u"qaa"));
        CPPUNIT_ASSERT_EQUAL(sal_Int64(5),
                             officecfg::Office::Linguistic::DainvoLanguagePacks::LastSequence::get());

        Access::setPart(aDlg, u"qaa", 1, false);
        Access::apply(aDlg);
        CPPUNIT_ASSERT(!hyphenator()->hasLocale(aQaa));
        CPPUNIT_ASSERT_EQUAL(u"Not installed"_ustr, Access::rowStatus(aDlg, u"qaa"));
    }
    // an older index than one already seen is refused
    {
        std::shared_ptr<comphelper::ConfigurationChanges> xChanges(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Linguistic::DainvoLanguagePacks::LastSequence::set(6, xChanges);
        xChanges->commit();
        DainvoLanguagesDialog aDlg(nullptr, u"Dictionary"_ustr);
        Access::waitForJob(aDlg);
        CPPUNIT_ASSERT(Access::status(aDlg).indexOf("older") >= 0);
    }
}
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
