/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dainvo Office Languages dialog: download and remove language packs.
// See dainvo/docs/LANGUAGE_TOOLS.md.

#include <sal/config.h>

#include <dainvolanguages.hxx>

#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/linguistic2/XSpellChecker.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <comphelper/processfactory.hxx>
#include <dialmgr.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <officecfg/Office/Linguistic.hxx>
#include <osl/thread.h>
#include <rtl/ustrbuf.hxx>
#include <svtools/langtab.hxx>
#include <unotools/localedatawrapper.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld/Window.hxx>

#include <strings.hrc>
#include <dainvolanguages.hrc>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>

using namespace css;
namespace lp = linguistic::langpacks;

struct DainvoLanguagesDialog::JobState
{
    std::mutex aMutex;
    std::atomic<bool> bCancel{ false };
    std::atomic<bool> bDone{ false };
    // guarded by aMutex
    OUString aStatus;
    int nPercent = -1;
    std::vector<OUString> aErrors;
    std::optional<lp::Index> oIndex;
    OUString aFetchError;
    bool bFromCache = false;
    bool bChanged = false;
};

namespace
{
constexpr const char* KIND_NAMES[3] = { "spell", "hyph", "thes" };

int kindIndex(std::u16string_view aKind)
{
    for (int i = 0; i < 3; ++i)
        if (aKind == OUString::createFromAscii(KIND_NAMES[i]))
            return i;
    return -1;
}

OUString kindLabel(int nKind)
{
    switch (nKind)
    {
        case 0:
            return CuiResId(RID_CUISTR_SPELL);
        case 1:
            return CuiResId(RID_CUISTR_HYPH);
        default:
            return CuiResId(RID_CUISTR_THES);
    }
}

bool servesLocale(const lp::InstalledComponent& rComp, const OUString& rTag)
{
    for (const lp::PackDictionary& rDict : rComp.aDictionaries)
        if (std::find(rDict.aLocales.begin(), rDict.aLocales.end(), rTag) != rDict.aLocales.end())
            return true;
    return false;
}

OUString formatSize(sal_Int64 nBytes)
{
    const LocaleDataWrapper& rLocale = Application::GetSettings().GetUILocaleDataWrapper();
    if (nBytes < 100000)
        return rLocale.getNum(std::max<sal_Int64>(1, (nBytes + 500) / 1000), 0) + " KB";
    return rLocale.getNum((nBytes + 50000) / 100000, 1) + " MB";
}

/// Tags the user probably writes in: UI and system language, default document languages.
std::set<OUString> suggestedTags()
{
    std::set<OUString> aTags;
    auto add = [&aTags](LanguageType eLang) {
        if (eLang != LANGUAGE_DONTKNOW && eLang != LANGUAGE_NONE && eLang != LANGUAGE_SYSTEM)
            aTags.insert(LanguageTag(eLang).getBcp47());
    };
    add(MsLangId::getSystemUILanguage());
    add(MsLangId::getSystemLanguage());
    try
    {
        for (const OUString& rTag :
             { officecfg::Office::Linguistic::General::DefaultLocale::get(),
               officecfg::Office::Linguistic::General::DefaultLocale_CJK::get(),
               officecfg::Office::Linguistic::General::DefaultLocale_CTL::get() })
            if (!rTag.isEmpty())
                aTags.insert(rTag);
    }
    catch (const uno::Exception&)
    {
    }
    return aTags;
}
}

