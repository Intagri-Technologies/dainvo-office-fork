/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <linguistic/dainvolangpacks.hxx>
#include <vcl/timer.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/CheckButton.hxx>
#include <vcl/weld/DialogController.hxx>
#include <vcl/weld/Entry.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/ProgressBar.hxx>
#include <vcl/weld/TreeView.hxx>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <thread>

/// Dainvo Office: download and remove spelling, hyphenation and thesaurus
/// data (language packs). Opened by .uno:MoreDictionaries and the "Get more
/// dictionaries" links. See dainvo/docs/LANGUAGE_TOOLS.md.
class DainvoLanguagesDialog : public weld::GenericDialogController
{
public:
    DainvoLanguagesDialog(weld::Window* pParent, const OUString& rTag);
    virtual ~DainvoLanguagesDialog() override;

    struct JobState;

private:
    friend struct DainvoLanguagesDialogTestAccess; // cui/qa/unit/dainvolanguages.cxx

    enum Kind
    {
        SPELL = 0,
        HYPH = 1,
        THES = 2
    };

    struct Row
    {
        OUString aTag;
        OUString aDisplay;
        bool bSuggested = false;
        // per kind: id of the component offered by the index, if any
        std::array<OUString, 3> aOffered;
    };

    std::vector<Row> m_aRows;
    std::optional<linguistic::langpacks::Index> m_oIndex;
    std::vector<linguistic::langpacks::InstalledComponent> m_aInstalled;
    // pending changes: (language tag, kind) -> wanted installed state
    std::map<std::pair<OUString, int>, bool> m_aPending;
    // language tag -> spelling provided by the system speller (macOS)
    mutable std::map<OUString, bool> m_aSystemSpell;

    std::shared_ptr<JobState> m_pJob;
    std::thread m_aWorker;
    bool m_bFetchJob = false;
    bool m_bUpdatingChecks = false;
    Timer m_aPoll;

    std::unique_ptr<weld::Entry> m_xSearch;
    std::unique_ptr<weld::TreeView> m_xList;
    std::array<std::unique_ptr<weld::CheckButton>, 3> m_aChecks;
    std::unique_ptr<weld::Label> m_xDetails;
    std::unique_ptr<weld::ProgressBar> m_xProgress;
    std::unique_ptr<weld::Label> m_xStatus;
    std::unique_ptr<weld::Button> m_xApply;
    std::unique_ptr<weld::Button> m_xStop;

    DECL_LINK(SearchHdl, weld::TextWidget&, void);
    DECL_LINK(SelectHdl, weld::ItemView&, void);
    DECL_LINK(ToggleHdl, weld::Toggleable&, void);
    DECL_LINK(ApplyHdl, weld::Button&, void);
    DECL_LINK(StopHdl, weld::Button&, void);
    DECL_LINK(PollHdl, Timer*, void);

    void StartFetch();
    void BuildRows();
    void FillList();
    void UpdateChecks();
    void SetBusy(bool bBusy);
    void FinishJob();

    /// installed component of this kind serving the language (user before bundled)
    const linguistic::langpacks::InstalledComponent* FindInstalled(const OUString& rTag,
                                                                    int nKind) const;
    bool IsProvidedBySystem(const OUString& rTag, int nKind) const;
    OUString StatusText(const Row& rRow) const;
    OUString SizeText(const Row& rRow) const;
    const Row* SelectedRow() const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
