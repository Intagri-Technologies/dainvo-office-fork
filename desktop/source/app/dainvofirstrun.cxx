/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "dainvofirstrun.hxx"

#include <dp_shared.hxx>
#include <strings.hrc>

#include <comphelper/configuration.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <officecfg/Office/Commands.hxx>
#include <officecfg/Office/Common.hxx>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/lang/XSingleServiceFactory.hpp>
#include <unotools/configmgr.hxx>
#include <unotools/moduleoptions.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/MessageDialog.hxx>

#include <memory>

namespace desktop
{
namespace
{
constexpr int RESPONSE_MSO = 100;
constexpr int RESPONSE_ODF = 101;

void SetMicrosoftOfficeDefaults()
{
    SvtModuleOptions aModuleOpt;
    aModuleOpt.SetFactoryDefaultFilter(SvtModuleOptions::EFactory::WRITER,
                                       u"MS Word 2007 XML"_ustr);
    aModuleOpt.SetFactoryDefaultFilter(SvtModuleOptions::EFactory::CALC,
                                       u"Calc MS Excel 2007 XML"_ustr);
    aModuleOpt.SetFactoryDefaultFilter(SvtModuleOptions::EFactory::IMPRESS,
                                       u"Impress MS PowerPoint 2007 XML"_ustr);
}
}

void DainvoAskDefaultFileFormats()
{
    if (Application::IsHeadlessModeEnabled()
        || officecfg::Office::Common::Misc::DainvoAskedDefaultFormat::get()
        || officecfg::Office::Common::Misc::DainvoAskedDefaultFormat::isReadOnly())
        return;

    try
    {
        // Parent it to the app's current window (the Start Center) so it is a
        // proper modal dialog of the app
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(
            Application::GetDefDialogParent(), VclMessageType::Question, VclButtonsType::NONE,
            DpResId(STR_DAINVO_FORMAT_PRIMARY)
                .replaceAll("%PRODUCTNAME", utl::ConfigManager::getProductName())));
        xBox->set_title(DpResId(STR_DAINVO_FORMAT_TITLE));
#ifdef MACOSX
        // On macOS the options window is "Settings" in the application menu
        xBox->set_secondary_text(DpResId(STR_DAINVO_FORMAT_SECONDARY_MAC)
                                     .replaceAll("%PRODUCTNAME", utl::ConfigManager::getProductName()));
#else
        xBox->set_secondary_text(DpResId(STR_DAINVO_FORMAT_SECONDARY));
#endif
        xBox->add_button(DpResId(STR_DAINVO_FORMAT_ODF), RESPONSE_ODF);
        xBox->add_button(DpResId(STR_DAINVO_FORMAT_MSO), RESPONSE_MSO);
        xBox->set_default_response(RESPONSE_MSO);
        // Name the buttons for screen readers (VoiceOver)
        for (const auto& [nResponse, pLabel] :
             { std::pair{ RESPONSE_ODF, STR_DAINVO_FORMAT_ODF },
               std::pair{ RESPONSE_MSO, STR_DAINVO_FORMAT_MSO } })
        {
            if (std::unique_ptr<weld::Button> xButton = xBox->weld_button_for_response(nResponse))
                xButton->set_accessible_name(DpResId(pLabel));
        }

        const int nResponse = xBox->run();
        // Close the dialog window right away, before any other work
        xBox->hide();
        xBox.reset();
        // Closed without choosing (or the app was quitting): ask again next time
        if (nResponse != RESPONSE_MSO && nResponse != RESPONSE_ODF)
            return;
        if (nResponse == RESPONSE_MSO)
            SetMicrosoftOfficeDefaults();

        std::shared_ptr<comphelper::ConfigurationChanges> batch(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Common::Misc::DainvoAskedDefaultFormat::set(true, batch);
        batch->commit();
    }
    catch (const css::uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("desktop.app", "DainvoAskDefaultFileFormats");
    }
}

void DainvoApplyMacroSetting()
{
    // Commands behind Tools > Macros; hidden from menus, toolbars and
    // shortcuts while macros are off.
    static constexpr OUString aMacroCommands[]
        = { u"MacrosMenu"_ustr,     u"MacroManager"_ustr,    u"MacroRecorder"_ustr,
            u"StopRecording"_ustr,  u"RunMacro"_ustr,        u"MacroDialog"_ustr,
            u"BasicIDEAppear"_ustr, u"ScriptOrganizer"_ustr, u"MacroSignature"_ustr,
            u"MacroOrganizer"_ustr, u"TableEvents"_ustr };

    try
    {
        const bool bEnabled = officecfg::Office::Common::Misc::DainvoMacrosEnabled::get();
        std::shared_ptr<comphelper::ConfigurationChanges> batch(
            comphelper::ConfigurationChanges::create());
        bool bChanged = false;

        css::uno::Reference<css::container::XNameContainer> xDisabled(
            officecfg::Office::Commands::Execute::Disabled::get(batch), css::uno::UNO_SET_THROW);
        css::uno::Reference<css::lang::XSingleServiceFactory> xFactory(xDisabled,
                                                                       css::uno::UNO_QUERY_THROW);
        for (const OUString& rCommand : aMacroCommands)
        {
            const OUString aNode = "DainvoMacro" + rCommand;
            const bool bListed = xDisabled->hasByName(aNode);
            if (!bEnabled && !bListed)
            {
                css::uno::Reference<css::beans::XPropertySet> xEntry(xFactory->createInstance(),
                                                                     css::uno::UNO_QUERY_THROW);
                xEntry->setPropertyValue(u"Command"_ustr, css::uno::Any(rCommand));
                xDisabled->insertByName(aNode, css::uno::Any(xEntry));
                bChanged = true;
            }
            else if (bEnabled && bListed)
            {
                xDisabled->removeByName(aNode);
                bChanged = true;
            }
        }

        if (officecfg::Office::Common::Security::Scripting::DisableMacrosExecution::get()
                == bEnabled
            && !officecfg::Office::Common::Security::Scripting::DisableMacrosExecution::isReadOnly())
        {
            officecfg::Office::Common::Security::Scripting::DisableMacrosExecution::set(!bEnabled,
                                                                                        batch);
            bChanged = true;
        }

        if (bChanged)
            batch->commit();
    }
    catch (const css::uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("desktop.app", "DainvoApplyMacroSetting");
    }
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
