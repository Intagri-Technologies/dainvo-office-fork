# Dainvo Office releases and updates

How Dainvo Office is versioned, how installed copies find updates, and how to
make a release. The design discussion is in [UPDATES-DESIGN.md](UPDATES-DESIGN.md).

## Versioning

Same scheme as the Dainvo app (`package.json` `version` + `dainvoBuildNumber`):

- **Version**: semantic version, e.g. `0.1.0`, tag `v0.1.0`.
- **Build number**: a whole number that only goes up, e.g. `100`. Every
  package that leaves this machine gets a new one, even a rebuild of the same
  version. Updaters compare build numbers, not versions.

Both live in [`distro-configs/DainvoVersion.conf`](../../distro-configs/DainvoVersion.conf),
which every platform config includes. The first release is **0.1.0, build 100**.

Where they show up:

| Place | Value |
|---|---|
| Help > About | `0.1.0 (build 100, aarch64)` and "Based on LibreOffice 27.2…" |
| `Info.plist` | `CFBundleShortVersionString` = `0.1.0`, `CFBundleVersion` = `100` (also the Quick Look extensions) |
| Installer names | `Dainvo-Office-0.1.0-mac-arm64.dmg`, `…-win-x64.msi`, `…-linux-amd64-deb.tar.gz` |
| Update feed | `sparkle:shortVersionString` / `sparkle:version` |

LibreOffice's own version (`AC_INIT` in `configure.ac`, `LIBO_VERSION_*`) is
not changed: extension compatibility checks, the user profile folder and the
SDK depend on it. It still appears in the About box, PDF producer field and
`ooSetupVersionAboutBox`.

## How updates work

Every published release has an `appcast.xml` (Sparkle format) next to the
installers. Installed copies read the one from the **latest published
release**:

    https://github.com/Intagri-Technologies/dainvo-office-releases/releases/latest/download/appcast.xml

GitHub's `latest` skips drafts and prereleases, so nothing reaches users until
a release is published as a normal release.

| Platform | Updater | What the user sees |
|---|---|---|
| macOS | Sparkle 2.10 (`external/sparkle`, in `Contents/Frameworks`) | Sparkle's update window with release notes; downloads the DMG, checks its EdDSA signature, replaces the app and relaunches. On the second launch Sparkle asks whether to check automatically (daily). |
| Windows | WinSparkle 0.9 (`external/winsparkle`, next to `soffice.exe`) | Same flow for the MSI (`/passive`). **Not built or tested yet**: there is no Windows build host. |
| Linux | Built-in (`sfx2/source/appl/dainvoupdate.cxx`) | Daily check; an infobar "Dainvo Office X is available" with **Download** (opens the release page) and **Skip This Version**. |

Help > **Check for Updates…** checks right away on every platform. If
Sparkle or WinSparkle is missing (e.g. a developer build run from `instdir`
without them), the built-in check is used instead.

Each appcast item has the build number, version, release notes, the
installer's URL and size, and an **EdDSA (ed25519) signature** of the
installer. The feed itself also carries an embedded EdDSA signature.

### Signing key

- Public key (compiled in, `SUPublicEDKey`): in `DainvoVersion.conf`.
- Private key: the release owner's **login keychain**, item "Private key for
  signing Sparkle updates", account `dainvo-office` (made with Sparkle's
  `generate_keys --account dainvo-office`).

**Back it up now**: `generate_keys --account dainvo-office -x dainvo-office-ed25519.key`
(Sparkle's `bin/` is in `workdir/UnpackedTarball/sparkle/bin` after a build),
and store the file in the password manager, not in git. If the key is lost,
installed copies can't verify new updates and users must reinstall by hand.
To sign on another machine or in CI, import it with `generate_keys -f <file>`.

### Settings

- macOS: Sparkle's own (`defaults read com.dainvo.office`; `SUEnableAutomaticChecks`).
- Windows: WinSparkle's, in `HKCU\Software\Dainvo\Dainvo Office\WinSparkle`.
- Linux: Tools > Options > Advanced > Expert Configuration,
  `org.openoffice.Office.Common/DainvoUpdate` (`AutoCheck`, `LastCheck`, `SkippedBuild`).

## Making a release (macOS)

Prerequisites: `gh` signed in with write access to
`Intagri-Technologies/dainvo-office-releases`, the signing key in the keychain,
build tools on `PATH` (`export PATH=/path/to/lo-buildtools:$PATH`).

1. **Pick the version and build number.** The build must be higher than any
   build ever released (the script checks every release in the repo):

   ```bash
   dainvo/tools/release.py prepare 0.2.0 101
   ```

2. **Build and package.**

   ```bash
   ./autogen.sh && make && make instsetoo_native PKGFORMAT=dmg
   ```

   The DMG is `workdir/installation/DainvoOffice/dmg/install/en-US/Dainvo-Office-0.2.0-mac-arm64.dmg`.

3. **Write release notes** in Markdown (user-facing, like the Dainvo app's
   changelog), e.g. `dainvo/dist/notes-0.2.0.md`.

4. **Collect, check and sign.**

   ```bash
   dainvo/tools/release.py package --notes dainvo/dist/notes-0.2.0.md
   ```

   This mounts the DMG and checks `Info.plist` (version, build, feed, key, and
   that Sparkle.framework is inside), signs each installer with the EdDSA key,
   and writes a signed `dainvo/dist/0.2.0/appcast.xml`. Windows/Linux
   installers built elsewhere are added with `--add <file>`. For platforms the
   release doesn't ship, the newest item from the live appcast is carried over.

5. **Create the draft release.**

   ```bash
   dainvo/tools/release.py publish
   ```

   Creates draft `v0.2.0` with the installers and `appcast.xml`.

6. **Check the draft on GitHub and publish it.** From then on installed copies
   see the update (Sparkle within a day, or at once with Check for Updates…).

`dainvo/tools/release.py status` shows the configured, built and published
versions. `dainvo/tools/release.py verify` re-checks signatures in a dist folder.

### Testing an update before users see it

1. Build and package a test version (e.g. `0.1.1`, build `101`), then
   `release.py publish --prerelease`, and publish that draft as a
   **prerelease** (still hidden from `releases/latest`).
2. Point a copy at the test feed:

   ```bash
   defaults write com.dainvo.office SUFeedURL https://github.com/Intagri-Technologies/dainvo-office-releases/releases/download/v0.1.1/appcast.xml
   ```

   (Linux: start with `DAINVO_UPDATE_FEED_OVERRIDE=<url>`.) Run Help > Check for Updates…
3. Afterwards: `defaults delete com.dainvo.office SUFeedURL`, and delete the
   test release and tag (`gh release delete v0.1.1 --cleanup-tag --repo …`).
   Don't reuse its build number.

## Not done yet

- **Developer ID signing and notarisation** (macOS). The app is not signed
  with a Developer ID yet, so first installs need right-click > Open, and
  Sparkle relies on the EdDSA signature alone. When a certificate exists:
  configure with `--enable-macosx-code-signing=<identity>` (signs the app,
  including Sparkle.framework, in `installer::simplepackage`; the Dainvo app
  name still has to be added to the signing branch there), then notarise and
  staple the DMG before `release.py package`.
- **Windows and Linux builds**: no build hosts yet. The WinSparkle code is
  written but untested; Linux installers would be added with `--add`.
- Sparkle and WinSparkle are MIT licensed; their notices should be added to
  the bundled third-party license file.
