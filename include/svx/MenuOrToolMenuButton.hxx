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

#pragma once

#include <rtl/ustring.hxx>
#include <svx/Palette.hxx>
#include <vcl/toolboxid.hxx>
#include <vcl/weld/MenuButton.hxx>

namespace weld
{
class Toolbar;
}

class ToolBox;

class SvxColorToolBoxControl;

class SVXCORE_DLLPUBLIC MenuOrToolMenuButton
{
private:
    // either
    weld::MenuButton* m_pMenuButton;
    // or
    weld::Toolbar* m_pToolbar;
    OUString m_aIdent;
    // or
    SvxColorToolBoxControl* m_pControl;
    VclPtr<ToolBox> m_xToolBox;
    ToolBoxItemId m_nId;

public:
    MenuOrToolMenuButton(weld::MenuButton* pMenuButton);
    MenuOrToolMenuButton(weld::Toolbar* pToolbar, OUString sIdent);
    MenuOrToolMenuButton(SvxColorToolBoxControl* pControl, ToolBox* pToolbar, ToolBoxItemId nId);
    ~MenuOrToolMenuButton();

    MenuOrToolMenuButton(MenuOrToolMenuButton const&) = default;
    MenuOrToolMenuButton(MenuOrToolMenuButton&&) = default;
    MenuOrToolMenuButton& operator=(MenuOrToolMenuButton const&) = default;
    MenuOrToolMenuButton& operator=(MenuOrToolMenuButton&&) = default;

    bool get_active() const;
    void set_inactive() const;
    weld::Widget* get_widget() const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