DainvoLanguagesDialog::DainvoLanguagesDialog(weld::Window* pParent, const OUString& /*rTag*/)
    : GenericDialogController(pParent, u"cui/ui/dainvolanguagesdialog.ui"_ustr,
                              u"DainvoLanguagesDialog"_ustr)
    , m_aPoll("DainvoLanguagesDialog m_aPoll")
    , m_xSearch(m_xBuilder->weld_entry(u"search"_ustr))
    , m_xList(m_xBuilder->weld_tree_view(u"languages"_ustr))
    , m_aChecks{ m_xBuilder->weld_check_button(u"spell"_ustr),
                 m_xBuilder->weld_check_button(u"hyph"_ustr),
                 m_xBuilder->weld_check_button(u"thes"_ustr) }
    , m_xDetails(m_xBuilder->weld_label(u"details"_ustr))
    , m_xProgress(m_xBuilder->weld_progress_bar(u"progress"_ustr))
    , m_xStatus(m_xBuilder->weld_label(u"status"_ustr))
    , m_xApply(m_xBuilder->weld_button(u"apply"_ustr))
    , m_xStop(m_xBuilder->weld_button(u"stop"_ustr))
{
    m_xList->set_size_request(m_xList->get_approximate_digit_width() * 70,
                              m_xList->get_height_rows(12));
    const int nDigit = m_xList->get_approximate_digit_width();
    m_xList->set_column_fixed_widths({ nDigit * 34, nDigit * 26 });

    m_xSearch->connect_changed(LINK(this, DainvoLanguagesDialog, SearchHdl));
    m_xList->connect_selection_changed(LINK(this, DainvoLanguagesDialog, SelectHdl));
    for (auto& rCheck : m_aChecks)
        rCheck->connect_toggled(LINK(this, DainvoLanguagesDialog, ToggleHdl));
    m_xApply->connect_clicked(LINK(this, DainvoLanguagesDialog, ApplyHdl));
    m_xStop->connect_clicked(LINK(this, DainvoLanguagesDialog, StopHdl));
    m_aPoll.SetTimeout(100);
    m_aPoll.SetInvokeHandler(LINK(this, DainvoLanguagesDialog, PollHdl));

    m_xProgress->set_percentage(0);
    m_aInstalled = lp::GetInstalledComponents();
    BuildRows();
    FillList();
    UpdateChecks();

    if (lp::IsDownloadEnabled())
        StartFetch();
    else
    {
        m_oIndex = lp::LoadCachedIndex();
        m_xStatus->set_label(CuiResId(RID_CUISTR_DAINVOLANG_DISABLED));
        BuildRows();
        FillList();
    }
}

DainvoLanguagesDialog::~DainvoLanguagesDialog()
{
    m_aPoll.Stop();
    if (m_aWorker.joinable())
    {
        m_pJob->bCancel = true;
        m_aWorker.join();
        // finished downloads must still be used
        bool bChanged = false;
        {
            std::scoped_lock aGuard(m_pJob->aMutex);
            bChanged = m_pJob->bChanged;
        }
        if (bChanged)
            lp::ReloadLinguServices();
    }
}

void DainvoLanguagesDialog::StartFetch()
{
    m_pJob = std::make_shared<JobState>();
    m_bFetchJob = true;
    m_xStatus->set_label(CuiResId(RID_CUISTR_DAINVOLANG_LOADING));
    std::shared_ptr<JobState> pJob = m_pJob;
    m_aWorker = std::thread([pJob] {
        osl_setThreadName("DainvoLanguagesFetch");
        OUString aError;
        std::optional<lp::Index> oIndex
            = lp::FetchIndex(aError, [&pJob](sal_Int64, sal_Int64) { return !pJob->bCancel; });
        bool bFromCache = false;
        if (!oIndex)
        {
            oIndex = lp::LoadCachedIndex();
            bFromCache = oIndex.has_value();
        }
        std::scoped_lock aGuard(pJob->aMutex);
        pJob->oIndex = std::move(oIndex);
        pJob->aFetchError = aError;
        pJob->bFromCache = bFromCache;
        pJob->bDone = true;
    });
    SetBusy(true);
    m_aPoll.Start();
}

