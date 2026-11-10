/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <svx/MenuOrToolMenuButton.hxx>
#include <svx/tbcontrl.hxx>
#include <vcl/toolbox.hxx>
#include <vcl/weld/Toolbar.hxx>
#include <vcl/weld/Window.hxx>

MenuOrToolMenuButton::MenuOrToolMenuButton(weld::MenuButton* pMenuButton)
    : m_pMenuButton(pMenuButton)
    , m_pToolbar(nullptr)
    , m_pControl(nullptr)
    , m_nId(0)
{
}

MenuOrToolMenuButton::MenuOrToolMenuButton(weld::Toolbar* pToolbar, OUString aIdent)
    : m_pMenuButton(nullptr)
    , m_pToolbar(pToolbar)
    , m_aIdent(std::move(aIdent))
    , m_pControl(nullptr)
    , m_nId(0)
{
}

MenuOrToolMenuButton::MenuOrToolMenuButton(SvxColorToolBoxControl* pControl, ToolBox* pToolbar,
                                           ToolBoxItemId nId)
    : m_pMenuButton(nullptr)
    , m_pToolbar(nullptr)
    , m_pControl(pControl)
    , m_xToolBox(pToolbar)
    , m_nId(nId)
{
}

MenuOrToolMenuButton::~MenuOrToolMenuButton() {}

bool MenuOrToolMenuButton::get_active() const
{
    if (m_pMenuButton)
        return m_pMenuButton->get_active();
    if (m_pToolbar)
        return m_pToolbar->get_menu_item_active(m_aIdent);
    return m_xToolBox->GetDownItemId() == m_nId;
}

void MenuOrToolMenuButton::set_inactive() const
{
    if (m_pMenuButton)
    {
        if (m_pMenuButton->get_active())
            m_pMenuButton->set_active(false);
        return;
    }
    if (m_pToolbar)
    {
        if (m_pToolbar->get_menu_item_active(m_aIdent))
            m_pToolbar->set_menu_item_active(m_aIdent, false);
        return;
    }
    m_pControl->EndPopupMode();
}

weld::Widget* MenuOrToolMenuButton::get_widget() const
{
    if (m_pMenuButton)
        return m_pMenuButton;
    if (m_pToolbar)
        return m_pToolbar;
    return m_xToolBox->GetFrameWeld();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
