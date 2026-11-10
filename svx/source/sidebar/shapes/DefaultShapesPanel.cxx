/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#include <DefaultShapesPanel.hxx>

#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <comphelper/dispatchcommand.hxx>
#include <utility>
#include <vcl/bitmap.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <vcl/image.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld/Builder.hxx>

namespace svx::sidebar {

DefaultShapesPanel::DefaultShapesPanel(weld::Widget* pParent,
                                       css::uno::Reference<css::frame::XFrame> xFrame)
    : PanelLayout(pParent, u"DefaultShapesPanel"_ustr, u"svx/ui/defaultshapespanel.ui"_ustr)
    , m_pLineArrowIconView(m_xBuilder->weld_icon_view(u"LinesArrows"_ustr))
    , m_pCurveIconView(m_xBuilder->weld_icon_view(u"Curves"_ustr))
    , m_pConnectorIconView(m_xBuilder->weld_icon_view(u"Connectors"_ustr))
    , m_pBasicShapeIconView(m_xBuilder->weld_icon_view(u"BasicShapes"_ustr))
    , m_pSymbolShapeIconView(m_xBuilder->weld_icon_view(u"SymbolShapes"_ustr))
    , m_pBlockArrowIconView(m_xBuilder->weld_icon_view(u"BlockArrows"_ustr))
    , m_pFlowchartIconView(m_xBuilder->weld_icon_view(u"Flowcharts"_ustr))
    , m_pCalloutIconView(m_xBuilder->weld_icon_view(u"Callouts"_ustr))
    , m_pStarIconView(m_xBuilder->weld_icon_view(u"Stars"_ustr))
    , m_p3DObjectIconView(m_xBuilder->weld_icon_view(u"3DObjects"_ustr))
    , mxFrame(std::move(xFrame))
{
    Initialize();
    pParent->set_size_request(pParent->get_approximate_digit_width() * 20, -1);
    m_xContainer->set_size_request(m_xContainer->get_approximate_digit_width() * 25, -1);
}

std::unique_ptr<PanelLayout> DefaultShapesPanel::Create(
    weld::Widget* pParent,
    const Reference< XFrame >& rxFrame)
{
    if (pParent == nullptr)
        throw lang::IllegalArgumentException(u"no parent Window given to DefaultShapesPanel::Create"_ustr, nullptr, 0);
    if ( ! rxFrame.is())
        throw lang::IllegalArgumentException(u"no XFrame given to DefaultShapesPanel::Create"_ustr, nullptr, 1);

    return std::make_unique<DefaultShapesPanel>(pParent, rxFrame);
}

void DefaultShapesPanel::Initialize()
{
    const std::map<weld::IconView*, std::vector<OUString>> aShapesViewsMap
        = { { m_pLineArrowIconView.get(), m_aLineShapes },
            { m_pCurveIconView.get(), m_aCurveShapes },
            { m_pConnectorIconView.get(), m_aConnectorShapes },
            { m_pBasicShapeIconView.get(), m_aBasicShapes },
            { m_pSymbolShapeIconView.get(), m_aSymbolShapes },
            { m_pBlockArrowIconView.get(), m_aBlockArrowShapes },
            { m_pFlowchartIconView.get(), m_aFlowchartShapes },
            { m_pCalloutIconView.get(), m_aCalloutShapes },
            { m_pStarIconView.get(), m_aStarShapes },
            { m_p3DObjectIconView.get(), m_a3DShapes } };

    for (auto& rEntry : aShapesViewsMap)
    {
        for (size_t i = 0; i < rEntry.second.size(); i++)
        {
            const OUString sSlotStr = rEntry.second.at(i);
            const Bitmap aSlotImage
                = vcl::CommandInfoProvider::GetImageForCommand(sSlotStr, mxFrame).GetBitmap();
            auto aProperties = vcl::CommandInfoProvider::GetCommandProperties(
                sSlotStr, vcl::CommandInfoProvider::GetModuleIdentifier(mxFrame));
            const OUString sLabel
                = vcl::CommandInfoProvider::GetTooltipForCommand(sSlotStr, aProperties, mxFrame);
            rEntry.first->insert(i, nullptr, &sSlotStr, &aSlotImage, nullptr);
            rEntry.first->set_item_accessible_name(i, sLabel);
            rEntry.first->set_item_tooltip_text(i, sLabel);
        }

        rEntry.first->connect_item_activated(LINK(this, DefaultShapesPanel, ShapeActivatedHdl));
        m_aShapesViews.push_back(rEntry.first);
    }
}

DefaultShapesPanel::~DefaultShapesPanel() { m_aShapesViews.clear(); }

IMPL_LINK(DefaultShapesPanel, ShapeActivatedHdl, const weld::TreeIter&, rIter, bool)
{
    const weld::ItemView& rActiveItemView = rIter.getItemView();
    for (weld::IconView* pView : m_aShapesViews)
    {
        if (&rActiveItemView == pView)
            comphelper::dispatchCommand(rActiveItemView.get_id(rIter), {});
        else
            pView->unselect_all();
    }

    return true;
}

} // end of namespace svx::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
