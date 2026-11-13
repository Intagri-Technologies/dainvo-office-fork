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
which every platform config (`DainvoMacOS.conf`, `DainvoWindows.conf`,
`DainvoLinux.conf`) includes. The first release is **0.1.0** (build 102:
builds 100 and 101 were used for local tests and are never reused).

Where they show up:

| Place | Value |
|---|---|
| Help > About | `0.1.0 (build 100, aarch64)` |
| `Info.plist` | `CFBundleShortVersionString` = `0.1.0`, `CFBundleVersion` = `100` (also the Quick Look extensions) |
| Installer names | `Dainvo-Office-0.1.0-mac-arm64.dmg`, `…-win-x64.msi`, `…-linux-amd64.deb`, `…-linux-x86_64.rpm`, `…-linux-x86_64.tar.gz` |
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
| Windows | WinSparkle 0.9 (`external/winsparkle`, next to `soffice.exe`) | Same flow for the MSI (`/passive`). Built by the release workflow; **not tested yet**. |
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

## Release packages

| File | For | Made by |
|---|---|---|
| `Dainvo-Office-<v>-mac-arm64.dmg` | macOS 11+ on Apple silicon | `make instsetoo_native PKGFORMAT=dmg` |
| `Dainvo-Office-<v>-win-x64.msi` | Windows 10/11, 64-bit | `make instsetoo_native` (Cygwin + Visual Studio 2022) |
| `Dainvo-Office-<v>-linux-amd64.deb` | Debian 12+, Ubuntu 22.04+ | [`dainvo/packaging/linux/package.sh`](../packaging/linux/package.sh) (nfpm) |
| `Dainvo-Office-<v>-linux-x86_64.rpm` | Fedora, openSUSE, RHEL 9+ | same |
| `Dainvo-Office-<v>-linux-x86_64.tar.gz` | other Linux; source of the AUR package | same |
| AUR `dainvo-office-bin` | Arch Linux | [`dainvo/packaging/aur`](../packaging/aur), pushed when a release goes public |
| `appcast.xml` | the updaters | `release.py package` |
| `SHA256SUMS` | people checking downloads | `release.py package` |

The Linux packages share one layout: the program in `/opt/dainvo-office`,
the launcher `/usr/bin/dainvo-office`, desktop entries for the Start Center,
Writer, Calc and Impress, and icons in `/usr/share/icons/hicolor`. They are
built in the `manylinux_2_34` container (AlmaLinux 9) and require glibc
2.34 and the GTK 3 libraries listed in the package dependencies. Spell checking on Linux uses the
distribution's Hunspell dictionaries (recommended by the packages).

## Checking release branding

Mount the disk image or extract the installer, then run
`python3 dainvo/tools/audit-package-branding.py <directory>`. It reports
upstream references in names, compiled files, and ZIP members. It exits
with status 1 if it finds references outside the designated license and
notice files. Review the results before publishing. The scan also reports
copyright headers in bundled resources and identifiers used to read older
documents.

Calc reads the older `ORG.LIBREOFFICE.*` formula names. Dainvo writes
`ORG.DAINVO.*` for those vendor-specific formulas in ODS, XLSX and XLS.
Other spreadsheet apps may not recognize the new names. Standard formulas
keep their existing names.

## Making a release (GitHub Actions)

Two workflows in `Intagri-Technologies/dainvo-office-fork`:

- **Release build** ([`.github/workflows/release.yml`](../../.github/workflows/release.yml)):
  runs on a `v*` tag (or by hand). Builds macOS, Windows and Linux in
  parallel; when all three pass, signs every installer with the EdDSA key,
  writes `appcast.xml` and creates a **draft** release in
  `dainvo-office-releases`. Drafts are invisible to users and updaters.
