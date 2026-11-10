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

#include <sfx2/sidebar/PanelLayout.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/CheckButton.hxx>
#include <vcl/weld/IconView.hxx>

#include <com/sun/star/ui/LayoutSize.hpp>

namespace com::sun::star::beans { class XPropertySet; }
namespace com::sun::star::container { class XIndexAccess; class XNameContainer; }

namespace sdtools {
class EventMultiplexerEvent;
}

namespace sd
{

class ViewShellBase;
class DrawController;

enum TableCheckBox : sal_uInt16
{
    CB_HEADER_ROW       = 0,
    CB_TOTAL_ROW        = 1,
    CB_BANDED_ROWS      = 2,
    CB_FIRST_COLUMN     = 3,
    CB_LAST_COLUMN      = 4,
    CB_BANDED_COLUMNS   = 5,
    CB_COUNT            = CB_BANDED_COLUMNS + 1
};

class TableDesignWidget final
{
public:
    TableDesignWidget(weld::Builder& rBuilder, ViewShellBase& rBase);
    ~TableDesignWidget();

    // callbacks
    void onSelectionChanged();

    void ApplyOptions();
    void InsertStyle();
    void CloneStyle();
    void ResetStyle();
    void DeleteStyle();
    void EditStyle(const OUString& rCommand);

private:
    void addListener();
    void removeListener();
    void updateControls();
    void selectStyle(std::u16string_view rStyle);
    void endTextEditForStyle(const css::uno::Reference<css::uno::XInterface>& rStyle);
    void setDocumentModified();

    void FillDesignPreviewControl();

    DECL_LINK(EventMultiplexerListener, sdtools::EventMultiplexerEvent&, void);
    DECL_LINK(ContextMenuHdl, const CommandEvent&, bool);
    DECL_LINK(IconViewItemActivatedHdl, const weld::TreeIter&, bool);
    DECL_LINK(implCheckBoxHdl, weld::Toggleable&, void);

    ViewShellBase& mrBase;

    std::unique_ptr<weld::Menu> m_xMenu;
    std::unique_ptr<weld::IconView> m_xIconView;
    std::unique_ptr<weld::CheckButton> m_aCheckBoxes[CB_COUNT];

    css::uno::Reference< css::beans::XPropertySet > mxSelectedTable;
    rtl::Reference< DrawController > mxView;
    css::uno::Reference< css::container::XIndexAccess > mxTableFamily;
    css::uno::Reference< css::container::XNameContainer > mxCellFamily;
};

class TableDesignPane final : public PanelLayout
{
private:
    std::unique_ptr<TableDesignWidget> m_xImpl;
public:
    TableDesignPane( weld::Widget* pParent, ViewShellBase& rBase )
        : PanelLayout(pParent, u"TableDesignPanel"_ustr,
            u"modules/simpress/ui/tabledesignpanel.ui"_ustr)
        , m_xImpl(new TableDesignWidget(*m_xBuilder, rBase))
    {
    }
    virtual css::ui::LayoutSize GetHeightForWidth(const sal_Int32 /*nWidth*/) override
    {
        return css::ui::LayoutSize(-1, -1, -1);
    }
};

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