void DainvoLanguagesDialog::BuildRows()
{
    const std::set<OUString> aSuggested = suggestedTags();
    std::map<OUString, Row> aRows;
    std::map<OUString, std::pair<OUString, OUString>> aNames; // tag -> (name, autonym)

    if (m_oIndex)
    {
        for (const lp::IndexLanguage& rLang : m_oIndex->aLanguages)
        {
            Row& rRow = aRows[rLang.aTag];
            rRow.aTag = rLang.aTag;
            aNames[rLang.aTag] = { rLang.aName, rLang.aAutonym };
            for (const OUString& rId : rLang.aComponents)
            {
                int nKind = kindIndex(m_oIndex->aComponents.at(rId).aKind);
                if (nKind >= 0 && rRow.aOffered[nKind].isEmpty())
                    rRow.aOffered[nKind] = rId;
            }
        }
    }
    // installed languages, also when the index is not available
    for (const lp::InstalledComponent& rComp : m_aInstalled)
    {
        const OUString aTag = rComp.aId.copy(0, rComp.aId.lastIndexOf('.'));
        aRows[aTag].aTag = aTag;
    }

    m_aRows.clear();
    for (auto& [rTag, rRow] : aRows)
    {
        LanguageTag aLangTag(rTag);
        const LanguageType eLang = aLangTag.getLanguageType(false);
        OUString aLocal = eLang != LANGUAGE_DONTKNOW ? SvtLanguageTable::GetLanguageString(eLang)
                                                     : OUString();
        const auto it = aNames.find(rTag);
        if (aLocal.isEmpty() && it != aNames.end())
            aLocal = it->second.first;
        if (aLocal.isEmpty())
            aLocal = rTag;
        rRow.aDisplay = aLocal;
        if (it != aNames.end() && !it->second.second.isEmpty() && it->second.second != aLocal)
            rRow.aDisplay = it->second.second + u" – " + aLocal;

        rRow.bSuggested = aSuggested.count(rTag) > 0;
        if (!rRow.bSuggested)
            for (const OUString& rSuggested : aSuggested)
                if (rSuggested.indexOf('-') < 0 && rTag.startsWith(OUString(rSuggested + "-")))
                    rRow.bSuggested = true;
        m_aRows.push_back(rRow);
    }
    std::sort(m_aRows.begin(), m_aRows.end(), [](const Row& a, const Row& b) {
        if (a.bSuggested != b.bSuggested)
            return a.bSuggested;
        return a.aDisplay.compareToIgnoreAsciiCase(b.aDisplay) < 0;
    });
}

void DainvoLanguagesDialog::FillList()
{
    const OUString aSelected = m_xList->get_selected_id();
    const OUString aFilter = m_xSearch->get_text().trim().toAsciiLowerCase();
    m_xList->freeze();
    m_xList->clear();
    int nSelect = -1;
    for (const Row& rRow : m_aRows)
    {
        if (!aFilter.isEmpty() && rRow.aDisplay.toAsciiLowerCase().indexOf(aFilter) < 0
            && rRow.aTag.toAsciiLowerCase().indexOf(aFilter) < 0)
            continue;
        m_xList->append(rRow.aTag, rRow.aDisplay);
        const int nPos = m_xList->n_children() - 1;
        m_xList->set_text(nPos, StatusText(rRow), 1);
        m_xList->set_text(nPos, SizeText(rRow), 2);
        if (rRow.bSuggested)
            m_xList->set_text_emphasis(nPos, true, 0);
        if (rRow.aTag == aSelected)
            nSelect = nPos;
    }
    m_xList->thaw();
    if (nSelect >= 0)
        m_xList->select(nSelect);
    UpdateChecks();
}

const lp::InstalledComponent* DainvoLanguagesDialog::FindInstalled(const OUString& rTag,
                                                                   int nKind) const
{
    const OUString aKind = OUString::createFromAscii(KIND_NAMES[nKind]);
    for (const lp::InstalledComponent& rComp : m_aInstalled) // user ones come first
        if (rComp.aKind == aKind && servesLocale(rComp, rTag))
            return &rComp;
    return nullptr;
}

bool DainvoLanguagesDialog::IsProvidedBySystem(const OUString& rTag, int nKind) const
{
#ifdef MACOSX
    if (nKind != SPELL)
        return false;
    auto it = m_aSystemSpell.find(rTag);
    if (it != m_aSystemSpell.end())
        return it->second;
    bool bHas = false;
    try
    {
        uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext());
        uno::Reference<linguistic2::XSpellChecker> xMac(
            xContext->getServiceManager()->createInstanceWithContext(
                u"org.openoffice.lingu.MacOSXSpellChecker"_ustr, xContext),
            uno::UNO_QUERY);
        bHas = xMac.is() && xMac->hasLocale(LanguageTag(rTag).getLocale(false));
    }
    catch (const uno::Exception&)
    {
    }
    m_aSystemSpell[rTag] = bHas;
    return bHas;
#else
    (void)rTag;
    (void)nKind;
    return false;
#endif
}