- **Publish release** ([`.github/workflows/publish-release.yml`](../../.github/workflows/publish-release.yml)):
  run by hand with the version and a channel:
  - `public`: the draft becomes the latest release. Every installed copy is
    offered the update (Sparkle/WinSparkle within a day, Linux notice), and
    the AUR package is updated.
  - `prerelease`: published but not "latest", so only testers who point a
    copy at its own feed see it (see "Testing an update"). Run the workflow
    again with `public` to release it to everyone.

Steps:

1. **Version and build number**, on `main`:

   ```bash
   dainvo/tools/release.py prepare 0.2.0 103
   ```

   The build must be higher than any build ever used (the script checks
   every release, drafts included).
2. **Release notes**: write `dainvo/release-notes/0.2.0.md` (user-facing,
   like the Dainvo app's changelog). Commit both files.
3. **Tag and push**:

   ```bash
   git tag v0.2.0 && git push dainvo main v0.2.0
   ```

   The tag must match `DainvoVersion.conf`, or the workflow stops.
4. **Wait for the draft** (Actions > Release build). If a platform job
   failed or timed out, use *Re-run failed jobs*: the compiler cache makes
   the second run much faster, and the draft is created once all three pass.
5. **Check the draft** on the releases page: notes, files, `appcast.xml`,
   and the source link to the exact build revision. In the installed app,
   check **Help > License Information** and the shipped `NOTICE` and
   component license files. The updater notices are under `licenses/` in the
   app's `Contents/Resources` on macOS and `share` on Windows.
6. **Publish**: Actions > Publish release > Run workflow, version `0.2.0`,
   channel `public` (or `prerelease` first).

For a manual check, set **platform** to one platform in **Release build**.
A single-platform run uploads its packages without creating a draft.

### One-time setup (repository settings of dainvo-office-fork)

Create an environment named **`release`** (Settings > Environments). Under
*Deployment branches and tags*, allow `main` and tags `v*`, so no other
branch can use the signing secrets. Add these secrets to it:

| Secret | Needed for | How to get it |
|---|---|---|
| `DAINVO_ED_PRIVATE_KEY` | **required**: update signatures | On the Mac with the key: `generate_keys --account dainvo-office -x key.txt`, paste the file's single line, then delete the file. Never commit it. |
| `RELEASES_TOKEN` | **required**: creating/publishing releases | Fine-grained personal access token (or GitHub App token) with *Contents: Read and write* on `Intagri-Technologies/dainvo-office-releases` only. |
| `MACOS_CERTIFICATE_P12` | macOS signing (optional) | Office's Developer ID Application identity, issued by the Dainvo Apple Developer team. Export it with its private key as a password-protected `.p12`, then pipe `base64 -i cert.p12` to `gh secret set MACOS_CERTIFICATE_P12 --env release --repo Intagri-Technologies/dainvo-office-fork`. Delete the `.p12` after upload. |
| `MACOS_CERTIFICATE_PASSWORD` | macOS signing | The .p12 export password. |
| `APPLE_API_KEY_BASE64`, `APPLE_API_KEY_ID`, `APPLE_API_ISSUER` | macOS notarization (optional) | A dedicated App Store Connect team key for Office, its Key ID, and the Issuer ID from the Team Keys page. Pipe `base64 -i AuthKey_<KEY_ID>.p8` to `gh secret set APPLE_API_KEY_BASE64 --env release --repo Intagri-Technologies/dainvo-office-fork`. |
| `AZURE_TENANT_ID`, `AZURE_CLIENT_ID` | Windows signing (optional) | Directory (tenant) ID and application (client) ID from the Office Microsoft Entra app registration. |
| `AZURE_TRUSTED_SIGNING_ENDPOINT`, `AZURE_TRUSTED_SIGNING_ACCOUNT_NAME`, `AZURE_TRUSTED_SIGNING_CERTIFICATE_PROFILE_NAME` | Windows signing | The regional endpoint, account name, and certificate profile name from the existing Dainvo Azure Artifact Signing profile. |
| `AUR_SSH_PRIVATE_KEY` | AUR updates (optional) | An SSH key registered on the AUR account that owns `dainvo-office-bin`. |

On the Office Microsoft Entra app registration, add a federated credential for
GitHub Actions with the repository's immutable organization and repository IDs,
the `release` environment, and the `api://AzureADTokenExchange` audience.
Assign the Office service principal the **Artifact Signing Certificate Profile
Signer** role on the shared Dainvo certificate profile. The Windows job uses
GitHub OIDC to sign in to Azure, signs the binaries in `instdir` before packaging,
then signs the MSI. If any of the five Azure settings in the table is missing,
the job warns and produces an unsigned build.

`generate_keys` is in Sparkle's `bin/` folder (after a build:
`workdir/UnpackedTarball/sparkle/bin`; or unpack
`external/tarballs/Sparkle-*.tar.xz`).

