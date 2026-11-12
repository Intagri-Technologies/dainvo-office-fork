# Dainvo Office: downloadable language tools

Status: **phase 1 built** on 2026-11-12 (macOS build tested; Windows and Linux
compile paths not yet built), not committed. Owner decisions (section 10): the
macOS speller stays first, packs live in their own repo
`Intagri-Technologies/dainvo-office-languages`, and the index is signed with
its own key. Section 0 describes what was built; sections 1 to 10 are the
design it follows, with the differences listed in 0.3.

How Dainvo Office lets people add spelling, hyphenation and thesaurus data for
their own languages on macOS, Windows and Linux, without bringing back
extension support. Grammar checking is covered in section 6, but it comes
second because Dainvo plans its own grammar and AI tools.

## 0. What is built (phase 1)

### 0.1 Pieces

| Piece | Where |
|---|---|
| Pack engine: scan, index download and Ed25519 check, install, remove, cleanup, reload | `include/linguistic/dainvolangpacks.hxx`, `linguistic/source/dainvolangpacks.cxx` (in `liblnglo`, links OpenSSL and curl) |
| Dictionary discovery uses packs first, then (Linux) system folders; per-locale merge | `lingucomponent/source/lingutil/lingutil.cxx` |
| No restart: `css::util::XRefreshable` on Hunspell, Hyphen and MyThes services | `lingucomponent/source/spellcheck/spell/sspellimp.*`, `hyphenator/hyphen/hyphenimp.*`, `thesaurus/libnth/nthesimp.*` |
| Settings: `Linguistic/DainvoLanguagePacks/{AllowDownloads, IndexURL, PublicKey, LastSequence}` | `officecfg/registry/schema/org/openoffice/Office/Linguistic.xcs` |
| Bundled US English spelling, US and UK hyphenation (about 0.8 MB) | `dainvo/languages/bundled/*/pack.json`, `lingucomponent/Package_dainvo_languages.mk`, one line in `Repository.mk` |
| Languages dialog | `cui/source/dialogs/dainvolanguages.cxx`, `cui/source/inc/dainvolanguages.hxx`, `cui/inc/dainvolanguages.hrc`, `cui/uiconfig/ui/dainvolanguagesdialog.ui` |
| Entry points | Review tab "Languages" button in Writer, Calc and Impress (`.uno:MoreDictionaries`, re-enabled in `Commands.xcu`, label in `GenericCommands.xcu`, `dainvo/ribbon/ribbons.json` and `generate.py`, Lucide `languages` icon via `dainvo/icons/ribbon_icons.py`); "Download more languages..." links in Options > Writing Aids and Edit Modules. All go through `AbstractDialogFactory_Impl::CreateAdditionsDialog` with the "Dictionary" tag (`cui/source/factory/dlgfact.cxx`) |
| Publishing tool | `dainvo/tools/langpacks.py` (build, licenses, index, sign, verify, publish, keygen), `dainvo/languages/licenses.json` |
| Tests | `CppunitTest_linguistic_dainvolangpacks` (6 tests; `testLiveIndex` downloads from the real index only when `DAINVO_LANGPACKS_LIVE_TEST=1` is set: Ed25519 vectors from RFC 8032, signed index, bundled English, install/refresh/remove, checksum, zip-slip and mismatch refusal) and `CppunitTest_cui_dainvolanguages` (the dialog headless: fetch, install, in use without restart, remove, older index refused). Test data in `linguistic/qa/data/langpacks`, signed with the public RFC 8032 test key |

### 0.2 Signing key

The private key is in the login keychain of the release machine: generic
password, service "Dainvo language packs signing key", account
`dainvo-languages` (base64 of the 32-byte Ed25519 seed). The public key
`6CDsCCtcugNsLEyShLzl55T4wP4HNb2zBL55ONYSWqk=` is the default of
`DainvoLanguagePacks/PublicKey`. **Back the private key up** (for example in
the password manager):

```bash
security find-generic-password -a dainvo-languages -s "Dainvo language packs signing key" -w
```

For CI, put that value in a `DAINVO_LANGUAGES_KEY` secret; `langpacks.py sign`
reads it there, from `--key FILE`, or from the keychain on macOS.

### 0.3 Differences from the design below

- **Settings are configuration, not configure options.** Index URL and public
  key are `Linguistic.xcs` defaults (no `--with-dainvo-languages-*`, no
  autogen run needed). An administrator can lock them, and `AllowDownloads`.
- **OpenSSL instead of NSS** for Ed25519 (`EVP_PKEY_ED25519`): simpler raw-key
  API, bundled on all three platforms. SHA-256 uses `comphelper::Hash`.
- **Component URLs may be relative** to the index's own address, so a release
  (or a local folder) can hold the index and its packs together.
- **The dialog runs downloads on a worker thread** and polls it; closing the
  dialog mid-download cancels cleanly. It applies all ticked changes at once
  and also installs updates of installed packs.
- **Component ids** follow the first locale of each LibreOffice dictionary
  entry, e.g. `de.thes` (shared by de, de-DE, de-AT) and `de-AT.spell`.
- **Suggested languages** (system and default document languages) are listed
  first, in bold. Language names come from Dainvo's own translated list.

### 0.4 Publishing packs