OUString DainvoLanguagesDialog::StatusText(const Row& rRow) const
{
    OUStringBuffer aBuf;
    for (int nKind = 0; nKind < 3; ++nKind)
    {
        const lp::InstalledComponent* pComp = FindInstalled(rRow.aTag, nKind);
        OUString aPart;
        if (pComp)
        {
            aPart = kindLabel(nKind);
            if (pComp->bBundled)
                aPart += " (" + CuiResId(RID_CUISTR_DAINVOLANG_BUILT_IN) + ")";
            else if (m_oIndex && !rRow.aOffered[nKind].isEmpty()
                     && m_oIndex->aComponents.at(rRow.aOffered[nKind]).aVersion
                            != pComp->aVersion
                     && rRow.aOffered[nKind] == pComp->aId)
                aPart += " (" + CuiResId(RID_CUISTR_DAINVOLANG_UPDATE) + ")";
        }
        else if (IsProvidedBySystem(rRow.aTag, nKind))
            aPart = kindLabel(nKind) + " (macOS)";
        if (!aPart.isEmpty())
        {
            if (!aBuf.isEmpty())
                aBuf.append(", ");
            aBuf.append(aPart);
        }
    }
    return aBuf.isEmpty() ? CuiResId(RID_CUISTR_DAINVOLANG_NOT_INSTALLED) : aBuf.makeStringAndClear();
}

OUString DainvoLanguagesDialog::SizeText(const Row& rRow) const
{
    if (!m_oIndex)
        return OUString();
    sal_Int64 nSize = 0;
    std::set<OUString> aCounted;
    for (int nKind = 0; nKind < 3; ++nKind)
    {
        const OUString& rId = rRow.aOffered[nKind];
        if (rId.isEmpty() || FindInstalled(rRow.aTag, nKind) || !aCounted.insert(rId).second)
            continue;
        if (nKind == SPELL && IsProvidedBySystem(rRow.aTag, nKind))
            continue;
        nSize += m_oIndex->aComponents.at(rId).nSize;
    }
    return nSize ? formatSize(nSize) : OUString();
}

const DainvoLanguagesDialog::Row* DainvoLanguagesDialog::SelectedRow() const
{
    const OUString aTag = m_xList->get_selected_id();
    for (const Row& rRow : m_aRows)
        if (rRow.aTag == aTag)
            return &rRow;
    return nullptr;
}

void DainvoLanguagesDialog::UpdateChecks()
{
    m_bUpdatingChecks = true;
    const Row* pRow = SelectedRow();
    const bool bBusy = m_aWorker.joinable();
    const bool bDownloads = lp::IsDownloadEnabled();
    OUStringBuffer aDetails;
    for (int nKind = 0; nKind < 3; ++nKind)
    {
        weld::CheckButton& rCheck = *m_aChecks[nKind];
        if (!pRow)
        {
            rCheck.set_active(false);
            rCheck.set_sensitive(false);
            continue;
        }
        const lp::InstalledComponent* pComp = FindInstalled(pRow->aTag, nKind);
        const OUString& rOffered = pRow->aOffered[nKind];
        bool bWanted = pComp != nullptr;
        auto it = m_aPending.find({ pRow->aTag, nKind });
        if (it != m_aPending.end())
            bWanted = it->second;
        const bool bSensitive
            = !bBusy
              && ((pComp && !pComp->bBundled) || (!pComp && !rOffered.isEmpty() && bDownloads));
        rCheck.set_active(bWanted);
        rCheck.set_sensitive(bSensitive);

        // details: what is installed or offered, with version and licence
        OUString aVersion, aLicense;
        if (pComp)
        {
            aVersion = pComp->aVersion;
            aLicense = pComp->aLicense;
        }
        else if (!rOffered.isEmpty() && m_oIndex)
        {
            aVersion = m_oIndex->aComponents.at(rOffered).aVersion;
            aLicense = m_oIndex->aComponents.at(rOffered).aLicense;
        }
        if (!aVersion.isEmpty())
        {
            if (!aDetails.isEmpty())
                aDetails.append("\n");
            aDetails.append(CuiResId(RID_CUISTR_DAINVOLANG_LICENSE)
                                .replaceFirst("%1", kindLabel(nKind))
                                .replaceFirst("%2", aVersion)
                                .replaceFirst("%3", aLicense));
            if (pComp && pComp->bBundled)
                aDetails.append(" "
                                + CuiResId(RID_CUISTR_DAINVOLANG_BUILTIN_NOTE)
                                      .replaceFirst("%1", kindLabel(nKind)));
        }
    }
    if (pRow && IsProvidedBySystem(pRow->aTag, SPELL) && !FindInstalled(pRow->aTag, SPELL))
    {
        if (!aDetails.isEmpty())
            aDetails.append("\n");
        aDetails.append(CuiResId(RID_CUISTR_DAINVOLANG_MACOS_SPELL));
    }
    m_xDetails->set_label(aDetails.makeStringAndClear());

    // Apply: pending changes, or updates for installed packs
    bool bApply = !m_aPending.empty();
    if (!bApply && m_oIndex)
        for (const lp::InstalledComponent& rComp : m_aInstalled)
        {
            auto itComp = m_oIndex->aComponents.find(rComp.aId);
            if (!rComp.bBundled && itComp != m_oIndex->aComponents.end()
                && itComp->second.aVersion != rComp.aVersion)
                bApply = true;
        }
    m_xApply->set_sensitive(!bBusy && bApply && (bDownloads || !m_aPending.empty()));
    m_bUpdatingChecks = false;
}

