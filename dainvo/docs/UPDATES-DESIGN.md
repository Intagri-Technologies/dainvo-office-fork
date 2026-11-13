# Dainvo Office: versioning and updates (design proposal)

Status: decided 2026-11-12; CI release builds added (see RELEASES.md) — Sparkle 2 (macOS), WinSparkle (Windows), built-in notice (Linux), file names `Dainvo-Office-<version>-<os>-<arch>`. Built as described in [RELEASES.md](RELEASES.md), which is the current reference; this file keeps the reasoning.

## 1. Versioning

Same scheme as the Dainvo app (`package.json` `version` + `dainvoBuildNumber`):

| Value | Example | Rule |
|---|---|---|
| `DAINVO_VERSION` | `0.1.0` | Semantic version, chosen per release. Tag `v0.1.0`. |
| `DAINVO_BUILD` | `100` | Plain integer, always increases, never reused. A rebuilt package gets a new build number even if the version stays. |

Both live in `distro-configs/DainvoMacOS.conf` as new configure options
`--with-dainvo-version=0.1.0` and `--with-dainvo-build=100` (Windows and Linux
configs include the same lines). `LIBO_VERSION_*` stays untouched, so
extensions, user profiles (`user/` dir name) and the SDK keep working.

Where they show:

- **Help > About**: `0.1.0 (build 100)`. The About box reads
  `ooSetupVersionAboutBox`, which postprocess fills from `LIBO_VERSION_*`
  today. When `DAINVO_VERSION` is set, it gets the Dainvo version instead.
  The underlying LibreOffice version is still shown on a second line
  ("Based on LibreOffice 27.2.0.0") so bug reports stay useful.
- **Info.plist**: `CFBundleShortVersionString = 0.1.0`,
  `CFBundleVersion = 100`. Sparkle compares `CFBundleVersion`, so this is
  required for the updater.
- **Installer names**: `Dainvo-Office-0.1.0-mac-arm64.dmg`,
  `Dainvo-Office-0.1.0-win-x64.msi`, `Dainvo-Office-0.1.0-linux-x86_64.deb`
  (mirrors `Dainvo-0.19.8-mac-arm64.dmg`). The DMG volume name stays
  "Dainvo Office".
- **Windows**: MSI `ProductVersion` must be `a.b.c` with a < 256, so it uses
  `0.1.100`-style mapping internally only if needed; the display version is
  `0.1.0`. (To be checked when a Windows build host exists.)

## 2. Update channel (shared by all platforms)

One signed feed, `appcast.xml`, in Sparkle's format, attached to every
release in `Intagri-Technologies/dainvo-office-releases`. Clients fetch:

    https://github.com/Intagri-Technologies/dainvo-office-releases/releases/latest/download/appcast.xml

GitHub's `latest` ignores drafts and prereleases, so a draft release is never
offered to users until it is published. The appcast has one `<item>` per
platform (`sparkle:os="macos" | "windows" | "linux"`), with
`sparkle:version` = build number, `sparkle:shortVersionString` = semver, the
asset URL, length and an **EdDSA (ed25519) signature** of the asset. Release
notes come from the GitHub release body.

Signing key: one ed25519 key pair made with Sparkle's `generate_keys`. The
public key is compiled into the app (`SUPublicEDKey` on macOS, WinSparkle's
`EdDSAPub` resource on Windows). The private key stays in the macOS keychain
(and later a CI secret). Losing it means existing installs can no longer
update automatically, so it needs a backup.

## 3. Options per platform

### macOS

| Option | What users get | Cost / risk |
|---|---|---|
| **A. Sparkle 2** (recommended) | Standard Mac "update available" window, release notes, background download, EdDSA check, install and relaunch. Delta updates possible later. | Bundle `Sparkle.framework` (+ its XPC services) in `Contents/Frameworks`, ~3 MB, MIT. A small Obj-C hook in `vcl/osx` starts it and adds **Check for Updates…** to the app menu. Works with the current ad-hoc signing because EdDSA is checked; once Developer ID signing exists, Sparkle also checks the signature matches. |
| B. Custom checker | A notice "Dainvo Office 0.2.0 is available – Download"; user installs the DMG by hand. | Small, no third-party code, but no install/relaunch and users stay on old versions longer. |

### Windows

| Option | What users get | Cost / risk |
|---|---|---|
| **A. WinSparkle** (recommended) | Same window and flow as Sparkle; downloads the signed MSI, verifies EdDSA, runs it. Reads the same appcast. | Bundle `WinSparkle.dll` (~1 MB, MIT), call it from `vcl/win` at start-up and from Help menu. **Cannot be built or tested on this Mac**: needs a Windows build host, which the fork doesn't have yet. I'd write the code and docs, untested. |
| B. Custom checker | Notice + download link. | Same as macOS B. |

### Linux

Linux users get deb/rpm or a tarball; distro package managers own updates
for deb/rpm, and AppImage isn't produced by this build today.

| Option | What users get | Cost / risk |
|---|---|---|
| **A. In-app notice** (recommended) | Once a day, the built-in checker reads the appcast; if a higher build exists, an infobar says "Dainvo Office 0.2.0 is available" with **Download** (opens the release page) and **Not now**. | Small C++ component using the bundled curl. Also gives a "Check for Updates…" item in Help. |
| B. AppImage + zsync | Delta self-update for AppImage users. | Needs a new AppImage packaging step first; worth doing only once AppImage is the main Linux format. |

### Not recommended: LibreOffice's own updater

`extensions/source/update` has two parts: the old "check" notifier (speaks
LibreOffice's update server protocol, no signatures) and the MAR updater
(needs Mozilla's MAR tooling, a signing CA and an update server). Adapting
either is more work than Sparkle/WinSparkle and gives a worse result.

## 4. Recommended combination

**Sparkle 2 on macOS, WinSparkle on Windows, built-in notice on Linux, all
reading one EdDSA-signed `appcast.xml` from the latest GitHub release.**
The built-in checker is also a fallback that can be switched on for any
platform. Users can turn automatic checks off (Sparkle/WinSparkle store
this; Linux uses an `officecfg` setting).

## 5. Release process (script `dainvo/tools/release.sh`)

1. `release.sh prepare 0.2.0 101` – sets version and build in the distro
   config, checks the build number is higher than the last published one.
2. `make && make instsetoo_native PKGFORMAT=dmg` (macOS).
3. `release.sh package` – renames the DMG to
   `Dainvo-Office-0.2.0-mac-arm64.dmg`, signs it with the EdDSA key
   (`sign_update`), and writes/updates `appcast.xml` (merging in items for
   other platforms from the previous release, so one appcast covers all).
4. `release.sh publish` – `gh release create v0.2.0 --draft` on
   `dainvo-office-releases` with the DMG, `appcast.xml` and notes.
5. Owner checks the draft and publishes it; from then on clients see it.

Developer ID signing and notarisation slot in between 2 and 3 once a
certificate exists; until then the DMG is ad-hoc signed and first install
needs right-click > Open.

## 6. Test plan for "done"

- macOS build: About shows `0.1.0 (build 100)`; `Info.plist` has `0.1.0`/`100`;
  DMG is `Dainvo-Office-0.1.0-mac-arm64.dmg`.
- A test release `v0.1.1-test` / build 101 published as a **prerelease** in
  the releases repo, with a test feed URL override
  (`defaults write com.dainvo.office SUFeedURL …`) so real users never see it;
  Sparkle in build 100 finds it, verifies the signature and offers it.
  The test release is deleted afterwards.
