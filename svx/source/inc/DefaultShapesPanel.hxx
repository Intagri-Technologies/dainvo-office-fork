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

#pragma once

#include <com/sun/star/frame/XFrame.hpp>
#include <sfx2/sidebar/PanelLayout.hxx>
#include <vcl/weld/IconView.hxx>
#include <map>
#include "ShapesUtil.hxx"

using namespace css;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::frame;

namespace svx::sidebar {

/** This panel provides buttons for inserting shapes into a document.
*/
class DefaultShapesPanel final
    : public PanelLayout, public SvxShapeCommandsMap
{
public:
    DefaultShapesPanel (
        weld::Widget* pParent,
        css::uno::Reference<css::frame::XFrame> xFrame);
    static std::unique_ptr<PanelLayout> Create(
        weld::Widget* pParent,
        const css::uno::Reference<css::frame::XFrame>& rxFrame);
    virtual ~DefaultShapesPanel() override;

private:
    std::unique_ptr<weld::IconView> m_pLineArrowIconView;
    std::unique_ptr<weld::IconView> m_pCurveIconView;
    std::unique_ptr<weld::IconView> m_pConnectorIconView;
    std::unique_ptr<weld::IconView> m_pBasicShapeIconView;
    std::unique_ptr<weld::IconView> m_pSymbolShapeIconView;
    std::unique_ptr<weld::IconView> m_pBlockArrowIconView;
    std::unique_ptr<weld::IconView> m_pFlowchartIconView;
    std::unique_ptr<weld::IconView> m_pCalloutIconView;
    std::unique_ptr<weld::IconView> m_pStarIconView;
    std::unique_ptr<weld::IconView> m_p3DObjectIconView;

    Reference< XFrame >       mxFrame;
    std::vector<weld::IconView*> m_aShapesViews;

    void Initialize();
    DECL_LINK(ShapeActivatedHdl, const weld::TreeIter&, bool);
};

} // end of namespace sd::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
