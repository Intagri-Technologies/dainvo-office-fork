/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dainvo Office language packs: spelling (Hunspell), hyphenation (Hyphen) and
// thesaurus (MyThes) data that users download per user, without extensions.
// Design: dainvo/docs/LANGUAGE_TOOLS.md.
//
// On disk, a component lives in <root>/<language>/<kind>-<version>/ with a
// pack.json describing its dictionaries. Roots, highest priority first:
//   1. <UserInstallation>/user/languages      (downloads, per user)
//   2. $BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/languages   (bundled with the app)

#pragma once

#include <linguistic/lngdllapi.h>
#include <rtl/ustring.hxx>
#include <sal/types.h>

#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <vector>

struct SvtLinguConfigDictionaryEntry;

namespace linguistic::langpacks
{
/// One dictionary inside a component, as listed in pack.json.
struct PackDictionary
{
    OUString aFormat; ///< DICT_SPELL, DICT_HYPH or DICT_THES
    std::vector<OUString> aFiles; ///< absolute file URLs
    std::vector<OUString> aLocales; ///< BCP 47 tags
};

/// A component (one spelling list, hyphenation set or thesaurus) found on disk.
struct InstalledComponent
{
    OUString aId; ///< e.g. "de-DE.hyph"
    OUString aKind; ///< "spell", "hyph" or "thes"
    OUString aVersion;
    OUString aDirUrl; ///< file URL of the <kind>-<version> folder
    OUString aLicense; ///< SPDX expression
    bool bBundled = false;
    std::vector<PackDictionary> aDictionaries;
};

/// A component offered by the download index.
struct IndexComponent
{
    OUString aId;
    OUString aKind;
    OUString aVersion;
    OUString aUrl;
    OUString aSha256; ///< lower-case hex
    OUString aLicense;
    sal_Int64 nSize = 0;
    sal_Int64 nInstalledSize = 0;
    std::vector<OUString> aLocales;
};

/// A language offered by the download index.
struct IndexLanguage
{
    OUString aTag;
    OUString aName;
    OUString aAutonym;
    std::vector<OUString> aComponents; ///< ids into Index::aComponents
};

struct Index
{
    sal_Int64 nSequence = 0;
    sal_Int64 nMinAppBuild = 0;
    std::vector<IndexLanguage> aLanguages;
    std::map<OUString, IndexComponent> aComponents;
};

/// Progress callback: bytes done, bytes expected (0 if unknown). Return false to cancel.
using ProgressFn = std::function<bool(sal_Int64, sal_Int64)>;

/// file URL of the per-user root, <UserInstallation>/user/languages
LNG_DLLPUBLIC OUString GetUserRootUrl();
/// file URL of the bundled root inside the installation
LNG_DLLPUBLIC OUString GetBundledRootUrl();

/// Installed components, user ones first. Per root and id only the highest
/// version is returned. Broken components are skipped.
LNG_DLLPUBLIC std::vector<InstalledComponent> GetInstalledComponents();

/// Dictionary entries of the given format (DICT_SPELL, DICT_HYPH, DICT_THES)
/// from all installed components. A locale is served by the first component
/// that has it (user packs before bundled ones).
LNG_DLLPUBLIC std::vector<SvtLinguConfigDictionaryEntry>
GetDictionaryEntries(std::u16string_view aFormat);

/// Remove leftovers: the staging folder, superseded versions and folders
/// marked for removal. Safe to call at any time; files in use are skipped.
LNG_DLLPUBLIC void Cleanup();

/// Whether downloads are allowed and an index URL and key are configured.
LNG_DLLPUBLIC bool IsDownloadEnabled();

/// Download (or read, for file: URLs) the index and its signature, verify
/// both and the sequence number, and cache them in the user root.
LNG_DLLPUBLIC std::optional<Index> FetchIndex(OUString& rError,
                                              const ProgressFn& rProgress = {});

/// The last verified index from the cache, if any.
LNG_DLLPUBLIC std::optional<Index> LoadCachedIndex();

/// Parse and verify an index. Exposed for tests.
LNG_DLLPUBLIC std::optional<Index> ParseVerifiedIndex(const std::vector<char>& rIndex,
                                                      const std::vector<char>& rSignature,
                                                      const OUString& rPublicKeyBase64,
                                                      OUString& rError);

/// Ed25519 signature check. Exposed for tests.
LNG_DLLPUBLIC bool VerifyEd25519(const std::vector<char>& rData,
                                 const std::vector<unsigned char>& rSignature,
                                 const std::vector<unsigned char>& rPublicKey);

/// Download, check and install one component into the user root.
LNG_DLLPUBLIC bool InstallComponent(const IndexComponent& rComponent, const ProgressFn& rProgress,
                                    OUString& rError);

/// Install a component from a local zip whose SHA-256 is already known to be
/// good (used by InstallComponent and by tests).
LNG_DLLPUBLIC bool InstallComponentFromZip(const OUString& rZipUrl, const IndexComponent& rComponent,
                                           OUString& rError);

/// Remove an installed user component. Folders that are in use are marked
/// and deleted by the next Cleanup().
LNG_DLLPUBLIC bool RemoveComponent(const OUString& rId, OUString& rError);

/// Make the spelling, hyphenation and thesaurus services re-read their data
/// and ask open documents to check again. Call on the main thread.
LNG_DLLPUBLIC void ReloadLinguServices();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
