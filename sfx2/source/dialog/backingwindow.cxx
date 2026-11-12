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

#include "backingwindow.hxx"
#include <chrono>
#include <utility>
#include <vcl/event.hxx>
#include <vcl/help.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/syswin.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Menu.hxx>

#include <unotools/historyoptions.hxx>
#include <unotools/moduleoptions.hxx>
#include <unotools/cmdoptions.hxx>
#include <unotools/configmgr.hxx>
#include <svtools/openfiledroptargetlistener.hxx>
#include <svtools/colorcfg.hxx>
#include <svtools/langhelp.hxx>
#include <templateviewitem.hxx>

#include <comphelper/processfactory.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>
#include <sfx2/app.hxx>
#include <officecfg/Office/Common.hxx>

#include <i18nlangtag/languagetag.hxx>
#include <comphelper/diagnose_ex.hxx>

#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/document/MacroExecMode.hpp>
#include <com/sun/star/document/UpdateDocMode.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <com/sun/star/util/URLTransformer.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>

#include <sfx2/viewfrm.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <sfx2/styfitem.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/tplpitem.hxx>

#include <svl/itemset.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/sfxsids.hrc>
#include <sfx2/strings.hrc>
#include <sfx2/sfxresid.hxx>
#include <bitmaps.hlst>
#include <rtl/bootstrap.hxx>
#include <vcl/image.hxx>
#include <vcl/virdev.hxx>
#include <vcl/graph.hxx>
#include <vcl/graphicfilter.hxx>

#include <config_folders.h>
#include <osl/file.hxx>
#include <osl/security.hxx>
#include <osl/time.h>
#include <tools/date.hxx>
#include <tools/time.hxx>
#include <tools/urlobj.hxx>
#include <unotools/historyoptions.hxx>
#include <unotools/localedatawrapper.hxx>
#include <unotools/syslocale.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::document;

class BrandImage final : public weld::CustomWidgetController
{
private:
    Bitmap maBrandImage;
    bool mbIsDark = false;
    Size m_BmpSize;

public:
    const Size & getSize() { return m_BmpSize; }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);

        const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
        OutputDevice& rDevice = pDrawingArea->get_ref_device();
        rDevice.SetBackground(Wallpaper(rStyleSettings.GetWindowColor()));

        SetPointer(PointerStyle::RefHand);
    }

    virtual void Resize() override
    {
        auto nWidth = GetOutputSizePixel().Width();
        if (maBrandImage.GetSizePixel().Width() != nWidth)
            LoadImageForWidth(nWidth);
        weld::CustomWidgetController::Resize();
    }

    void LoadImageForWidth(int nWidth)
    {
        mbIsDark = Application::GetSettings().GetStyleSettings().GetDialogColor().IsDark();
        SfxApplication::loadBrandSvg(mbIsDark ? u"shell/logo-sc_inverted" : u"shell/logo-sc",
                                    maBrandImage, nWidth);
    }

    void ConfigureForWidth(int nWidth)
    {
        LoadImageForWidth(nWidth);
        m_BmpSize = maBrandImage.GetSizePixel();
        set_size_request(m_BmpSize.Width(), m_BmpSize.Height());
    }

    virtual void StyleUpdated() override
    {
        const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

        // tdf#141857 update background to current theme
        OutputDevice& rDevice = GetDrawingArea()->get_ref_device();
        rDevice.SetBackground(Wallpaper(rStyleSettings.GetWindowColor()));

        const bool bIsDark = rStyleSettings.GetDialogColor().IsDark();
        if (bIsDark != mbIsDark)
            LoadImageForWidth(GetOutputSizePixel().Width());
        weld::CustomWidgetController::StyleUpdated();
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
        {
            OUString sURL = officecfg::Office::Common::Menus::VolunteerURL::get();
            localizeWebserviceURI(sURL);

            Reference<css::system::XSystemShellExecute> const xSystemShellExecute(
                css::system::SystemShellExecute::create(
                    ::comphelper::getProcessComponentContext()));
            xSystemShellExecute->execute(sURL, OUString(),
                                         css::system::SystemShellExecuteFlags::URIS_ONLY);
        }
        return true;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        rRenderContext.DrawBitmap(Point(0, 0), maBrandImage);
    }
};

// increase size of the text in the buttons on the left fMultiplier-times
float const g_fMultiplier = 1.2f;

