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

#ifndef INCLUDED_SFX2_SOURCE_DIALOG_BACKINGWINDOW_HXX
#define INCLUDED_SFX2_SOURCE_DIALOG_BACKINGWINDOW_HXX

#include <rtl/ustring.hxx>
#include <map>
#include <recentfilemetadatacache.hxx>
#include <recentfilemetadataloader.hxx>

#include <vcl/InterimItemWindow.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/ComboBox.hxx>
#include <vcl/weld/Entry.hxx>
#include <vcl/weld/Container.hxx>
#include <vcl/weld/Image.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/LinkButton.hxx>
#include <vcl/weld/MenuButton.hxx>
#include <vcl/weld/ToggleButton.hxx>
#include <vcl/weld/TreeView.hxx>

#include <templatedefaultview.hxx>

#include <svtools/acceleratorexecute.hxx>

#include <com/sun/star/datatransfer/dnd/XDropTargetListener.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XFrame.hpp>

#include <memory>

class BrandImage;

class BackingWindow : public InterimItemWindow
{
    css::uno::Reference<css::uno::XComponentContext> mxContext;
    css::uno::Reference<css::frame::XDispatchProvider> mxDesktopDispatchProvider;
    css::uno::Reference<css::frame::XFrame> mxFrame;

    /** helper for drag&drop. */
    css::uno::Reference<css::datatransfer::dnd::XDropTargetListener> mxDropTargetListener;

    std::unique_ptr<weld::Button> mxOpenButton;
    std::unique_ptr<weld::ToggleButton> mxRecentButton;
    std::unique_ptr<weld::Button> mxRemoteButton;
    std::unique_ptr<weld::ToggleButton> mxTemplateButton;

    std::unique_ptr<weld::Label> mxCreateLabel;
    std::unique_ptr<weld::Label> mxAltHelpLabel;
    std::unique_ptr<weld::ComboBox> mxFilter;
    // Dainvo: recent files as a list (Name, Location, Modified) with search
    std::unique_ptr<weld::Widget> mxRecentBox;
    std::unique_ptr<weld::Entry> mxRecentSearch;
    std::unique_ptr<weld::TreeView> mxRecentList;
    std::unique_ptr<weld::Label> mxRecentEmpty;
    std::unique_ptr<weld::Label> mxRecentHeading;
    std::unique_ptr<weld::Label> mxRecentColName;
    std::unique_ptr<weld::Label> mxRecentColLocation;
    std::unique_ptr<weld::Label> mxRecentColModified;
    std::unique_ptr<weld::Widget> mxRecentCols;
    std::unique_ptr<weld::Button> mxBrowseTemplates;
    struct RecentRow
    {
        OUString aName, aSearchName, aExtension, aFolder, aPath, aModified, aIcon;
        bool bReadOnly;
    };
    std::map<OUString, RecentRow> maRecentRows; // keyed by URL (the row id)
    std::vector<OUString> maRecentOrder; // preserve history order, including pins
    sfx2::RecentFileMetadataCache maRecentMetadata;
    std::unique_ptr<sfx2::RecentFileMetadataLoader> mxRecentMetadataLoader;
    bool mbRecentNarrow = false;
    bool mbRecentRendererOn = false;
    OUString maRecentHoverId; // row under the mouse, drawn with a hover highlight
    tools::Long mnRecentWidth = 800; // current list width; rows are drawn this wide
    std::unique_ptr<weld::MenuButton> mxActions;

    std::unique_ptr<weld::Button> mxWriterAllButton;
    std::unique_ptr<weld::Button> mxCalcAllButton;
    std::unique_ptr<weld::Button> mxImpressAllButton;
    std::unique_ptr<weld::Button> mxDrawAllButton;
    std::unique_ptr<weld::Button> mxDBAllButton;
    std::unique_ptr<weld::Button> mxMathAllButton;
    std::unique_ptr<BrandImage> mxBrandImage;
    std::unique_ptr<weld::CustomWeld> mxBrandImageWeld;