void DainvoLanguagesDialog::SetBusy(bool bBusy)
{
    m_xStop->set_sensitive(bBusy);
    m_xSearch->set_sensitive(!bBusy || m_bFetchJob);
    m_xList->set_sensitive(!bBusy || m_bFetchJob);
    if (!bBusy)
        m_xProgress->set_percentage(0);
    UpdateChecks();
}

IMPL_LINK_NOARG(DainvoLanguagesDialog, SearchHdl, weld::TextWidget&, void) { FillList(); }

IMPL_LINK_NOARG(DainvoLanguagesDialog, SelectHdl, weld::ItemView&, void) { UpdateChecks(); }

IMPL_LINK(DainvoLanguagesDialog, ToggleHdl, weld::Toggleable&, rToggle, void)
{
    if (m_bUpdatingChecks)
        return;
    const Row* pRow = SelectedRow();
    if (!pRow)
        return;
    for (int nKind = 0; nKind < 3; ++nKind)
    {
        if (&rToggle != m_aChecks[nKind].get())
            continue;
        const bool bInstalled = FindInstalled(pRow->aTag, nKind) != nullptr;
        if (rToggle.get_active() == bInstalled)
            m_aPending.erase({ pRow->aTag, nKind });
        else
            m_aPending[{ pRow->aTag, nKind }] = rToggle.get_active();
    }
    UpdateChecks();
}

IMPL_LINK_NOARG(DainvoLanguagesDialog, ApplyHdl, weld::Button&, void)
{
    struct Action
    {
        bool bInstall;
        lp::IndexComponent aComp; // for installs
        OUString aId; // for removals
        OUString aLabel;
    };
    if (m_aWorker.joinable())
        return; // one job at a time
    std::vector<Action> aActions;
    std::set<OUString> aSeen;
    auto rowLabel = [this](const OUString& rTag) {
        for (const Row& rRow : m_aRows)
            if (rRow.aTag == rTag)
                return rRow.aDisplay;
        return rTag;
    };

    for (const auto& [rKey, bWanted] : m_aPending)
    {
        const auto& [rTag, nKind] = rKey;
        const OUString aLabel = rowLabel(rTag) + u": " + kindLabel(nKind);
        const lp::InstalledComponent* pComp = FindInstalled(rTag, nKind);
        if (bWanted && !pComp && m_oIndex)
        {
            auto itRow = std::find_if(m_aRows.begin(), m_aRows.end(),
                                      [&](const Row& r) { return r.aTag == rTag; });
            if (itRow != m_aRows.end() && !itRow->aOffered[nKind].isEmpty()
                && aSeen.insert(itRow->aOffered[nKind]).second)
                aActions.push_back(
                    { true, m_oIndex->aComponents.at(itRow->aOffered[nKind]), {}, aLabel });
        }
        else if (!bWanted && pComp && !pComp->bBundled && aSeen.insert(pComp->aId).second)
            aActions.push_back({ false, {}, pComp->aId, aLabel });
    }
    // updates of installed packs
    if (m_oIndex && lp::IsDownloadEnabled())
        for (const lp::InstalledComponent& rComp : m_aInstalled)
        {
            auto it = m_oIndex->aComponents.find(rComp.aId);
            if (!rComp.bBundled && it != m_oIndex->aComponents.end()
                && it->second.aVersion != rComp.aVersion && aSeen.insert(rComp.aId).second)
            {
                const OUString aTag = rComp.aId.copy(0, rComp.aId.lastIndexOf('.'));
                aActions.push_back({ true, it->second, {},
                                     rowLabel(aTag) + u": " + kindLabel(kindIndex(rComp.aKind)) });
            }
        }
    if (aActions.empty())
        return;

    m_pJob = std::make_shared<JobState>();
    m_bFetchJob = false;
    std::shared_ptr<JobState> pJob = m_pJob;
    const OUString aDownloading = CuiResId(RID_CUISTR_DAINVOLANG_DOWNLOADING);
    const OUString aRemoving = CuiResId(RID_CUISTR_DAINVOLANG_REMOVING);
    const OUString aFailed = CuiResId(RID_CUISTR_DAINVOLANG_FAILED);
    m_aWorker = std::thread([pJob, aActions = std::move(aActions), aDownloading, aRemoving,
                             aFailed] {
        osl_setThreadName("DainvoLanguagesApply");
        for (const Action& rAction : aActions)
        {
            if (pJob->bCancel)
                break;
            {
                std::scoped_lock aGuard(pJob->aMutex);
                pJob->aStatus = (rAction.bInstall ? aDownloading : aRemoving)
                                    .replaceFirst("%1", rAction.aLabel);
                pJob->nPercent = rAction.bInstall ? 0 : -1;
            }
            OUString aError;
            bool bOk;
            if (rAction.bInstall)
                bOk = lp::InstallComponent(
                    rAction.aComp,
                    [&pJob](sal_Int64 nDone, sal_Int64 nTotal) {
                        if (nTotal > 0)
                        {
                            std::scoped_lock aGuard(pJob->aMutex);
                            pJob->nPercent = int(std::min<sal_Int64>(100, nDone * 100 / nTotal));
                        }
                        return !pJob->bCancel;
                    },
                    aError);
            else
                bOk = lp::RemoveComponent(rAction.aId, aError);
            std::scoped_lock aGuard(pJob->aMutex);
            if (bOk)
                pJob->bChanged = true;
            else if (!aError.isEmpty())
                pJob->aErrors.push_back(
                    aFailed.replaceFirst("%1", rAction.aLabel).replaceFirst("%2", aError));
        }
        pJob->bDone = true;
    });
    SetBusy(true);
    m_aPoll.Start();
}

