/* This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/util/XFlushable.hpp>
#include <comphelper/processfactory.hxx>
#include <unotools/configmgr.hxx>
#include <unotools/moduleoptions.hxx>

namespace sfx2
{
inline void SetDainvoSaveFormats(bool bMicrosoft)
{
    using Module = SvtModuleOptions::EModule;
    using Factory = SvtModuleOptions::EFactory;
    SvtModuleOptions aOptions;
    auto set = [&](Module eModule, Factory eFactory, const OUString& rMicrosoft,
                   const OUString& rOpenDocument) {
        if (aOptions.IsModuleInstalled(eModule) && !aOptions.IsDefaultFilterReadonly(eFactory))
            aOptions.SetFactoryDefaultFilter(eFactory, bMicrosoft ? rMicrosoft : rOpenDocument);
    };
    set(Module::WRITER, Factory::WRITER, u"MS Word 2007 XML"_ustr, u"writer8"_ustr);
    set(Module::CALC, Factory::CALC, u"Calc MS Excel 2007 XML"_ustr, u"calc8"_ustr);
    set(Module::IMPRESS, Factory::IMPRESS, u"Impress MS PowerPoint 2007 XML"_ustr, u"impress8"_ustr);
    utl::ConfigManager::storeConfigItems();
    const css::uno::Reference<css::util::XFlushable> xProvider(
        css::configuration::theDefaultProvider::get(comphelper::getProcessComponentContext()),
        css::uno::UNO_QUERY);
    if (xProvider)
        xProvider->flush();
}
}