Without the optional secrets the builds still run: the DMG is then not
Developer ID signed (users must allow it in System Settings > Privacy &
Security on first launch), the MSI is unsigned (SmartScreen warning), and
the AUR package is not updated.

Optional repository variables `MACOS_RUNNER`, `WINDOWS_RUNNER` and
`LINUX_RUNNER` choose other runners (larger GitHub runners or self-hosted
ones), e.g. `windows-2022-16core`.

### Build times

Full LibreOffice builds are heavy. Expect on standard GitHub runners, for the
first build without a warm compiler cache: Linux 3-5 hours, macOS 3-5 hours,
Windows 6+ hours. Jobs stop at 6 hours (GitHub's limit for hosted runners).
macOS and Linux keep a compiler cache (ccache, 3 GB each), so a re-run or the
next release is much faster. **Windows has no compiler cache and will most
likely need a larger runner** (`WINDOWS_RUNNER`; paid, needs a GitHub Team or
Enterprise plan for the organisation) **or a self-hosted Windows machine**.

### Making a release by hand (macOS only)

The same steps work locally for a macOS-only release, with the signing key
from the login keychain:

```bash
dainvo/tools/release.py prepare 0.2.0 103
# Commit and push the version change and any other source changes before building.
./autogen.sh && make && make instsetoo_native PKGFORMAT=dmg
dainvo/tools/release.py package            # checks and signs, writes dainvo/dist/0.2.0
dainvo/tools/release.py publish            # creates the draft
dainvo/tools/release.py promote            # publishes it (add --prerelease for testers)
```

`package --add FILE` adds Windows/Linux installers built elsewhere from the
same source revision. For platforms a release doesn't ship, the newest item
from the live appcast is carried over. `release.py status` shows the configured
and published versions. `release.py verify` checks the signatures in a dist folder.

### Testing an update before users see it

1. Make a test release (e.g. `0.1.1`, a new build number) and publish its
   draft with channel **`prerelease`**. It is not in `releases/latest`.
2. Point a copy at the test feed:

   ```bash
   defaults write com.dainvo.office SUFeedURL https://github.com/Intagri-Technologies/dainvo-office-releases/releases/download/v0.1.1/appcast.xml
   ```

   (Windows and Linux: start the app with the environment variable
   `DAINVO_UPDATE_FEED_OVERRIDE=<url>` set.) Run Help > Check for Updates…
3. Afterwards: `defaults delete com.dainvo.office SUFeedURL`, and either
   publish the release as `public` or delete it and its tag
   (`gh release delete v0.1.1 --cleanup-tag --repo …`). Don't reuse its
   build number.

## Not done yet

- **Nothing has been built in CI yet.** The workflows, the Windows and Linux
  configs and the Linux packaging are written but not run; expect fixes on
  the first run, especially on Windows.
- **Windows**: no spell checking (LibreOffice ships its dictionaries as
  extensions, which this build disables). WinSparkle is untested.
- **macOS Intel**: only Apple silicon (arm64) is built.
- **Flatpak / Snap** (planned): would be built from the same Linux
  `.tar.gz` (`/opt/dainvo-office` tree) in another job of the release
  workflow, published to Flathub / the Snap Store when a release goes public.
  On Flatpak/Snap the store handles updates, so the in-app notice should be
  switched off there.
