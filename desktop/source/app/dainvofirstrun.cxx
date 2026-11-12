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
#include <officecfg/Office/Common.hxx>
#include <unotools/configmgr.hxx>
#include <unotools/moduleoptions.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclenum.hxx>
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
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(
            nullptr, VclMessageType::Question, VclButtonsType::NONE,
            DpResId(STR_DAINVO_FORMAT_PRIMARY)
                .replaceAll("%PRODUCTNAME", utl::ConfigManager::getProductName())));
        xBox->set_title(DpResId(STR_DAINVO_FORMAT_TITLE));
        xBox->set_secondary_text(DpResId(STR_DAINVO_FORMAT_SECONDARY));
        xBox->add_button(DpResId(STR_DAINVO_FORMAT_ODF), RESPONSE_ODF);
        xBox->add_button(DpResId(STR_DAINVO_FORMAT_MSO), RESPONSE_MSO);
        xBox->set_default_response(RESPONSE_MSO);

        const int nResponse = xBox->run();
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
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