The public repo
[`Intagri-Technologies/dainvo-office-languages`](https://github.com/Intagri-Technologies/dainvo-office-languages)
was created on 2026-11-12. Its first release, `2026.11.1` (index sequence 1),
holds the 8 English packs (en-US, en-GB, en-AU, en-CA, en-ZA spelling, US and
UK hyphenation, the English thesaurus; 18 English language variants).
`langpacks.py verify --remote --download` passed against it.

The second release, `2026.11.2` (sequence 2), added every other dictionary in
the LibreOffice dictionaries repo: 172 packs in all (about 109 MB), covering
162 language variants. The 8 English packs are unchanged and still served from
`2026.11.1`. Left out because no licence is stated anywhere in their files:
Guarani spelling (`gug-PY.spell`) and Odia spelling (`or-IN.spell`). Where a
folder states an open licence without a clear version, `licenses.json` says
`LicenseRef-see-included-files` (11 packs; the licence files ship inside each
pack). The `.idx` files that `langpacks.py` generates are byte-identical to
LibreOffice's `idxdict` for all 32 thesauri.

For the next publish:

```bash
python3 dainvo/tools/langpacks.py licenses
```

Review the licence files it lists and add each component to
`dainvo/languages/licenses.json` (the build refuses unlisted ones).

```bash
python3 dainvo/tools/langpacks.py build --skip-unreviewed
```

```bash
python3 dainvo/tools/langpacks.py index --tag 2026.11.3
```

```bash
python3 dainvo/tools/langpacks.py sign
```

```bash
python3 dainvo/tools/langpacks.py verify
```

```bash
python3 dainvo/tools/langpacks.py publish --tag 2026.11.3
```

`index` reads the published index, keeps the old URLs of unchanged packs, raises `sequence` and uploads
only new or changed packs. Packs go to `dainvo/dist/languages` (git-ignored
with the rest of `dainvo/dist`).

To try packs locally without publishing, build into a folder, run `index
--first --base-url ''` and `sign`, and point `DainvoLanguagePacks/IndexURL`
(Tools > Options > Advanced > Expert Configuration) at
`file:///…/index.json`.

### 0.5 Not done yet

- Windows and Linux builds of this code (it has no platform-specific parts
  except the macOS speller check in the dialog).
- Exact licence versions for the 11 `LicenseRef-see-included-files` packs, and licences for Guarani and Odia spelling (ask their maintainers).
- Phase 2: prompts when a language has no data, first-run suggestion,
  offline `.dainvolang` files, machine-wide folder.
- Autonyms in the index (names come from Dainvo's language list instead).

## 1. Recommendation in short

1. **Ship a small English set in the app**: US English spelling plus US and
   UK English hyphenation. That is about 0.8 MB on disk and about 0.3 MB of
   the download (section 7).
2. **Add Dainvo "language packs"**: zip files of standard Hunspell, Hyphen and
   MyThes data, listed in a **signed `index.json`** published as releases of a
   new public repo, `Intagri-Technologies/dainvo-office-languages`, signed
   with its own key. Packs are installed **per
   user** under the profile (`<profile>/user/languages/`) and found by a new
   folder scan in `lingucomponent`. No configuration is written and no
   extension machinery is used.
3. **No restart needed**: the Hunspell, Hyphen and MyThes services get a
   `refresh()` (`css::util::XRefreshable`), and the existing
   `LngSvcMgr::modified()` reload path re-reads them and asks documents to
   check spelling again.
4. **One "Languages" dialog** in `cui`. It opens from the Review ribbon tab
   (the existing `.uno:MoreDictionaries` command, re-enabled) and from
   Options > Language Settings > Writing Aids (the existing "Get more
   dictionaries online" link).
5. **Don't re-enable extensions**, not even for dictionaries only (section 5).
6. **Grammar**: ship none for now. Keep the built-in LanguageTool client
   hidden. Build Dainvo's own checker as a `css::linguistic2::Proofreader`
   component, which gets the squiggles, context menu and Spelling dialog
   without further work (section 6).

Phase 1 effort: about **3 to 3.5 weeks** for one developer (section 9).

## 2. Current state (verified in this tree)

- `--disable-extensions` gives `HAVE_FEATURE_EXTENSIONS 0`
  (`config_host/config_extensions.h`). There is no Extension Manager, so `.oxt`
  dictionaries can't be installed. `LngSvcMgr`'s constructor catches the
  missing `ExtensionManager` (`linguistic/source/lngsvcmgr.cxx:420-436`).
- `lingucomponent.xcd` registers `org.openoffice.lingu.MySpellSpellChecker`
  (Hunspell), `org.openoffice.lingu.LibHnjHyphenator`,
  `org.openoffice.lingu.new.Thesaurus` (MyThes) and
  `org.openoffice.lingu.LanguageToolGrammarChecker`. On macOS,
  `MacOSXSpellChecker` is registered too (`Library_MacOSXSpell`).
- These services find **no data**: the `dictionaries` submodule isn't packaged
  (`WITH_MYSPELL_DICTS` is empty), and `SYSTEM_DICTS` is off on macOS and
  Windows (`configure.ac:6918`). So on macOS only the system speller works. On
  Windows, nothing would work.
- `.uno:MoreDictionaries` is disabled (`DainvoMoreDictionaries` in
  `officecfg/.../Office/Commands.xcu`) and left out of the ribbons
  (`DROP` regex in `dainvo/ribbon/generate.py`). The LanguageTool options page
  is hidden in `OptionsDialog.xcu`.

## 3. How LibreOffice finds dictionary data

Each of the three data-driven services builds its locale list once, then
caches it. The calls are in `SpellChecker::getLocales()`
(`lingucomponent/source/spellcheck/spell/sspellimp.cxx:108`), and the
matching functions in `hyphenator/hyphen/hyphenimp.cxx` and
`thesaurus/libnth/nthesimp.cxx`:

```
dics = SvtLinguConfig().GetActiveDictionariesByFormat("DICT_SPELL" | "DICT_HYPH" | "DICT_THES")
MergeNewStyleDicsAndOldStyleDics(dics, GetOldStyleDics("DICT" | "HYPH" | "THES"))
```

### 3.1 "New style": the `Linguistic/ServiceManager/Dictionaries` config set

This is what a dictionary extension adds. Each node has three properties:

```xml
<oor:component-data oor:name="Linguistic" oor:package="org.openoffice.Office"
    xmlns:oor="http://openoffice.org/2001/registry" xmlns:xs="http://www.w3.org/2001/XMLSchema">
 <node oor:name="ServiceManager">
  <node oor:name="Dictionaries">
   <node oor:name="HyphDic_en-US" oor:op="fuse">
    <prop oor:name="Locations" oor:type="oor:string-list">
     <value>vnd.sun.star.expand:$BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/languages/en-US/hyph/hyph_en_US.dic</value>
    </prop>
    <prop oor:name="Format" oor:type="xs:string"><value>DICT_HYPH</value></prop>
    <prop oor:name="Locales" oor:type="oor:string-list"><value>en-US en-IL en-PH</value></prop>
   </node>
  </node>
 </node>
</oor:component-data>
```

- `Format` is `DICT_SPELL`, `DICT_HYPH` or `DICT_THES`.
- `Locations` lists **file URLs**. Spelling needs the `.aff` and the `.dic`.
  Hyphenation needs one `hyph_*.dic`. A thesaurus lists the `.dat`, but MyThes
  also needs a `.idx` with the same base name next to it
  (`nthesimp.cxx:265`). `SvtLinguConfig::GetDictionaryEntry`
  (`unotools/source/config/lingucfg.cxx:997`) passes every location through
  `comphelper::getExpandedUri` and drops any entry that isn't a `file:` URL.
  So `vnd.sun.star.expand:` macros work, such as `$BRAND_BASE_DIR`, or
  `${$BRAND_BASE_DIR/<etc>/bootstraprc:UserInstallation}` for the profile.
- `%origin%`, which the LibreOffice dictionary `.xcu` files use, is **only**
  replaced by the extension backend (`desktop/source/deployment/registry/configuration/dp_configuration.cxx:572`).
  It doesn't work in a plain `.xcu` layer.
- `Linguistic/ServiceManager/DisabledDictionaries` (string list of node names)
  switches entries off. This is the "Dictionaries" check list in Writing Aids.
- `Linguistic/General/DictionaryList/ActiveDictionaries` is about **user word
  lists** (`standard.dic` etc. in `Paths/Dictionary` = `$(userurl)/wordbook`),
  not Hunspell data. It's unrelated to this design.
- The `Paths/Linguistic` setting (`$(insturl)/share/dict`, `.../dict/ooo`) is a
  leftover. Nothing in `lingucomponent` reads it now (its only users are the
  Paths options page and `SvtPathOptions`), so it can't be used to register
  dictionaries.

### 3.2 "Old style": folder scan by file name

`GetOldStyleDics` (`lingucomponent/source/lingutil/lingutil.cxx:147`) runs only
when built with `SYSTEM_DICTS` (the default on Linux) or on iOS. It scans:

1. each folder in the `DICPATH` environment variable (Linux), then
2. `DICT_SYSTEM_DIR` = `/usr/share/hunspell` (or `/usr/share/myspell`),
   `HYPH_SYSTEM_DIR` = `/usr/share/hyphen`, `THES_SYSTEM_DIR` = `/usr/share/mythes`
   (`--with-external-{dict,hyph,thes}-dir` override these).

The file name gives the language: `<ll_CC>.dic` for spelling, `hyph_<ll_CC>.dic`
for hyphenation, `th_<ll_CC>_v2.dat` for a thesaurus. **Each file covers exactly
one locale**, and the first file found for a locale wins. New-style entries
win over old-style ones for the same locale.

### 3.3 Minimal ways to register a dictionary in a folder

| Approach | What's needed | Good for |
|---|---|---|
| Config node | One `.xcu` node per dictionary (above), either in a `.xcd` that ships with the app (`$BRAND_BASE_DIR` form) or written to the user layer | Data bundled with the app; several locales per dictionary |
| Folder scan (Linux) | Drop `de_DE.aff/.dic`, `hyph_de_DE.dic`, `th_de_DE_v2.dat/.idx` into a folder named in `DICPATH` or a system folder | Distro packages (`hunspell-de-de`, `hyphen-de`, `mythes-de`) |
| **Dainvo pack scan (proposed)** | A folder with `pack.json` plus the data files under `user/languages` or `share/languages` | Downloaded packs on every platform |

### 3.4 Runtime reload

`hyphen.component`, `lnth.component` and `spell.component` are all
`single-instance="true"`, and each service caches its data the first time
`getLocales()` runs (`m_DictItems` in `sspellimp.cxx:114`, and `aDicts` /
`mvThesInfo` in the others). `LngSvcMgr::modified()` (`lngsvcmgr.cxx:440`) is the
reload path the Extension Manager uses. It drops the service caches, and on
idle `UpdateAll()` rewrites `SpellCheckerList` / `HyphenatorList` /
`ThesaurusList` for newly available locales. It then broadcasts
`SPELL_*_AGAIN | HYPHENATE_AGAIN`, so Writer and Calc check spelling again.
The single-instance services never re-read their files, though. That's why
upstream LibreOffice often needs a restart after installing a dictionary. The
fix is in section 4.4.

## 4. Proposed design: Dainvo language packs

### 4.1 Pack layout on disk

```
<root>/                                   roots, highest priority first:
  de-DE/                                  1. <UserInstallation>/user/languages      (downloads, per user)
    spell-2017.01.12/                     2. $BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/languages
      pack.json                              (bundled: Contents/Resources/languages on macOS,
      de_DE_frami.aff                         share/languages on Windows/Linux)
      de_DE_frami.dic                     3. Linux only: DICPATH and /usr/share/{hunspell,hyphen,mythes}
      licenses/README_de_DE_frami.txt        (the existing SYSTEM_DICTS scan, unchanged)
      licenses/COPYING_GPLv2 ...
    hyph-2016.05.13/
      pack.json, hyph_de_DE.dic, licenses/...
    thes-2017.01.12/
      pack.json, th_de_DE_v2.dat, th_de_DE_v2.idx, licenses/...
  .staging/                               temporary downloads, cleared on start
```

`pack.json` is the metadata. It's the same information as an extension's
`dictionaries.xcu` node, but with file names relative to the folder:

```json
{
  "schema": 1,
  "id": "de-DE.hyph",
  "kind": "hyph",
  "version": "2016.05.13",
  "dictionaries": [
    { "format": "DICT_HYPH", "files": ["hyph_de_DE.dic"],
      "locales": ["de-DE", "de-BE", "de-LU"] }
  ],
  "license": "LGPL-2.1-or-later OR MPL-1.1",
  "source": "https://git.libreoffice.org/dictionaries/+/<commit>/de/"
}
```

Each component's folder name includes its version. If one kind has more than
one version, the scanner uses the highest, and startup cleanup deletes the
others. This matters because **MyThes keeps its `.dat` open** for lookups, and
Windows can't replace an open file. An update always goes into a new folder,
and the old folder is removed at the next start.

### 4.2 Discovery: new code in `lingucomponent/source/lingutil`

Add `std::vector<SvtLinguConfigDictionaryEntry> GetDainvoPackDics(const char* pDicType)`
to `lingutil.{hxx,cxx}`:

- Resolve the two roots. Use `rtl::Bootstrap::expandMacros` on
  `${$BRAND_BASE_DIR/` LIBO_ETC_FOLDER `/` SAL_CONFIGFILE("bootstrap") `:UserInstallation}/user/languages`
  (the same pattern as `scripting/source/provider/URIHelper.cxx:44`), and on
  `$BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/languages`.
- For each `<lang>/<kind>-<version>/pack.json` whose `kind` matches
  (`DICT`→`spell`, `HYPH`→`hyph`, `THES`→`thes`), read the entries with
  `boost::property_tree::json_parser`. This is header-only and already used in
  the tree. Build `aLocations` from absolute file URLs and keep **all** locales.
- Ignore broken packs (missing files, invalid BCP 47 tags) with a `SAL_WARN`.
  A bad pack must never stop the app from starting.
- A user pack overrides a bundled pack for the same locale.

Call it in the three services, between the config entries and the system scan.
Add a merge helper that checks **every** locale of an entry. The existing
`MergeNewStyleDicsAndOldStyleDics` only looks at `aLocaleNames[0]`:

```cpp
std::vector<SvtLinguConfigDictionaryEntry> aDics(aLinguCfg.GetActiveDictionariesByFormat(u"DICT_HYPH"));
MergeDicsByLocale(aDics, GetDainvoPackDics("HYPH"));   // new
MergeDicsByLocale(aDics, GetOldStyleDics("HYPH"));     // Linux system dirs, unchanged
```

**Why scan folders instead of writing config nodes?** The folder on disk is
the single source of truth. There are no stale `registrymodifications.xcu`
entries after a user deletes a folder or moves a profile, and no config writes
during startup. It's also the same code path for bundled packs, downloaded
packs and (later) admin-deployed packs. Writing config nodes would work too
(section 3.3), but keeping them in step with the files is the hard part.

### 4.3 Bundled English

- Commit `dainvo/languages/en-US/{spell,hyph}/pack.json` and
  `dainvo/languages/en-GB/hyph/pack.json`.
- New `lingucomponent/Package_dainvo_languages.mk` (or put it in `extras`).
  It copies `dictionaries/en/en_US.aff`, `en_US.dic`, `hyph_en_US.dic`,
  `hyph_en_GB.dic`, their `README_*.txt` and the `pack.json` files to
  `$(LIBO_SHARE_FOLDER)/languages/...`. Add it to
  `lingucomponent/Module_lingucomponent.mk`, and add one line to `Repository.mk`
  (`gb_Helper_register_packages_for_install,ooo,...`). The submodule doesn't
  change. It only needs to be checked out, as it is now.
- On macOS, everything inside `Contents/Resources` is covered by the code
  signature. Bundled packs are read-only, and nothing writes there.

### 4.4 Reload without restarting

- `SpellChecker` (Hunspell), `Hyphenator` and `Thesaurus` also implement
  `css::util::XRefreshable`. `refresh()` takes `GetLinguMutex()` (every other
  method already does), frees the Hunspell, `HyphenDict` and MyThes objects,
  and clears `m_DictItems` / `aDicts` / `mvThesInfo` and the supported-locale
  sequences. The next `getLocales()` scans again. `addRefreshListener` can be
  a no-op. About 40 lines per service.
- `LngSvcMgr::modified()` refreshes those three services before it resets the
  caches and starts `aUpdateIdle`. It looks them up by implementation name
  from the `pAvail*Svcs` lists, or with `createInstance`, since they are
  singletons.
- When an install or removal finishes, the Languages dialog calls
  `LinguMgr::GetLngSvcMgr()` → `XModifyListener::modified()`. That interface
  is already public, so no new UNO API is needed.
- What the user sees: German squiggles appear within a second of the
  download finishing, and Tools > Thesaurus works straight away. The one
  exception is **removing** a thesaurus that's in use on Windows. Its folder
  is deleted at the next start, and the dialog says "Removed. The files are
  deleted when Dainvo Office restarts."

### 4.5 Download index and pack format

Host it in its **own public repo, `Intagri-Technologies/dainvo-office-languages`**
(decided 2026-11-12). It's set up like the app's releases repo:

- Each publish is a normal GitHub release, tagged by date and sequence
  (`2026.11.1`). It carries `index.json`, `index.json.sig`, and the pack zips
  that are **new or changed** in that publish. Older zips stay in the release
  where they first appeared and are never replaced, so every URL is
  permanent and old indexes still work.
- The app always reads
  `https://github.com/Intagri-Technologies/dainvo-office-languages/releases/latest/download/index.json`
  (and `.sig`), which is the same pattern as the app's `appcast.xml`.
- Why not a release inside `dainvo-office-releases`: the app's update feed
  is that repo's `releases/latest/download/appcast.xml`
  (`dainvo/tools/release.py:39`). A language release there that accidentally
  became "latest" would break app updates. A separate repo removes that risk
  and keeps the app's release list clean.
- Each component in `index.json` has its own full `url`, so packs can move
  to a CDN (R2, S3, Bunny) later by publishing a new index. No app update is
  needed. GitHub release assets are free for a public repo and have a 2 GB
  per-file limit, far above the largest pack (about 9 MB).

`index.json`:

```json
{
  "schema": 1,
  "sequence": 7,
  "generated": "2026-11-12T10:00:00Z",
  "min_app_build": 100,
  "source": { "repo": "https://git.libreoffice.org/dictionaries", "commit": "c0865a8" },
  "languages": [
    { "tag": "de-DE", "name": "German (Germany)", "autonym": "Deutsch (Deutschland)",
      "components": ["de-DE.spell", "de-DE.hyph", "de-DE.thes"] },
    { "tag": "de-AT", "name": "German (Austria)", "autonym": "Deutsch (Österreich)",
      "components": ["de-AT.spell", "de-AT.hyph", "de-DE.thes"] }
  ],
  "components": {
    "de-DE.hyph": {
      "kind": "hyph", "version": "2016.05.13",
      "url": "https://github.com/Intagri-Technologies/dainvo-office-languages/releases/download/2026.11.1/de-DE.hyph-2016.05.13.zip",
      "size": 297172, "installed_size": 1096000,
      "sha256": "…",
      "locales": ["de-DE", "de-BE", "de-LU"],
      "license": "LGPL-2.1-or-later OR MPL-1.1",
      "platforms": ["mac", "win", "linux"]
    }
  }
}
```

- **Signature**: `index.json.sig` holds an Ed25519 signature (base64) of the
  exact bytes of `index.json`, made with a **separate language-pack key**
  (decided 2026-11-12). Signing must work from any OS and from CI, so it
  doesn't use Sparkle's macOS-only `generate_keys`/`sign_update` or the macOS
  keychain. Instead, `langpacks.py keygen` makes the key with Python's
  `cryptography` package (`Ed25519PrivateKey`). The private key is kept as a
  base64 file in the password manager, and later as a GitHub Actions secret
  (`DAINVO_LANGUAGES_KEY`). `langpacks.py sign` reads it from that
  environment variable or from `--key <file>`. The same commands work on
  macOS, Windows and Linux. The public key (base64, 32 bytes) goes into
  every platform's build through a new `--with-dainvo-languages-key`
  configure option in `distro-configs/DainvoVersion.conf`, which the macOS,
  Windows and Linux configs all include, next to `--with-dainvo-update-key`.
  Pack zips are trusted through the `sha256` values in the signed index.
  - Why a separate key: the app key (`dainvo-office`) can replace the whole
    application, so it should stay on one machine. The language index will
    be published more often, and later probably from GitHub Actions. With its
    own key, CI never holds the app key. A leaked language key could at worst
    offer bad dictionary files, which are data only and still pass the zip
    and file checks. It can't touch the app. The cost is one more key to back
    up and one configure option.
- **Rollback protection**: the client stores the highest `sequence` it has
  accepted, and refuses an index with a lower one.
- **Verification in C++**: the same code on all three platforms. NSS is
  built by default on macOS, Windows and Linux (`enable_nss=yes` in
  `configure.ac`; it's only turned off for static and WebAssembly builds) and
  is already used for document signing. The bundled NSS 3.133 supports Ed25519
  (`CKM_EDDSA`). Import the key as an SPKI (`SECKEY_DecodeDERSubjectPublicKeyInfo`,
  `SECKEY_ExtractPublicKey`), verify with `PK11_Verify`, and use NSS for
  SHA-256 too. Fallback: OpenSSL 3.5 (`EVP_PKEY_ED25519`, bundled), or vendor
  Monocypher's verify function (BSD-2/CC0, about 1 kLOC).
- **Zips**: plain deflate. Read them with `css::packages::zip::ZipFileAccess`
  (the `package` module). Accept only flat entry names and `licenses/`. Reject
  `..`, absolute paths, symlinks, and anything over `installed_size` + 10 %.
  Then check that every file named in `pack.json` exists.

**Granularity.** One component equals one `dictionaries.xcu` node: one spelling
list, one hyphenation pattern set or one thesaurus, with the node's locale
list. The user chooses a *language* ("German (Austria)"), and the dialog
installs the components that cover it. Spanish has 23 spelling nodes and one
hyphenation and thesaurus file shared by all `es-*` locales, so sharing
matters.

### 4.6 Build and publish tool

New `dainvo/tools/langpacks.py`, in the same style as `release.py`. It's
plain Python 3.11+ with `cryptography`, plus `gh` for publishing. Pack
contents are the same on every platform, so packs are built once, on any OS
or in CI. They are not built per platform.

| Command | Does |
|---|---|
| `build [--only de,fr]` | Reads every `dictionaries/<dir>/dictionaries.xcu` at the submodule commit. Resolves `%origin%`, including the `dictionaries/` subfolder used by `fr_FR` and `ca`. Writes one zip per node with `pack.json`, the data, and the README/COPYING/LICENSE files. Creates missing `.idx` files with the `idxdict` build tool (`workdir/LinkTarget/Executable/idxdict`) or a short Python port. Zips are deterministic (fixed timestamps, sorted names). |
| `index` | Writes `index.json`: names and autonyms from `i18nlangtag`'s list (or a small table), sizes, hashes, `sequence` + 1 |
| `keygen` | Once: makes the Ed25519 key pair with `cryptography`, writes the private key file and prints the public key for `--with-dainvo-languages-key` |
| `sign` | Signs `index.json` → `index.json.sig` with the key from `DAINVO_LANGUAGES_KEY` or `--key`, then checks it against the public key |
| `publish` | `gh release create <yyyy.mm.n> --repo Intagri-Technologies/dainvo-office-languages` with the index, signature and new or changed zips; unchanged zips keep their old URLs |
| `verify` | Downloads the published index, checks the signature and every hash |

Licence review is a step of `build`. The tool prints each component's licence
files. A hand-written `dainvo/languages/licenses.json` maps component ids to
SPDX expressions, and the build **fails** for any component without an entry.

### 4.7 The Languages dialog (UI)

New `cui/source/dialogs/dainvolanguages.cxx` and
`cui/uiconfig/ui/dainvolanguagesdialog.ui`, created through
`SfxAbstractDialogFactory` (`cui/source/factory/dlgfact.{hxx,cxx}` and
`include/sfx2/sfxdlg.hxx`).

```
┌ Languages ───────────────────────────────────────────────────────────┐
│ Search [ germ…            ]                     [Install from file…] │
│                                                                      │
│ Suggested (your system and open documents)                           │
│  ✓ English (USA)        Spelling · Hyphenation   Built in            │
│    German (Germany)     Spelling · Hyphenation · Thesaurus  9.0 MB [Get] │
│ All languages                                                        │
│    Deutsch (Österreich) …                                       [Get] │
│    Español (España)     Spelling ✓  Hyphenation ✓  Thesaurus  [Update] [Remove] │
│                                                                      │
│ ▸ Details: choose parts, licences, source                            │
│ [progress bar ……………………  Downloading German thesaurus 4.1/7.1 MB] [Cancel] │
│                                                        [Close]       │
└──────────────────────────────────────────────────────────────────────┘
```

- **Suggested**: the system UI and preferred languages
  (`MsLangId::getSystemUILanguage`, macOS `NSLocale.preferredLanguages`,
  Windows `GetUserPreferredUILanguages`, Linux `LANGUAGE`), plus the
  default document languages (`Linguistic/General/DefaultLocale`, `_CJK`,
  `_CTL`).
- **Parts**: all three by default. **On macOS, spelling is unticked** for
  locales that `MacOSXSpellChecker` already supports. Its `hasLocale` gives
  the answer, and the row says "Spelling: provided by macOS". Keeping the
  macOS speller first preserves the user's learned words and matches Pages
  and TextEdit. The user can still tick it.
- **Downloads** run on a worker thread with the bundled libcurl, set up the
  same way as `sfx2/source/appl/dainvoupdate.cxx` (`systools/curlinit.hxx`
  handles certificates on each platform). libcurl only picks up a proxy from
  environment variables, which macOS and Windows users rarely set. So the
  downloader also reads Dainvo's own proxy settings
  (`officecfg::Inet::Settings::ooInetProxyType`, `ooInetHTTPSProxyName/Port`,
  `ooInetNoProxy`) and passes them to `CURLOPT_PROXY`/`CURLOPT_NOPROXY`.
  When the type is "system", those settings come from each OS's own proxy
  settings. That way corporate proxies work the same on all three
  platforms. Progress goes to the main thread with `Application::PostUserEvent`.
  The dialog can be closed while a download runs; a status bar note finishes
  the job.
- **Install from file…** accepts a `.dainvolang` file (pack zip + detached
  `.sig`) for offline machines. It's verified with the same key. Phase 2.
- **Index cache**: `user/languages/index.json` + `.sig`, refreshed when the
  dialog opens if older than 24 h. With no network, the dialog shows the
  cached list and installed packs, and Remove still works.

**Entry points**

1. **Review tab > Proofing group**: "Languages" (`.uno:MoreDictionaries`,
   `SID_MORE_DICTIONARIES`). Remove `DainvoMoreDictionaries` from
   `Commands.xcu`. Remove `MoreDictionaries` from `DROP` in
   `dainvo/ribbon/generate.py`, and add the button in
   `dainvo/ribbon/ribbons.json` for Writer, Calc and Impress. In
   `sfx2/source/appl/appserv.cxx:1724`, open the Dainvo dialog instead of
   `AdditionsDialogHelper::RunAdditionsDialog`. Label override in
   `officecfg/.../UI/GenericCommands.xcu`: "Languages…" / tooltip "Download
   spelling, hyphenation and thesaurus data".
2. **Options > Language Settings > Writing Aids**: the "Get more dictionaries
   online…" link (`moredictslink`, `cui/source/options/optlingu.cxx:833`,
   `OnLinkClick`) becomes "Download languages…" and opens the dialog. The
   same link in the "Edit Modules" dialog (`optlingu.cxx:1565`) does too.
3. **Phase 2, prompt when data is missing**: when the user sets a text
   language that has no spelling or hyphenation, show an info bar with
   "Download German (Germany), 1.6 MB". The language is set through
   `sw/source/uibase/shells/langhelper.cxx` (Writer),
   `sc/source/ui/view/tabvwsh5.cxx` (Calc) and
   `sd/source/ui/view/ViewShellBase.cxx` (Impress), which handle
   `SID_LANGUAGE_STATUS`.
   Check with `LinguMgr::GetSpellChecker()->hasLocale`. Offer it once per
   language, and remember "Don't ask again" in `Office.Common/Dainvo`.
4. **Phase 2, first run**: if a preferred system language has no data, a
   Start Center info bar offers it.

The status bar language menu and the language list boxes already mark
languages that have spelling data (`SvxLanguageBox`). Once packs are
installed they show correctly, with no extra work.

### 4.8 Files to change (phase 1)

| Area | File(s) | Change |
|---|---|---|
| Discovery | `lingucomponent/source/lingutil/lingutil.{hxx,cxx}` | `GetDainvoPackDics`, `MergeDicsByLocale` |
| Services | `lingucomponent/source/spellcheck/spell/sspellimp.{hxx,cxx}`, `hyphenator/hyphen/hyphenimp.{hxx,cxx}`, `thesaurus/libnth/nthesimp.{hxx,cxx}` | call the pack scan; implement `XRefreshable` |
| Reload | `linguistic/source/lngsvcmgr.cxx` | `modified()` refreshes the three services |
| Bundled data | `lingucomponent/Package_dainvo_languages.mk`, `Module_lingucomponent.mk`, `Repository.mk` (1 line), `dainvo/languages/en-*/…/pack.json` | ship en-US spelling and en-US/en-GB hyphenation |
| Engine | new `cui/source/dialogs/dainvolanguages/{index,installer,verify}.cxx` | fetch the index, check the signature (NSS), download, check SHA-256, unpack, stage, swap, cleanup |
| Startup cleanup | `desktop/source/app/app.cxx` (beside the Dainvo update `Start()`) or lazily in `GetDainvoPackDics` | delete `.staging` and superseded version folders |
| Config | `officecfg/registry/schema/.../Office/Common.xcs` (`Dainvo` group) | `LanguagePacks/IndexURL`, `LastSequence`, `LastIndexCheck`, `PromptDeclined` (string list) |
| Build config | `configure.ac`, `config_host.mk.in` | `--with-dainvo-languages-index=<url>` (default: the `dainvo-office-languages` latest-release URL above) and `--with-dainvo-languages-key=<base64>` |
| UI | `cui/source/dialogs/dainvolanguages.cxx`, `cui/uiconfig/ui/dainvolanguagesdialog.ui`, `cui/source/factory/dlgfact.*`, `include/sfx2/sfxdlg.hxx`, `cui/inc/strings.hrc`, `cui/Library_cui.mk`, `cui/UIConfig_cui.mk` | dialog |
| Entry points | `sfx2/source/appl/appserv.cxx`, `cui/source/options/optlingu.cxx`, `officecfg/.../Commands.xcu`, `officecfg/.../UI/GenericCommands.xcu`, `dainvo/ribbon/generate.py`, `dainvo/ribbon/ribbons.json` | re-enable `.uno:MoreDictionaries` → Languages dialog |
| Tooling | `dainvo/tools/langpacks.py`, `dainvo/languages/licenses.json`, `dainvo/docs/RELEASES.md` (section) | build, sign, publish |
| Tests | `lingucomponent/qa/cppunit/` (pack scan: user beats bundled, broken pack ignored, multi-locale), `cui/qa/` or a new CppunitTest for the installer (zip-slip, bad signature, bad hash, old sequence, swap and rollback), one uitest that opens the dialog with a `file://` test index | |

### 4.9 macOS, Windows and Linux

The design is one code path for all three platforms. The pack format,
index, signature check, download code, folder scan, reload and dialog are
shared C++ (`lingucomponent`, `linguistic`, `cui`), and the packs are
identical everywhere. Only the rows below differ, and most of those are
paths that LibreOffice already works out per platform.

| | macOS | Windows | Linux |
|---|---|---|---|
| Spelling without packs | macOS speller (`MacOSXSpellChecker`) for the languages macOS has, plus bundled US English | Bundled US English only (LibreOffice has no Windows speller integration) | Bundled US English, plus any distro dictionaries in `/usr/share/hunspell` (`SYSTEM_DICTS`, on by default on Linux) |
| Hyphenation and thesaurus without packs | Bundled US/UK English hyphenation only | Same | Same, plus `/usr/share/hyphen` and `/usr/share/mythes` from distro packages |
| What packs add | Hyphenation and thesaurus for every language; spelling only where macOS has none (decision 1) | Everything | Everything; packs take priority over system files |
| Per-user pack folder (`<UserInstallation>/user/languages`) | `~/Library/Application Support/Dainvo Office/4/user/languages` (checked in this build's `bootstraprc`) | `%APPDATA%\Dainvo Office\4\user\languages` (expected from the `scp2` template; check on the first Windows build) | `~/.config/dainvo office/4/user/languages` (expected from the template, lower case; check on the first Linux build). Flatpak: inside `~/.var/app/<id>/config/` |
| Bundled packs | `Dainvo Office.app/Contents/Resources/languages` (inside the signed bundle, read-only) | `<install dir>\share\languages` (in the MSI) | `<install dir>/share/languages` (in the deb/rpm/tarball) |
| How bundled files get into the installer | `gb_Helper_register_packages_for_install` adds them to the DMG | Same line adds them to the MSI file list | Same line adds them to deb/rpm/archive |
| Machine-wide folder for IT (phase 2) | `/Library/Application Support/Dainvo Office/languages` | `%ProgramData%\Dainvo Office\languages` | `/usr/share/dainvo-office/languages` |
| File locking | Files can be replaced while open | **An open file can't be deleted or replaced.** That's why updates go to a new version folder and old folders are deleted at the next start (section 4.1) | Like macOS |
| Long and non-ASCII paths | Fine | Already handled: Hunspell, Hyphen and MyThes paths get the `\\?\` UTF-8 prefix (`Win_AddLongPathPrefix` in `lingutil.cxx`), so profiles with accented user names work | Fine |
| Preferred languages for "Suggested" | `NSLocale.preferredLanguages` | `GetUserPreferredUILanguages` | `LANGUAGE`, then `LC_ALL`/`LC_MESSAGES`/`LANG` |
| Proxy | System settings through Dainvo's proxy options (section 4.7) | Same, through Windows' settings | Same, plus `http(s)_proxy` variables |
| Code signing | Packs are outside the bundle, so the app's signature and notarisation aren't affected | Packs are data, so no Authenticode signing is needed. SmartScreen doesn't check them | Nothing extra |
| Possible later | — | A `WinSpellChecker` component using the Windows Spell Checking API (`ISpellCheckerFactory`, Windows 8+), modelled on `macosxspell`. About 1 to 2 weeks; not needed for phase 1 | `.deb`/`.rpm` can list `Recommends: hunspell-en-us, hyphen-en-us, mythes-en-us` |

Linux build note: only `distro-configs/DainvoMacOS.conf` exists today. The
Windows and Linux configs must leave out `--without-system-dicts` (Linux)
and pass the same `--with-dainvo-languages-index/-key` through
`DainvoVersion.conf`.

**Testing on every platform** (part of the effort in section 9). Run the
cppunit tests in each platform's build. On each OS, do a manual pass:

1. Install German.
2. Check that squiggles, hyphenation and Tools > Thesaurus work without a
   restart.
3. Update the German thesaurus while it's in use (the Windows file-lock
   case).
4. Remove German, restart, and check that the folder is gone.
5. Try once behind a proxy and once offline.
6. Try once with a profile path that has a non-ASCII user name (Windows).

## 5. Why not re-enable a dictionary-only extension path

| | Dainvo packs | Re-enable extensions (dictionaries only) |
|---|---|---|
| Build | Unchanged; `--disable-extensions` stays | Brings back `desktop/deployment` (deployment, deploymentmisc, deploymentgui), unopkg, the registry backends and bundled-extension handling, about 3 to 5 MB |
| Security | Only data; signed index; no code | `.oxt` can carry Basic and native UNO components. Limiting it to "dictionary only" means patching the `dp_registry` backends and keeping that patch through upstream merges |
| Compatibility | Uses the same files as the LibreOffice dictionaries | Many popular `.oxt` dictionaries include Python (Lightproof in `en`, `ru`, `hu`, `pt_BR`; `DictionarySwitcher.py` in `fr_FR`; Grammalecte). They'd fail or give errors without Python |
| Source | Curated by Dainvo, licences checked, versions pinned | extensions.libreoffice.org: third-party, LibreOffice-branded, uneven quality and licensing |
| UX | One Dainvo dialog, sizes shown, no restart | Extension Manager + website, restart prompts, slow first start while the registry is rebuilt |
| Work | About 3 weeks | About 1 week to re-enable, plus permanent upkeep of a feature set that was removed on purpose |

The only real advantage of extensions is that third-party `.oxt` files work
with no new code. "Install from file…" with `.dainvolang` covers offline
needs, and `langpacks.py` turns any `.oxt` into a pack in seconds (unzip,
read `dictionaries.xcu`, re-zip). **Recommendation: keep extensions off.**

## 6. Grammar checking

| Option | Offline | Size | Languages | Fit |
|---|---|---|---|---|
| **None for now** (recommended) | — | 0 | — | Dainvo's own grammar and AI tools come later |
| Built-in LanguageTool client (already compiled: `lingucomponent/source/spellcheck/languagetool`, registered for about 50 locales) talking to languagetool.org | No | 0 | about 30 | Sends document text to a third party. The free API has limits (roughly 20 requests/min and about 20 KB per request; check current terms). Keep it hidden by default |
| Same client pointed at a **self-hosted** LanguageTool server (Docker) or LanguageTool Premium | Only on the LAN | 0 in the app | about 30 | Reasonable as an "Advanced" option for organisations. Would mean unhiding `LanguageSettings/LanguageTool` in `OptionsDialog.xcu` |
| Local LanguageTool bundled | Yes | Java 17 runtime (about 50 to 80 MB with jlink) + LanguageTool (about 200+ MB, n-grams are GBs) | about 30 | Clashes with `--without-java` and the slim goals. Not recommended |
| Lightproof / Grammalecte | Yes | small | en, ru, hu, pt-BR / fr | Python. Removed with `--enable-python=no` |
| Harper (Automattic, Rust, Apache-2.0) | Yes | a few MB | English only | A possible native stopgap behind a C ABI. Check the project's current status first |

**Plan for Dainvo's own tool.** Build it as an in-tree C++ UNO component that
implements `css::linguistic2::XProofreader` (as `languagetoolimp.cxx` does),
registered in `Linguistic/ServiceManager/GrammarCheckers` with its locales.
`GrammarCheckingIterator` already runs it per paragraph on a background
thread. It feeds the blue squiggles, context-menu suggestions, the
Spelling dialog's grammar part and autocorrect-style fixes, so the AI side
only needs to return `SingleProofreadingError`s. Rule data or local models
can be shipped as a new pack `kind` (`"grammar"`) in the same index; the
`schema`/`kind` fields leave room for that. There are no grammar packs in
phase 1.

## 7. Size of bundling English by default

Measured from `dictionaries/` at submodule commit `c0865a8`:

| File | On disk | Deflate (zip/MSI) | xz (DMG-like) |
|---|---|---|---|
| `hyph_en_US.dic` (US hyphenation, BSD-style licence) | **106 KB** | **43 KB** | 38 KB |
| `hyph_en_GB.dic` | 107 KB | 48 KB | — |
| `en_US.aff` + `en_US.dic` (SCOWL, permissive) | 555 KB | 192 KB | — |
| `en_GB.aff` + `en_GB.dic` | 1.23 MB | 433 KB | — |
| `th_en_US_v2.dat` (+ `.idx`, WordNet licence) | 18.6 MB (+ 3.0 MB `.idx`) | 4.0 MB (+ `.idx`) | — |

- **US English hyphenation alone: about 106 KB installed, about 40 KB in the
  download.** That's negligible next to a DMG of a few hundred MB, so bundle
  it.
- Recommended bundle: US spelling + US and UK hyphenation (about 0.8 MB
  installed, about 0.3 MB download). Windows has no system speller, so this
  makes spelling and hyphenation work out of the box in English everywhere.
- Don't bundle the thesaurus (21.6 MB with its index). It's the "English thesaurus" pack
  (4 MB download).

For scale, all of `dictionaries/` comes to about 475 MB on disk and about
67 MB with xz: 97 spelling, 47 hyphenation and 31 thesaurus entries in 66
folders. The biggest packs are German (8 MB xz, mostly the 32 MB thesaurus),
Turkish spelling (5 MB) and Spanish (5 MB). Most languages are under 1.5 MB.

## 8. Data sources and licences

- **LibreOffice `dictionaries` repo** (`git.libreoffice.org/dictionaries`,
  already the `dictionaries` submodule here): the recommended single source.
  Pin a commit in `index.json`. Licences vary by language and file. Common
  ones are GPL/LGPL/MPL tri-licences, plain GPL (for example `bg_BG`,
  `gu_IN`, `he_IL`, `hi_IN`, `ko_KR`), LGPL, BSD or CC-BY (`nl_NL`), MIT
  (`lij_IT`), CC-BY-SA (`bo`, `ckb`), the WordNet licence (English thesaurus)
  and LPPL-derived TeX patterns. These are **separate data files**, not linked
  into Dainvo's MPL code, so shipping them as downloads alongside the app is
  aggregation. Each pack carries its original README/COPYING files in
  `licenses/`, and the dialog shows the SPDX expression and a link to the
  source folder. `licenses.json` (section 4.6) makes sure nothing ships
  unreviewed.
- **Upstream projects**, if a newer version is wanted: Hunspell dictionaries
  (SCOWL/wordlist.sourceforge.net for English, igerman98 for German,
  Grammalecte's dictionaries for French, RLA-ES for Spanish, and so on), TeX
  `hyph-utf8` patterns (converted with Hyphen's `substrings.pl`), and
  OpenThesaurus or WordNet for MyThes. The engines themselves are bundled
  externals: Hunspell 1.7.6 (MPL/GPL/LGPL), Hyphen 2.8.10 (MPL/GPL/LGPL) and
  MyThes 1.2.7 (BSD-style).
- **System spellers**: macOS `NSSpellChecker` is already used
  (`MacOSXSpellChecker`), and macOS adds languages itself. Windows' Spell
  Checking API isn't integrated (section 4.9). Linux distro packages work
  through `SYSTEM_DICTS`. None of these provide hyphenation or a thesaurus
  that LibreOffice can use.

## 9. Effort (phase 1)

| Work | Estimate |
|---|---|
| Pack scan + merge + `XRefreshable` in three services + `LngSvcMgr` reload | 2 to 3 days |
| Bundled English package | 0.5 day |
| `langpacks.py` (build, index, sign, publish, verify, licence table) + first upload | 2 to 3 days |
| Install engine (index fetch, Ed25519/NSS, SHA-256, zip checks, staging/swap/cleanup) | 3 to 4 days |
| Languages dialog, entry points, ribbon, strings | 3 to 4 days |
| Tests (cppunit + one uitest) and testing on three platforms | 2 to 3 days |
| **Total** | **about 13 to 17 working days** |

Phase 2 (about 1 to 1.5 weeks): prompts when data is missing and on first
run, `.dainvolang` offline install, a machine-wide root with an admin lock,
and optionally a Windows system speller.

## 10. Decisions (2026-11-12)

1. **macOS speller first.** A downloaded spelling pack comes second to the
   macOS speller for languages macOS supports. The dialog leaves spelling
   unticked for those languages (section 4.7).
2. **Separate signing key** (`dainvo-languages`), so the app update key
   never has to leave its machine (section 4.5).
3. **Own repo for packs**: `Intagri-Technologies/dainvo-office-languages`,
   using GitHub releases. A CDN can come later through the per-component
   `url` field without an app update (section 4.5). The repo doesn't exist
   yet. Create it when phase 1 is built.