BackingWindow::BackingWindow(vcl::Window* i_pParent)
    : InterimItemWindow(i_pParent, u"sfx/ui/startcenter.ui"_ustr, u"StartCenter"_ustr, false)
    , mxOpenButton(m_xBuilder->weld_button(u"open_all"_ustr))
    , mxRecentButton(m_xBuilder->weld_toggle_button(u"open_recent"_ustr))
    , mxRemoteButton(m_xBuilder->weld_button(u"open_remote"_ustr))
    , mxTemplateButton(m_xBuilder->weld_toggle_button(u"templates_all"_ustr))
    , mxCreateLabel(m_xBuilder->weld_label(u"create_label"_ustr))
    , mxAltHelpLabel(m_xBuilder->weld_label(u"althelplabel"_ustr))
    , mxFilter(m_xBuilder->weld_combo_box(u"cbFilter"_ustr))
    , mxRecentBox(m_xBuilder->weld_widget(u"dv_recent_box"_ustr))
    , mxRecentSearch(m_xBuilder->weld_entry(u"dv_recent_search"_ustr))
    , mxRecentList(m_xBuilder->weld_tree_view(u"dv_recent_list"_ustr))
    , mxRecentEmpty(m_xBuilder->weld_label(u"dv_recent_empty"_ustr))
    , mxRecentHeading(m_xBuilder->weld_label(u"dv_recent_heading"_ustr))
    , mxRecentColName(m_xBuilder->weld_label(u"dv_colhead0"_ustr))
    , mxRecentColLocation(m_xBuilder->weld_label(u"dv_colhead1"_ustr))
    , mxRecentColModified(m_xBuilder->weld_label(u"dv_colhead2"_ustr))
    , mxActions(m_xBuilder->weld_menu_button(u"mbActions"_ustr))
    , mxWriterAllButton(m_xBuilder->weld_button(u"writer_all"_ustr))
    , mxCalcAllButton(m_xBuilder->weld_button(u"calc_all"_ustr))
    , mxImpressAllButton(m_xBuilder->weld_button(u"impress_all"_ustr))
    , mxDrawAllButton(m_xBuilder->weld_button(u"draw_all"_ustr))
    , mxDBAllButton(m_xBuilder->weld_button(u"database_all"_ustr))
    , mxMathAllButton(m_xBuilder->weld_button(u"math_all"_ustr))
    , mxBrandImage(new BrandImage)
    , mxBrandImageWeld(new weld::CustomWeld(*m_xBuilder, u"daBrand"_ustr, *mxBrandImage))
    , mxHelpButton(m_xBuilder->weld_button(u"help"_ustr))
    , mxExtensionsButton(m_xBuilder->weld_button(u"extensions"_ustr))
    , mxSettingsButton(m_xBuilder->weld_button(u"dv_settings"_ustr))
    , mxDonateButton(m_xBuilder->weld_button(u"donate"_ustr))
    , mxAllButtonsBox(m_xBuilder->weld_container(u"all_buttons_box"_ustr))
    , mxButtonsBox(m_xBuilder->weld_container(u"buttons_box"_ustr))
    , mxSmallButtonsBox(m_xBuilder->weld_container(u"small_buttons_box"_ustr))
    , mxRightBox(m_xBuilder->weld_container(u"box2"_ustr))
    , mxAllRecentThumbnails(new sfx2::RecentDocsView(m_xBuilder->weld_scrolled_window(u"scrollrecent"_ustr, true)))
    , mxAllRecentThumbnailsWin(new weld::CustomWeld(*m_xBuilder, u"all_recent"_ustr, *mxAllRecentThumbnails))
    , mxLocalView(new TemplateDefaultView(m_xBuilder->weld_scrolled_window(u"scrolllocal"_ustr, true),
                                          m_xBuilder->weld_menu(u"localmenu"_ustr)))
    , mxLocalViewWin(new weld::CustomWeld(*m_xBuilder, u"local_view"_ustr, *mxLocalView))
    , mxDonationLeft(m_xBuilder->weld_image(u"imgDonationLeft"_ustr))
    , mxDonationRight(m_xBuilder->weld_image(u"imgDonationRight"_ustr))
    , mxDonationLink(m_xBuilder->weld_link_button(u"btnDonateLink"_ustr))
    , mxDonationBox(m_xBuilder->weld_container(u"gdDonation"_ustr))
    , mxDonationBoxTitle(m_xBuilder->weld_label(u"lbDonateTitle"_ustr))
    , mxDonationBoxText(m_xBuilder->weld_label(u"lbDonateText"_ustr))
    , mbLocalViewInitialized(false)
    , mbInitControls(false)
{
    // init background, undo InterimItemWindow defaults for this widget
    SetPaintTransparent(false);

    // draw the donation image/text
    const bool bShowDonation(officecfg::Office::Common::Misc::ShowDonation::get());

    const auto t0 = std::chrono::system_clock::now().time_since_epoch();
    const sal_Int32 nDay = std::chrono::duration_cast<std::chrono::hours>(t0).count()/24; // days since 1970-01-01
    if (bShowDonation && (nDay % 30 == 0))  // show the donation banner every 30 days
        initDonationBanner();
    else
        mxDonationBox->set_visible(false);

    // square action button
    auto nHeight = mxFilter->get_preferred_size().getHeight();
    mxActions->set_size_request(nHeight, nHeight);

    //set an alternative help label that doesn't hotkey the H of the Help menu
    mxHelpButton->set_label(mxAltHelpLabel->get_label());
    mxHelpButton->connect_clicked(LINK(this, BackingWindow, ClickHelpHdl));

    // tdf#161796 replace the extension button with a donate button
    if (bShowDonation)
    {
        mxExtensionsButton->hide();
        mxDonateButton->show();
        mxDonateButton->set_from_icon_name(BMP_DONATE);
        OUString sDonate(SfxResId(STR_DONATE_BUTTON));
        if (sDonate.getLength() > 8)
        {
            mxDonateButton->set_tooltip_text(sDonate);
            sDonate = OUString::Concat(sDonate.subView(0, 7)) + "...";
        }
        mxDonateButton->set_label(sDonate);
    }

    mxDropTarget = mxAllRecentThumbnails->GetDropTarget();

    try
    {
        mxContext.set( ::comphelper::getProcessComponentContext(), uno::UNO_SET_THROW );
    }
    catch (const Exception&)
    {
        TOOLS_WARN_EXCEPTION( "fwk", "BackingWindow" );
    }

    SetStyle( GetStyle() | WB_DIALOGCONTROL );

    // get dispatch provider
    Reference<XDesktop2> xDesktop = Desktop::create( comphelper::getProcessComponentContext() );
    mxDesktopDispatchProvider = xDesktop;

}