IMPL_LINK_NOARG(DainvoLanguagesDialog, StopHdl, weld::Button&, void)
{
    if (m_pJob)
        m_pJob->bCancel = true;
}

IMPL_LINK_NOARG(DainvoLanguagesDialog, PollHdl, Timer*, void)
{
    if (!m_pJob)
        return;
    {
        std::scoped_lock aGuard(m_pJob->aMutex);
        if (!m_pJob->aStatus.isEmpty())
            m_xStatus->set_label(m_pJob->aStatus);
        if (m_pJob->nPercent >= 0)
            m_xProgress->set_percentage(m_pJob->nPercent);
    }
    if (!m_pJob->bDone)
    {
        m_aPoll.Start();
        return;
    }
    m_aWorker.join();
    FinishJob();
}

void DainvoLanguagesDialog::FinishJob()
{
    std::shared_ptr<JobState> pJob = m_pJob;
    std::scoped_lock aGuard(pJob->aMutex);
    if (m_bFetchJob)
    {
        m_oIndex = std::move(pJob->oIndex);
        OUString aStatus;
        if (!pJob->aFetchError.isEmpty())
        {
            aStatus = CuiResId(RID_CUISTR_DAINVOLANG_OFFLINE).replaceFirst("%1", pJob->aFetchError);
            if (pJob->bFromCache)
                aStatus += "\n" + CuiResId(RID_CUISTR_DAINVOLANG_CACHED);
        }
        m_xStatus->set_label(aStatus);
        m_bFetchJob = false;
        BuildRows();
    }
    else
    {
        if (pJob->bChanged)
            lp::ReloadLinguServices();
        m_aInstalled = lp::GetInstalledComponents();
        m_aPending.clear();
        OUString aStatus;
        for (const OUString& rError : pJob->aErrors)
            aStatus += (aStatus.isEmpty() ? u""_ustr : u"\n"_ustr) + rError;
        if (aStatus.isEmpty())
            aStatus = CuiResId(pJob->bCancel ? RID_CUISTR_DAINVOLANG_STOPPED
                                             : RID_CUISTR_DAINVOLANG_DONE);
        m_xStatus->set_label(aStatus);
        BuildRows();
    }
    m_pJob.reset();
    SetBusy(false);
    FillList();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
