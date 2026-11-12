<p align="center">
  <img src="sysui/desktop/icons/hicolor/256x256/apps/main.png" width="128" height="128" alt="Dainvo Office icon">
</p>

<h1 align="center">Dainvo Office</h1>

Dainvo Office is a lean office suite for documents, spreadsheets and
presentations, built from [LibreOffice](https://www.libreoffice.org). It keeps
LibreOffice's engines and Microsoft Office compatibility and drops the parts
most people never use, as the base for Dainvo's AI features.

- **Documents** (Writer), **Spreadsheets** (Calc) and **Presentations**
  (Impress), with charts and equations.
- Opens and saves Word, Excel and PowerPoint files, and saves in those formats
  by default if you choose that on first launch.
- A ribbon-only interface, a modern Start Center and Phosphor icon galleries.
- Signed automatic updates (Sparkle on macOS, WinSparkle on Windows, a
  built-in notice on Linux) and downloadable language packs.

Releases: [Intagri-Technologies/dainvo-office-releases](https://github.com/Intagri-Technologies/dainvo-office-releases).

## How it differs from LibreOffice

Removed or off by default, to keep the app small and simple:

| Area | Change |
|---|---|
| Apps | No Base (databases) or Draw app. The Formula (Math) app is hidden; equations in documents still work. |
| Data | No database layer: no mail merge, data sources or database fields (mail merge will return as a Dainvo feature). |
| Scripting | No Python or Java. Basic and VBA macros are off by default (Tools > Options > Advanced > Enable macros). |
| Add-ons | No extensions; language data comes as Dainvo language packs. |
| PDF | PDF export and signing stay; opening PDFs for editing is left to a separate Dainvo PDF app. |
| Formats | Very old import formats are dropped (`--disable-legacy-import-filters`). |
| UI | No classic toolbars or interface picker, no form design tools, no built-in or online help, no LibreOffice community links. |

Every build switch is in [`distro-configs/DainvoMacOS.conf`](distro-configs/DainvoMacOS.conf)
with a comment saying why. Dainvo's own tools and notes are in [`dainvo/`](dainvo/):

| Path | What |
|---|---|
| [`dainvo/ribbon/`](dainvo/ribbon/) | Ribbon definitions (`ribbons.json`) and `generate.py`, which writes the `notebookbar*.ui` files. Edit the JSON, not the generated `.ui` files. |
| [`dainvo/icons/`](dainvo/icons/) | Ribbon and sidebar icons and their installer. |
| [`dainvo/branding/`](dainvo/branding/) | App icon artwork (`app-icon-source.png`), logos, splash screen and About images. |
| [`dainvo/fonts/`](dainvo/fonts/) | Bundled fonts (Intos, Gelasio; SIL Open Font License). |
| [`dainvo/languages/`](dainvo/languages/) | Bundled US English language data and licence records. |
| [`dainvo/docs/`](dainvo/docs/) | [Releases](dainvo/docs/RELEASES.md), [updates design](dainvo/docs/UPDATES-DESIGN.md), [language packs](dainvo/docs/LANGUAGE_TOOLS.md), [font compatibility](dainvo/docs/FONT-COMPATIBILITY.md). |
| [`dainvo/tools/`](dainvo/tools/) | Release, language-pack, font and gallery tools. |

## Building (macOS)

You need Xcode, Homebrew `pkg-config` and Python 3.11 or newer on `PATH`, plus
the usual LibreOffice build tools (see
[LibreOffice's macOS build guide](https://wiki.documentfoundation.org/Development/BuildingOnMac)).

1. Create `autogen.input` in the repository root (it is ignored by git). Start
   from [`dainvo/autogen.input`](dainvo/autogen.input) and adjust the
   machine-specific lines:

   ```
   INCLUDE:DainvoMacOS
   --with-branding=/full/path/to/this/repo/dainvo/branding
   --enable-ccache
   --with-parallelism=8
   PKG_CONFIG=/opt/homebrew/bin/pkg-config
   ```

2. Configure and build:

   ```bash
   ./autogen.sh && make
   ```

   The app is in `instdir/DainvoOffice.app`.

3. For a release disk image (stripped, with symbols kept separately for crash
   reports):

   ```bash
   make instsetoo_native PKGFORMAT=dmg
   ```

   The image is in `workdir/installation/DainvoOffice/dmg/install/en-US/`.
   Signing, notarization and publishing are covered in
   [`dainvo/docs/RELEASES.md`](dainvo/docs/RELEASES.md).

Windows and Linux builds are planned; their configuration files don't exist yet.

## Licence and credit

Dainvo Office is a fork of LibreOffice, made by The Document Foundation and
its community, and keeps LibreOffice's licence: the
[Mozilla Public License 2.0](COPYING.MPL), with some files under other
licences as stated in their headers and in the [`COPYING`](COPYING) file.

This repository's history starts with a single commit that imports
LibreOffice exactly as it was at upstream commit
`6cd7c50d0e943f89b8092958deb01340afeb1fd1`; LibreOffice's earlier history is
in [its own repository](https://git.libreoffice.org/core). LibreOffice's
original README, with its overview of the code structure, is in
[`README.libreoffice.md`](README.libreoffice.md).

LibreOffice is a registered trademark of The Document Foundation. Dainvo
Office is not affiliated with or endorsed by The Document Foundation.