void BackingWindow::initDonationBanner()
{
    mxDonationLink->connect_activate_link(LINK(this, BackingWindow, OnDonateLinkClick));

    mxDonationBox->set_background(COL_WHITE);
    mxDonationBoxTitle->set_font_color(COL_BLACK);
    mxDonationBoxText->set_font_color(COL_BLACK);

    ScopedVclPtr<VirtualDevice> m_pVirDevLeft = mxDonationLeft->create_virtual_device();
    Graphic aGraphic;
    Size aGraphicSize;
    OUString aURL = "$BRAND_BASE_DIR/" LIBO_ETC_FOLDER + u"/shell/donate1.png"_ustr;
    rtl::Bootstrap::expandMacros( aURL );
    if (GraphicFilter::LoadGraphic(aURL, OUString(), aGraphic) == ERRCODE_NONE)
    {
        aGraphicSize = Size(200, 200);
        m_pVirDevLeft->SetOutputSizePixel(aGraphicSize);
        m_pVirDevLeft->DrawBitmap(Point(0, 0), aGraphicSize, aGraphic.GetBitmap());
    }
    mxDonationLeft->set_image(m_pVirDevLeft.get());
    m_pVirDevLeft.disposeAndClear();

    ScopedVclPtr<VirtualDevice> m_pVirDevRight = mxDonationRight->create_virtual_device();
    aURL = "$BRAND_BASE_DIR/" LIBO_ETC_FOLDER + u"/shell/donate2.png"_ustr;
    rtl::Bootstrap::expandMacros( aURL );
    if (GraphicFilter::LoadGraphic(aURL, OUString(), aGraphic) == ERRCODE_NONE)
    {
        aGraphicSize = Size(150, 150);
        m_pVirDevRight->SetOutputSizePixel(aGraphicSize);
        // image has a transparent background and needs to be drawn with the advanced function
        m_pVirDevRight->DrawBitmap(Point(0, 0), aGraphicSize, Point(), aGraphicSize,
                                   aGraphic.GetBitmap(), MetaActionType::BMPEX);
    }
    mxDonationRight->set_image(m_pVirDevRight.get());
    m_pVirDevRight.disposeAndClear();
}

IMPL_STATIC_LINK_NOARG(BackingWindow, OnDonateLinkClick, weld::LinkButton&, bool)
{
    SfxDispatcher &rDispatcher = *SfxGetpApp()->GetDispatcher_Impl();
    rDispatcher.Execute(SID_DONATION, SfxCallMode::ASYNCHRON);

    return true;
}

IMPL_LINK(BackingWindow, ClickHelpHdl, weld::Button&, rButton, void)
{
    if (Help* pHelp = Application::GetHelp())
        pHelp->Start(m_xContainer->get_help_id(), &rButton);
}

BackingWindow::~BackingWindow()
{
    disposeOnce();
}

void BackingWindow::dispose()
{
    // deregister drag&drop helper
    if (mxDropTargetListener.is())
    {
        if (mxDropTarget.is())
        {
            mxDropTarget->removeDropTargetListener(mxDropTargetListener);
            mxDropTarget->setActive(false);
        }
        mxDropTargetListener.clear();
    }
    mxDropTarget.clear();
    mxOpenButton.reset();
    mxRemoteButton.reset();
    mxRecentButton.reset();
    mxTemplateButton.reset();
    mxCreateLabel.reset();
    mxAltHelpLabel.reset();
    mxFilter.reset();
    mxRecentBox.reset();
    mxRecentSearch.reset();
    mxRecentList.reset();
    mxRecentEmpty.reset();
    mxRecentHeading.reset();
    mxRecentColName.reset();
    mxRecentColLocation.reset();
    mxRecentColModified.reset();
    mxActions.reset();
    mxWriterAllButton.reset();
    mxCalcAllButton.reset();
    mxImpressAllButton.reset();
    mxDrawAllButton.reset();
    mxDBAllButton.reset();
    mxMathAllButton.reset();
    mxBrandImageWeld.reset();
    mxBrandImage.reset();
    mxHelpButton.reset();
    mxDonateButton.reset();
    mxExtensionsButton.reset();
    mxSettingsButton.reset();
    mxAllButtonsBox.reset();
    mxButtonsBox.reset();
    mxSmallButtonsBox.reset();
    mxAllRecentThumbnailsWin.reset();
    mxAllRecentThumbnails.reset();
    mxLocalViewWin.reset();
    mxLocalView.reset();
    InterimItemWindow::dispose();
}