    std::unique_ptr<weld::Button> mxHelpButton;
    std::unique_ptr<weld::Button> mxExtensionsButton;
    std::unique_ptr<weld::Button> mxSettingsButton; // Dainvo
    std::unique_ptr<weld::Button> mxDonateButton;

    std::unique_ptr<weld::Container> mxAllButtonsBox;
    std::unique_ptr<weld::Container> mxButtonsBox;
    std::unique_ptr<weld::Container> mxSmallButtonsBox;
    std::unique_ptr<weld::Container> mxRightBox;

    std::unique_ptr<TemplateDefaultView> mxLocalView;
    std::unique_ptr<weld::CustomWeld> mxLocalViewWin;
    std::unique_ptr<weld::Image> mxDonationLeft;
    std::unique_ptr<weld::Image> mxDonationRight;
    std::unique_ptr<weld::LinkButton> mxDonationLink;
    std::unique_ptr<weld::Container> mxDonationBox;
    std::unique_ptr<weld::Label> mxDonationBoxTitle;
    std::unique_ptr<weld::Label> mxDonationBoxText;

    bool mbLocalViewInitialized;

    css::uno::Reference<css::datatransfer::dnd::XDropTarget> mxDropTarget;

    bool mbInitControls;
    std::unique_ptr<svt::AcceleratorExecute> mpAccExec;

    void dispatchURL(const OUString& i_rURL, const OUString& i_rTarget = u"_default"_ustr,
                     const css::uno::Reference<css::frame::XDispatchProvider>& i_xProv
                     = css::uno::Reference<css::frame::XDispatchProvider>(),
                     const css::uno::Sequence<css::beans::PropertyValue>& = css::uno::Sequence<
                         css::beans::PropertyValue>());

    DECL_LINK(ToggleHdl, weld::Toggleable&, void);
    DECL_LINK(FilterHdl, weld::ComboBox&, void);
    DECL_LINK(RecentSearchHdl, weld::TextWidget&, void);
    DECL_LINK(RecentActivatedHdl, const weld::TreeIter&, bool);
    DECL_LINK(RecentGetSizeHdl, weld::TreeView::get_size_args, Size);
    DECL_LINK(RecentRenderHdl, weld::TreeView::render_args, void);
    DECL_LINK(RecentTooltipHdl, const weld::TreeIter&, OUString);
    DECL_LINK(RecentSizeAllocHdl, const Size&, void);
    DECL_LINK(RecentMouseMoveHdl, const MouseEvent&, bool);
    DECL_LINK(ClickHdl, weld::Button&, void);
    DECL_LINK(ClickHelpHdl, weld::Button&, void);
    DECL_LINK(MenuSelectHdl, const OUString&, void);
    DECL_STATIC_LINK(BackingWindow, ExtLinkClickHdl, weld::Button&, void);
    DECL_LINK(CreateContextMenuHdl, TemplateViewItem*, void);
    DECL_LINK(OpenTemplateHdl, const OUString&, void);
    DECL_LINK(EditTemplateHdl, const OUString&, void);
    DECL_STATIC_LINK(BackingWindow, OnDonateLinkClick, weld::LinkButton&, bool);

    void initControls();
    void initDonationBanner();

    void initializeLocalView();

    void checkInstalledModules();

    void DataChanged(const DataChangedEvent&) override;

    template <typename WidgetClass> void setLargerFont(WidgetClass&, const vcl::Font&);
    void ApplyStyleSettings();

private:
    void applyFilter();
    void reloadRecentList();
    void fillRecentList();

public:
    explicit BackingWindow(vcl::Window* pParent);
    virtual ~BackingWindow() override;
    virtual void dispose() override;

    virtual bool PreNotify(NotifyEvent& rNEvt) override;
    virtual void GetFocus() override;

    void setOwningFrame(const css::uno::Reference<css::frame::XFrame>& xFrame);

    void clearRecentFileList();
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