void BackingWindow::initControls()
{
    if( mbInitControls )
        return;

    mbInitControls = true;

    // collect the URLs of the entries in the File/New menu
    SvtModuleOptions    aModuleOptions;

    if (aModuleOptions.IsWriterInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_WRITER;

    if (aModuleOptions.IsCalcInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_CALC;

    if (aModuleOptions.IsImpressInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_IMPRESS;

    if (aModuleOptions.IsDrawInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_DRAW;

    if (aModuleOptions.IsDataBaseInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_DATABASE;

    if (aModuleOptions.IsMathInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_MATH;

    mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_OTHER;
    mxAllRecentThumbnails->Reload();
    mxAllRecentThumbnails->ShowTooltips( true );

    mxRecentList->connect_custom_get_size(LINK(this, BackingWindow, RecentGetSizeHdl));
    mxRecentList->connect_custom_render(LINK(this, BackingWindow, RecentRenderHdl));
    mxRecentList->connect_query_tooltip(LINK(this, BackingWindow, RecentTooltipHdl));
    mxRecentList->connect_size_allocate(LINK(this, BackingWindow, RecentSizeAllocHdl));
    mxRecentButton->set_active(true);
    ToggleHdl(*mxRecentButton);

    //set handlers
    mxLocalView->setCreateContextMenuHdl(LINK(this, BackingWindow, CreateContextMenuHdl));
    mxLocalView->setOpenTemplateHdl(LINK(this, BackingWindow, OpenTemplateHdl));
    mxLocalView->setEditTemplateHdl(LINK(this, BackingWindow, EditTemplateHdl));
    mxLocalView->ShowTooltips( true );

    checkInstalledModules();

    mxExtensionsButton->connect_clicked(LINK(this, BackingWindow, ExtLinkClickHdl));
    mxSettingsButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxDonateButton->connect_clicked(LINK(this, BackingWindow, ExtLinkClickHdl));

    mxOpenButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    // Hide OpenRemote button on startpage if the OpenRemote uno command is not available
    SvtCommandOptions aCmdOptions;
    if (SvtCommandOptions().HasEntriesDisabled() && aCmdOptions.LookupDisabled(u"OpenRemote"_ustr))
        mxRemoteButton->set_visible(false);
    else
        mxRemoteButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    mxWriterAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxDrawAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxCalcAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxDBAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxImpressAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxMathAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    mxRecentButton->connect_toggled(LINK(this, BackingWindow, ToggleHdl));
    mxTemplateButton->connect_toggled(LINK(this, BackingWindow, ToggleHdl));

    mxFilter->connect_changed(LINK(this, BackingWindow, FilterHdl));
    mxRecentSearch->connect_changed(LINK(this, BackingWindow, RecentSearchHdl));
    mxRecentList->connect_item_activated(LINK(this, BackingWindow, RecentActivatedHdl));
    mxRecentList->set_size_request(-1, mxRecentList->get_height_rows(8));
    mxActions->connect_selected(LINK(this, BackingWindow, MenuSelectHdl));

    ApplyStyleSettings();
}

void BackingWindow::DataChanged(const DataChangedEvent& rDCEvt)
{
    if ((rDCEvt.GetType() != DataChangedEventType::SETTINGS)
        || !(rDCEvt.GetFlags() & AllSettingsFlags::STYLE))
    {
        InterimItemWindow::DataChanged(rDCEvt);
        return;
    }

    ApplyStyleSettings();
    Invalidate();
}

template <typename WidgetClass>
void BackingWindow::setLargerFont(WidgetClass& pWidget, const vcl::Font& rFont)
{
    vcl::Font aFont(rFont);
    aFont.SetFontSize(Size(0, aFont.GetFontSize().Height() * g_fMultiplier));
    pWidget->set_font(aFont);
}

void BackingWindow::ApplyStyleSettings()
{
    const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
    const Color aButtonsBackground(rStyleSettings.GetWindowColor());
    const vcl::Font& aButtonFont(rStyleSettings.GetPushButtonFont());
    const vcl::Font& aLabelFont(rStyleSettings.GetLabelFont());

    // setup larger fonts
    setLargerFont(mxWriterAllButton, aButtonFont);
    setLargerFont(mxDrawAllButton, aButtonFont);
    setLargerFont(mxCalcAllButton, aButtonFont);
    setLargerFont(mxDBAllButton, aButtonFont);
    setLargerFont(mxImpressAllButton, aButtonFont);
    setLargerFont(mxMathAllButton, aButtonFont);
    // Dainvo: "Create" is a section heading above the app cards
    {
        vcl::Font aHeading(aLabelFont);
        aHeading.SetFontSize(Size(0, aHeading.GetFontSize().Height() * 1.6));
        aHeading.SetWeight(WEIGHT_SEMIBOLD);
        mxCreateLabel->set_font(aHeading);
        mxRecentHeading->set_font(aHeading);

        vcl::Font aColumn(aLabelFont);
        aColumn.SetWeight(WEIGHT_SEMIBOLD);
        Color aMuted(rStyleSettings.GetWindowTextColor());
        aMuted.Merge(rStyleSettings.GetWindowColor(), 110);
        for (weld::Label* pLabel : { mxRecentColName.get(), mxRecentColLocation.get(), mxRecentColModified.get() })
        {
            pLabel->set_font(aColumn);
            pLabel->set_font_color(aMuted);
        }
        OUString aCreate = mxCreateLabel->get_label();
        if (aCreate.endsWith(":"))
            mxCreateLabel->set_label(aCreate.copy(0, aCreate.getLength() - 1));
    }

    mxAllButtonsBox->set_background(aButtonsBackground);
    mxSmallButtonsBox->set_background(aButtonsBackground);
    SetBackground(aButtonsBackground);

    // compute the menubar height
    sal_Int32 nMenuHeight = 0;
    if (SystemWindow* pSystemWindow = GetSystemWindow())
        nMenuHeight = pSystemWindow->GetMenuBarHeight();

    // fdo#34392: we do the layout dynamically, the layout depends on the font,
    // so we should handle data changed events (font changing) of the last child
    // control, at this point all the controls have updated settings (i.e. font).
    Size aPrefSize(mxAllButtonsBox->get_preferred_size());
    set_width_request(aPrefSize.Width());

    // Now set a brand image wide enough to fill this width
    weld::DrawingArea* pDrawingArea = mxBrandImage->GetDrawingArea();
    mxBrandImage->ConfigureForWidth(aPrefSize.Width() -
                                    (pDrawingArea->get_margin_start() + pDrawingArea->get_margin_end()));
    // Refetch because the brand image height to match this width is now set
    aPrefSize = mxAllButtonsBox->get_preferred_size();

    set_height_request(nMenuHeight + aPrefSize.Height() + mxBrandImage->getSize().getHeight());
}

void BackingWindow::initializeLocalView()
{
    if (!mbLocalViewInitialized)
    {
        mbLocalViewInitialized = true;
        mxLocalView->Populate();
        mxLocalView->filterItems(ViewFilter_Application(FILTER_APPLICATION::NONE));
        mxLocalView->showAllTemplates();
    }
}

void BackingWindow::checkInstalledModules()
{
    if (officecfg::Office::Common::Misc::ViewerAppMode::get())
    {
        mxTemplateButton->set_visible(false);
        mxCreateLabel->set_visible(false);
        mxWriterAllButton->set_visible(false);
        mxCalcAllButton->set_visible(false);
        mxImpressAllButton->set_visible(false);
        mxDrawAllButton->set_visible(false);
        mxMathAllButton->set_visible(false);
        mxDBAllButton->set_visible(false);
        return;
    }

    SvtModuleOptions aModuleOpt;

    mxWriterAllButton->set_sensitive(aModuleOpt.IsWriterInstalled());
    mxCalcAllButton->set_sensitive(aModuleOpt.IsCalcInstalled());
    mxImpressAllButton->set_sensitive(aModuleOpt.IsImpressInstalled());
    mxDrawAllButton->set_sensitive(aModuleOpt.IsDrawInstalled());
    mxMathAllButton->set_sensitive(aModuleOpt.IsMathInstalled());
    // Dainvo: the Base app is not offered to users. The database core stays
    // installed because mail merge (Calc sheets, macOS Contacts) needs it.
    mxDBAllButton->hide();
}

bool BackingWindow::PreNotify(NotifyEvent& rNEvt)
{
    if( rNEvt.GetType() == NotifyEventType::KEYINPUT )
    {
        const KeyEvent* pEvt = rNEvt.GetKeyEvent();
        const vcl::KeyCode& rKeyCode(pEvt->GetKeyCode());

        bool bThumbnailHasFocus = mxAllRecentThumbnails->HasFocus() || mxLocalView->HasFocus();

        // Subwindows of BackingWindow: Sidebar and Thumbnail view
        if( rKeyCode.GetCode() == KEY_F6 )
        {
            if( rKeyCode.IsShift() ) // Shift + F6
            {
                if (bThumbnailHasFocus)
                {
                    mxOpenButton->grab_focus();
                    return true;
                }
            }
            else if ( rKeyCode.IsMod1() ) // Ctrl + F6
            {
                if(mxAllRecentThumbnails->IsVisible())
                {
                    mxAllRecentThumbnails->GrabFocus();
                    return true;
                }
                else if(mxLocalView->IsVisible())
                {
                    mxLocalView->GrabFocus();
                    return true;
                }
            }
            else // F6
            {
                if (!bThumbnailHasFocus)
                {
                    if(mxAllRecentThumbnails->IsVisible())
                    {
                        mxAllRecentThumbnails->GrabFocus();
                        return true;
                    }
                    else if(mxLocalView->IsVisible())
                    {
                        mxLocalView->GrabFocus();
                        return true;
                    }
                }
            }
        }

        // try the 'normal' accelerators (so that eg. Ctrl+Q works)
        if (!mpAccExec)
        {
            mpAccExec = svt::AcceleratorExecute::createAcceleratorHelper();
            mpAccExec->init( comphelper::getProcessComponentContext(), mxFrame);
        }

        const OUString aCommand = mpAccExec->findCommand(svt::AcceleratorExecute::st_VCLKey2AWTKey(rKeyCode));
        if ((aCommand != "vnd.sun.star.findbar:FocusToFindbar") && pEvt && mpAccExec->execute(rKeyCode))
            return true;
    }
    return InterimItemWindow::PreNotify( rNEvt );
}

void BackingWindow::GetFocus()
{
    GetFocusFlags nFlags = GetParent()->GetGetFocusFlags();
    if( nFlags & GetFocusFlags::F6 )
    {
        if( nFlags & GetFocusFlags::Forward ) // F6
        {
            mxOpenButton->grab_focus();
            return;
        }
        else // Shift + F6 or Ctrl + F6
        {
            if(mxAllRecentThumbnails->IsVisible())
                mxAllRecentThumbnails->GrabFocus();
            else if(mxLocalView->IsVisible())
                mxLocalView->GrabFocus();
            return;
        }
    }
    InterimItemWindow::GetFocus();
}

void BackingWindow::setOwningFrame( const css::uno::Reference< css::frame::XFrame >& xFrame )
{
    mxFrame = xFrame;
    if( ! mbInitControls )
        initControls();

    // establish drag&drop mode
    mxDropTargetListener.set(new OpenFileDropTargetListener(mxContext, mxFrame));

    if (mxDropTarget.is())
    {
        mxDropTarget->addDropTargetListener(mxDropTargetListener);
        mxDropTarget->setActive(true);
    }

    css::uno::Reference<XFramesSupplier> xFramesSupplier(mxDesktopDispatchProvider, UNO_QUERY);
    if (xFramesSupplier)
        xFramesSupplier->setActiveFrame(mxFrame);
}

IMPL_STATIC_LINK_NOARG(BackingWindow, ExtLinkClickHdl, weld::Button&, void)
{
    try
    {
        OUString sURL;
        if (officecfg::Office::Common::Misc::ShowDonation::get())
            sURL = officecfg::Office::Common::Menus::DonationURL::get() +
                "?BCP47=" + LanguageTag(utl::ConfigManager::getUILocale()).getBcp47() +
                "&LOlang=" + LanguageTag(utl::ConfigManager::getUILocale()).getLanguage();
        else
            sURL = officecfg::Office::Common::Menus::ExtensionsURL::get() +
                "?LOvers=" + utl::ConfigManager::getProductVersion() +
                "&LOlocale=" + LanguageTag(utl::ConfigManager::getUILocale()).getBcp47();

        Reference<css::system::XSystemShellExecute> const
            xSystemShellExecute(
                css::system::SystemShellExecute::create(
                    ::comphelper::getProcessComponentContext()));
        xSystemShellExecute->execute(sURL, OUString(),
            css::system::SystemShellExecuteFlags::URIS_ONLY);
    }
    catch (const Exception&)
    {
    }
}

void BackingWindow::applyFilter()
{
    const int nFilter = mxFilter->get_active();
    if (mxLocalView->IsVisible())
    {
        FILTER_APPLICATION aFilter = static_cast<FILTER_APPLICATION>(nFilter);
        mxLocalView->filterItems(ViewFilter_Application(aFilter));
    }
    else
    {
        sfx2::ApplicationType aFilter;
        if (nFilter == 0)
            aFilter = sfx2::ApplicationType::TYPE_NONE;
        else
            aFilter = static_cast<sfx2::ApplicationType>(1 << (nFilter - 1));
        mxAllRecentThumbnails->setFilter(aFilter);
        fillRecentList();
    }
}

namespace
{
OUString lcl_RecentIcon(std::u16string_view rExt)
{
    using sfx2::ApplicationType;
    using sfx2::RecentDocsView;
    if (RecentDocsView::typeMatchesExtension(ApplicationType::TYPE_WRITER, rExt))
        return u"res/odt_16_8.png"_ustr;
    if (RecentDocsView::typeMatchesExtension(ApplicationType::TYPE_CALC, rExt))
        return u"res/ods_16_8.png"_ustr;
    if (RecentDocsView::typeMatchesExtension(ApplicationType::TYPE_IMPRESS, rExt))
        return u"res/odp_16_8.png"_ustr;
    if (RecentDocsView::typeMatchesExtension(ApplicationType::TYPE_DRAW, rExt))
        return u"res/odg_16_8.png"_ustr;
    if (RecentDocsView::typeMatchesExtension(ApplicationType::TYPE_MATH, rExt))
        return u"res/odf_16_8.png"_ustr;
    return OUString();
}
}

void BackingWindow::fillRecentList()
{
    if (!mxRecentList)
        return;

    const OUString aQuery = mxRecentSearch->get_text().trim().toAsciiLowerCase();
    OUString aHomeURL, aHome;
    if (osl::Security().getHomeDir(aHomeURL))
        osl::FileBase::getSystemPathFromFileURL(aHomeURL, aHome);
    const LocaleDataWrapper& rLocale = SvtSysLocale().GetLocaleData();
    const Date aToday(Date::SYSTEM);

    mxRecentList->freeze();
    mxRecentList->clear();
    maRecentRows.clear();
    if (!mbRecentRendererOn)
    {
        // must be enabled while the list is empty; the first fill can happen
        // during construction, before the rest of the setup runs
        mxRecentList->set_column_custom_renderer(1, true); // column 0 is the icon slot
        mbRecentRendererOn = true;
    }
    for (const SvtHistoryOptions::HistoryItem& rItem : SvtHistoryOptions::GetList(EHistoryType::PickList))
    {
        if (!mxAllRecentThumbnails->acceptsURL(rItem.sURL))
            continue;
        const INetURLObject aURL(rItem.sURL);
        const OUString aName = aURL.GetLastName(INetURLObject::DecodeMechanism::WithCharset);
        if (!aQuery.isEmpty() && aName.toAsciiLowerCase().indexOf(aQuery) == -1)
            continue;

        OUString aLocation, aFolder;
        if (aURL.GetProtocol() == INetProtocol::File)
        {
            INetURLObject aDir(aURL);
            aDir.removeSegment();
            aLocation = aDir.getFSysPath(FSysStyle::Detect);
            if (aLocation.endsWith("/") && aLocation.getLength() > 1)
                aLocation = aLocation.copy(0, aLocation.getLength() - 1);
            if (!aHome.isEmpty() && aLocation.startsWith(aHome))
                aLocation = "~" + aLocation.copy(aHome.getLength());
            // show just the folder name; the full path is in the tooltip
            aFolder = aLocation == "~" ? aLocation : aLocation.copy(aLocation.lastIndexOf('/') + 1);
            if (aFolder.isEmpty())
                aFolder = aLocation;
        }
        else
            aLocation = aFolder = aURL.GetHost();

        OUString aModified;
        osl::DirectoryItem aDirItem;
        osl::FileStatus aStatus(osl_FileStatus_Mask_ModifyTime);
        TimeValue aLocal;
        oslDateTime aDT;
        if (aURL.GetProtocol() == INetProtocol::File
            && osl::DirectoryItem::get(rItem.sURL, aDirItem) == osl::FileBase::E_None
            && aDirItem.getFileStatus(aStatus) == osl::FileBase::E_None)
        {
            TimeValue aUtc = aStatus.getModifyTime();
            if (osl_getLocalTimeFromSystemTime(&aUtc, &aLocal) && osl_getDateTimeFromTimeValue(&aLocal, &aDT))
            {
                const Date aDate(aDT.Day, aDT.Month, aDT.Year);
                if (aDate == aToday)
                    aModified = rLocale.getTime(tools::Time(aDT.Hours, aDT.Minutes), false);
                else
                    aModified = rLocale.getDate(aDate);
            }
        }

        maRecentRows[rItem.sURL] = { aName, aFolder, aLocation, aModified,
                                     lcl_RecentIcon(aURL.getExtension()) };
        mxRecentList->append(rItem.sURL, aName);
    }
    mxRecentList->thaw();
    const bool bEmpty = mxRecentList->n_children() == 0;
    // the scrolled window is merged into the list by the builder, so hide the list itself
    mxRecentList->set_visible(!bEmpty);
    mxRecentEmpty->set_visible(bEmpty);
}

// Dainvo: recent rows are drawn here - icon + name, folder, modified - at the
// same 50/30/20 split as the dv_recent_cols header grid, so both scale with the
// window. Below about 560px the Location column folds away.
constexpr tools::Long RECENT_ROW_HEIGHT = 40;
constexpr tools::Long RECENT_NARROW_WIDTH = 560;

IMPL_LINK_NOARG(BackingWindow, RecentGetSizeHdl, weld::TreeView::get_size_args, Size)
{
    // the reported width is the area the row is drawn in, so span the list
    return Size(std::max<tools::Long>(mnRecentWidth - 40, 1), RECENT_ROW_HEIGHT);
}

IMPL_LINK(BackingWindow, RecentRenderHdl, weld::TreeView::render_args, rArgs, void)
{
    vcl::RenderContext& rDev = std::get<0>(rArgs);
    const tools::Rectangle& rRect = std::get<1>(rArgs);
    const bool bSelected = std::get<2>(rArgs);
    auto it = maRecentRows.find(std::get<3>(rArgs));
    if (it == maRecentRows.end())
        return;
    const RecentRow& rRow = it->second;
    const StyleSettings& rStyle = rDev.GetSettings().GetStyleSettings();

    rDev.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR | vcl::PushFlags::FILLCOLOR
              | vcl::PushFlags::LINECOLOR);

    const tools::Long nLeft = rRect.Left(), nWidth = rRect.GetWidth();
    if (bSelected)
    {
        rDev.SetLineColor();
        rDev.SetFillColor(rStyle.GetHighlightColor());
        tools::Rectangle aSel(rRect);
        aSel.AdjustTop(2);
        aSel.AdjustBottom(-2);
        rDev.DrawRect(aSel, 6, 6);
    }
    else
    {
        rDev.SetLineColor(rStyle.GetSeparatorColor());
        rDev.DrawLine(Point(nLeft, rRect.Bottom()), Point(mnRecentWidth - 12, rRect.Bottom()));
    }

    vcl::Font aFont(rStyle.GetLabelFont());
    aFont.SetFontHeight(aFont.GetFontHeight() * 115 / 100);
    rDev.SetFont(aFont);
    const Color aText(bSelected ? rStyle.GetHighlightTextColor() : rStyle.GetWindowTextColor());
    Color aMuted(aText);
    if (!bSelected)
        aMuted.Merge(rStyle.GetWindowColor(), 110);

    constexpr tools::Long nIcon = 20;
    if (!rRow.aIcon.isEmpty())
    {
        Image aImage(StockImage::Yes, rRow.aIcon);
        rDev.DrawImage(Point(nLeft, rRect.Top() + (rRect.GetHeight() - nIcon) / 2),
                       Size(nIcon, nIcon), aImage);
    }

    // columns are placed in list coordinates (the header grid spans the whole
    // list), not relative to the row, which VCL indents for its icon slot
    (void)nWidth;
    const bool bNarrow = mnRecentWidth < RECENT_NARROW_WIDTH;
    const tools::Long nNameEnd = mnRecentWidth * (bNarrow ? 80 : 50) / 100;
    const tools::Long nLocEnd = mnRecentWidth * 80 / 100;
    const DrawTextFlags nFlags = DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis;
    auto drawCell = [&](const OUString& rText, tools::Long nX, tools::Long nEnd, const Color& rColor) {
        rDev.SetTextColor(rColor);
        rDev.DrawText(tools::Rectangle(nX, rRect.Top(), nEnd - 12, rRect.Bottom()), rText, nFlags);
    };
    drawCell(rRow.aName, nLeft + nIcon + 12, nNameEnd, aText);
    if (!bNarrow)
        drawCell(rRow.aFolder, nNameEnd, nLocEnd, aMuted);
    drawCell(rRow.aModified, bNarrow ? nNameEnd : nLocEnd, mnRecentWidth, aMuted);

    rDev.Pop();
}

IMPL_LINK(BackingWindow, RecentTooltipHdl, const weld::TreeIter&, rIter, OUString)
{
    auto it = maRecentRows.find(mxRecentList->get_id(rIter));
    return it == maRecentRows.end() ? OUString() : it->second.aPath;
}

IMPL_LINK(BackingWindow, RecentSizeAllocHdl, const Size&, rSize, void)
{
    const bool bNarrow = rSize.Width() < RECENT_NARROW_WIDTH;
    if (bNarrow != mbRecentNarrow)
    {
        mbRecentNarrow = bNarrow;
        mxRecentColLocation->set_visible(!bNarrow);
    }
    if (rSize.Width() != mnRecentWidth)
    {
        // row sizes are measured on insert: refill so rows follow the new width
        mnRecentWidth = rSize.Width();
        fillRecentList();
    }
}

IMPL_LINK_NOARG(BackingWindow, RecentSearchHdl, weld::TextWidget&, void)
{
    fillRecentList();
}

IMPL_LINK_NOARG(BackingWindow, RecentActivatedHdl, const weld::TreeIter&, bool)
{
    const OUString aURL = mxRecentList->get_selected_id();
    if (!aURL.isEmpty())
        mxAllRecentThumbnails->openURL(aURL);
    return true;
}

IMPL_LINK_NOARG( BackingWindow, FilterHdl, weld::ComboBox&, void )
{
    applyFilter();
}

IMPL_LINK( BackingWindow, ToggleHdl, weld::Toggleable&, rButton, void )
{
    if (&rButton == mxRecentButton.get())
    {
        mxLocalView->Hide();
        // Dainvo: the recent files list replaces the thumbnail grid
        mxAllRecentThumbnails->Hide();
        mxRecentBox->show();
        mxRecentList->grab_focus();
        mxTemplateButton->set_active(false);
        mxActions->show();
    }
    else
    {
        mxAllRecentThumbnails->Hide();
        mxRecentBox->hide();
        initializeLocalView();
        mxLocalView->Show();
        mxLocalView->reload();
        mxLocalView->GrabFocus();
        mxRecentButton->set_active(false);
        mxActions->hide();
    }
    applyFilter();
}

IMPL_LINK( BackingWindow, ClickHdl, weld::Button&, rButton, void )
{
    // dispatch the appropriate URL and end the dialog
    if( &rButton == mxWriterAllButton.get() )
        dispatchURL( u"private:factory/swriter"_ustr );
    else if( &rButton == mxCalcAllButton.get() )
        dispatchURL( u"private:factory/scalc"_ustr );
    else if( &rButton == mxImpressAllButton.get() )
        dispatchURL( u"private:factory/simpress?slot=6686"_ustr );
    else if( &rButton == mxDrawAllButton.get() )
        dispatchURL( u"private:factory/sdraw"_ustr );
    else if( &rButton == mxDBAllButton.get() )
        dispatchURL( u"private:factory/sdatabase?Interactive"_ustr );
    else if( &rButton == mxMathAllButton.get() )
        dispatchURL( u"private:factory/smath"_ustr );
    else if (&rButton == mxSettingsButton.get())
    {
        Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
        dispatchURL(u".uno:OptionsTreeDialog"_ustr, OUString(), xFrame);
    }
    else if( &rButton == mxOpenButton.get() )
    {
        Reference< XDispatchProvider > xFrame( mxFrame, UNO_QUERY );

        dispatchURL( u".uno:Open"_ustr, OUString(), xFrame, { comphelper::makePropertyValue(u"Referer"_ustr, u"private:user"_ustr) } );
    }
    else if( &rButton == mxRemoteButton.get() )
    {
        Reference< XDispatchProvider > xFrame( mxFrame, UNO_QUERY );

        dispatchURL( u".uno:OpenRemote"_ustr, OUString(), xFrame, {} );
    }
}

IMPL_LINK (BackingWindow, MenuSelectHdl, const OUString&, rId, void)
{
    if (rId == "clear_all")
    {
        SvtHistoryOptions::Clear(EHistoryType::PickList, false);
        mxAllRecentThumbnails->Reload();
        return;
    }
    else if(rId == "clear_unavailable")
    {
        mxAllRecentThumbnails->clearUnavailableFiles();
    }
}

IMPL_LINK(BackingWindow, CreateContextMenuHdl, TemplateViewItem*, pItem, void)
{
    if (!pItem)
        return;

    bool bIsInternal = TemplateLocalView::IsInternalTemplate(pItem->getPath());

    mxLocalView->createContextMenu(bIsInternal);

}

IMPL_LINK(BackingWindow, OpenTemplateHdl, const OUString&, rTemplatePath, void)
{
    uno::Sequence< PropertyValue > aArgs{
        comphelper::makePropertyValue(u"AsTemplate"_ustr, true),
        comphelper::makePropertyValue(u"MacroExecutionMode"_ustr, MacroExecMode::USE_CONFIG),
        comphelper::makePropertyValue(u"UpdateDocMode"_ustr, UpdateDocMode::ACCORDING_TO_CONFIG),
        comphelper::makePropertyValue(u"InteractionHandler"_ustr, task::InteractionHandler::createWithParent( ::comphelper::getProcessComponentContext(), nullptr ))
    };

    Reference< XDispatchProvider > xFrame( mxFrame, UNO_QUERY );

    try
    {
        dispatchURL(rTemplatePath, u"_default"_ustr, xFrame, aArgs);
    }
    catch( const uno::Exception& )
    {
    }
}

IMPL_LINK(BackingWindow, EditTemplateHdl, const OUString&, rTemplatePath, void)
{
    uno::Sequence< PropertyValue > aArgs{
        comphelper::makePropertyValue(u"AsTemplate"_ustr, false),
        comphelper::makePropertyValue(u"MacroExecutionMode"_ustr, MacroExecMode::USE_CONFIG),
        comphelper::makePropertyValue(u"UpdateDocMode"_ustr, UpdateDocMode::ACCORDING_TO_CONFIG),
    };

    Reference< XDispatchProvider > xFrame( mxFrame, UNO_QUERY );

    try
    {
        dispatchURL(rTemplatePath, u"_default"_ustr, xFrame, aArgs);
    }
    catch( const uno::Exception& )
    {
    }
}

namespace {

struct ImplDelayedDispatch
{
    Reference< XDispatch >      xDispatch;
    css::util::URL   aDispatchURL;
    Sequence< PropertyValue >   aArgs;

    ImplDelayedDispatch( const Reference< XDispatch >& i_xDispatch,
                         css::util::URL i_aURL,
                         const Sequence< PropertyValue >& i_rArgs )
    : xDispatch( i_xDispatch ),
      aDispatchURL(std::move( i_aURL )),
      aArgs( i_rArgs )
    {
    }
};

}

static void implDispatchDelayed( void*, void* pArg )
{
    struct ImplDelayedDispatch* pDispatch = static_cast<ImplDelayedDispatch*>(pArg);
    try
    {
        pDispatch->xDispatch->dispatch( pDispatch->aDispatchURL, pDispatch->aArgs );
    }
    catch (const Exception&)
    {
    }

    // clean up
    delete pDispatch;
}

void BackingWindow::dispatchURL( const OUString& i_rURL,
                                 const OUString& rTarget,
                                 const Reference< XDispatchProvider >& i_xProv,
                                 const Sequence< PropertyValue >& i_rArgs )
{
    // if no special dispatch provider is given, get the desktop
    Reference< XDispatchProvider > xProvider( i_xProv.is() ? i_xProv : mxDesktopDispatchProvider );

    // check for dispatch provider
    if( !xProvider.is())
        return;

    // get a URL transformer to clean up the URL
    css::util::URL aDispatchURL;
    aDispatchURL.Complete = i_rURL;

    Reference < css::util::XURLTransformer > xURLTransformer(
        css::util::URLTransformer::create( comphelper::getProcessComponentContext() ) );
    try
    {
        // clean up the URL
        xURLTransformer->parseStrict( aDispatchURL );
        // get a Dispatch for the URL and target
        Reference< XDispatch > xDispatch(
            xProvider->queryDispatch( aDispatchURL, rTarget, 0 )
            );
        // dispatch the URL
        if ( xDispatch.is() )
        {
            std::unique_ptr<ImplDelayedDispatch> pDisp(new ImplDelayedDispatch( xDispatch, std::move(aDispatchURL), i_rArgs ));
            if( Application::PostUserEvent( LINK_NONMEMBER( nullptr, implDispatchDelayed ), pDisp.get() ) )
                pDisp.release();
        }
    }
    catch (const css::uno::RuntimeException&)
    {
        throw;
    }
    catch (const css::uno::Exception&)
    {
    }
}

void BackingWindow::clearRecentFileList()
{
    mxAllRecentThumbnails->Clear();
    // tdf#166349 - reload recent documents to show pinned items
    mxAllRecentThumbnails->Reload();
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab:*/
