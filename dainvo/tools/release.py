#!/usr/bin/env python3
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Dainvo Office release tool. See dainvo/docs/RELEASES.md.

  release.py prepare <version> <build>   set the version for the next build
  release.py package [--add FILE ...]    collect, check and sign installers,
                                         write the signed appcast.xml
  release.py publish [--prerelease]      create a draft GitHub release
  release.py verify [DIR]                re-check signatures in a dist folder
  release.py status                      show configured and published versions
"""

import argparse
import datetime
import email.utils
import glob
import html
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VERSION_CONF = os.path.join(ROOT, "distro-configs", "DainvoVersion.conf")
DIST = os.path.join(ROOT, "dainvo", "dist")
REPO = "Intagri-Technologies/dainvo-office-releases"
FEED = f"https://github.com/{REPO}/releases/latest/download/appcast.xml"
KEY_ACCOUNT = "dainvo-office"
PRODUCT = "Dainvo Office"
FILE_PREFIX = "Dainvo-Office"
SPARKLE_NS = "http://www.andymatuschak.org/xml-namespaces/sparkle"
MACOS_MINIMUM = "11.0"  # MACOSX_DEPLOYMENT_TARGET

ET.register_namespace("sparkle", SPARKLE_NS)
ET.register_namespace("dc", "http://purl.org/dc/elements/1.1/")


def sp(name):
    return f"{{{SPARKLE_NS}}}{name}"


def fail(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, text=True, capture_output=True, **kwargs).stdout


# --- version config ---------------------------------------------------------

def read_version():
    text = open(VERSION_CONF, encoding="utf-8").read()
    version = re.search(r"^--with-dainvo-version=(\S+)$", text, re.M)
    build = re.search(r"^--with-dainvo-build=(\d+)$", text, re.M)
    if not version or not build:
        fail(f"{VERSION_CONF} has no --with-dainvo-version / --with-dainvo-build")
    return version.group(1), int(build.group(1))


def write_version(version, build):
    text = open(VERSION_CONF, encoding="utf-8").read()
    text = re.sub(r"^--with-dainvo-version=\S+$", f"--with-dainvo-version={version}", text, flags=re.M)
    text = re.sub(r"^--with-dainvo-build=\d+$", f"--with-dainvo-build={build}", text, flags=re.M)
    open(VERSION_CONF, "w", encoding="utf-8").write(text)


def configured_version():
    """Version and build the current build tree was configured with."""
    path = os.path.join(ROOT, "config_host.mk")
    if not os.path.exists(path):
        return None, None
    text = open(path, encoding="utf-8").read()
    version = re.search(r"^export DAINVO_VERSION=(\S*)$", text, re.M)
    build = re.search(r"^export DAINVO_BUILD=(\S*)$", text, re.M)
    return (version.group(1) if version else None,
            int(build.group(1)) if build and build.group(1) else None)


# --- GitHub -------------------------------------------------------------------

def gh_json(args):
    return json.loads(run(["gh"] + args))


def published_releases():
    """All releases, drafts included, as (tag, draft, prerelease)."""
    data = gh_json(["api", f"repos/{REPO}/releases", "--paginate"])
    return [(r["tag_name"], r["draft"], r["prerelease"]) for r in data]


def download_appcast(url):
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            return response.read()
    except Exception:
        return None


def appcast_items(data):
    if not data:
        return []
    try:
        root = ET.fromstring(data)
    except ET.ParseError:
        return []
    channel = root.find("channel")
    return [] if channel is None else channel.findall("item")


def item_build(item):
    node = item.find(sp("version"))
    if node is not None and node.text and node.text.strip().isdigit():
        return int(node.text.strip())
    enclosure = item.find("enclosure")
    value = enclosure.get(sp("version")) if enclosure is not None else None
    return int(value) if value and value.isdigit() else 0


def item_os(item):
    enclosure = item.find("enclosure")
    if enclosure is None:
        return "any"
    return enclosure.get(sp("os"), "macos")


def highest_published_build():
    """Highest build in any release's appcast, drafts and prereleases included,
    so a build number is never reused."""
    best = 0
    for tag, _draft, _pre in published_releases():
        assets = gh_json(["release", "view", tag, "--repo", REPO, "--json", "assets"])["assets"]
        if not any(a["name"] == "appcast.xml" for a in assets):
            continue
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(["gh", "release", "download", tag, "--repo", REPO, "--pattern",
                            "appcast.xml", "--dir", tmp], check=True, capture_output=True)
            data = open(os.path.join(tmp, "appcast.xml"), "rb").read()
        for item in appcast_items(data):
            best = max(best, item_build(item))
    return best


# --- Sparkle tools ------------------------------------------------------------

def sparkle_bin(tool):
    unpacked = os.path.join(ROOT, "workdir", "UnpackedTarball", "sparkle", "bin", tool)
    if os.path.exists(unpacked):
        return unpacked
    tarballs = glob.glob(os.path.join(ROOT, "external", "tarballs", "Sparkle-*.tar.xz"))
    if not tarballs:
        fail("Sparkle tools not found; run make (or make fetch) first")
    cache = os.path.join(DIST, ".sparkle")
    if not os.path.exists(os.path.join(cache, "bin", tool)):
        os.makedirs(cache, exist_ok=True)
        with tarfile.open(sorted(tarballs)[-1]) as tar:
            members = [m for m in tar.getmembers() if m.name.lstrip("./").startswith("bin/")]
            tar.extractall(cache, members=members, filter="tar")
    return os.path.join(cache, "bin", tool)


def ed_sign(path):
    """Returns the 'sparkle:edSignature="..." length="..."' values for a file."""
    out = run([sparkle_bin("sign_update"), "--account", KEY_ACCOUNT, path])
    signature = re.search(r'sparkle:edSignature="([^"]+)"', out)
    length = re.search(r'length="(\d+)"', out)
    if not signature or not length:
        fail(f"sign_update gave no signature for {path}: {out}")
    return signature.group(1), int(length.group(1))


def ed_verify(path, signature):
    result = subprocess.run([sparkle_bin("sign_update"), "--account", KEY_ACCOUNT, "--verify",
                             path, signature], capture_output=True, text=True)
    return result.returncode == 0


# --- installers -----------------------------------------------------------------

ARTIFACT_RE = re.compile(
    rf"^{FILE_PREFIX}-(?P<version>\d+\.\d+\.\d+)-(?P<os>mac|win|linux)-(?P<arch>[a-z0-9_]+)"
    r"(?:-(?P<content>deb|rpm|archive))?\.(?P<ext>dmg|msi|tar\.gz|zip)$")


def artifact_os(name):
    match = ARTIFACT_RE.match(name)
    if not match:
        return None
    os_name = {"mac": "macos", "win": "windows", "linux": "linux"}[match.group("os")]
    if os_name == "windows":
        os_name += "-" + match.group("arch")  # windows-x64, windows-arm64
    return os_name


def check_dmg(path, version, build):
    """Mount the DMG and check the app's Info.plist versions."""
    mount = tempfile.mkdtemp(prefix="dainvo-dmg-")
    subprocess.run(["hdiutil", "attach", "-nobrowse", "-readonly", "-mountpoint", mount, path],
                   check=True, capture_output=True)
    try:
        apps = [a for a in os.listdir(mount) if a.endswith(".app")]
        if len(apps) != 1:
            fail(f"{path}: expected one app, found {apps}")
        app = os.path.join(mount, apps[0])
        with open(os.path.join(app, "Contents", "Info.plist"), "rb") as f:
            info = plistlib.load(f)
        if info.get("CFBundleShortVersionString") != version or info.get("CFBundleVersion") != str(build):
            fail(f"{path}: Info.plist has {info.get('CFBundleShortVersionString')} "
                 f"({info.get('CFBundleVersion')}), expected {version} ({build}). Rebuild.")
        if info.get("SUFeedURL") != FEED or not info.get("SUPublicEDKey"):
            fail(f"{path}: Info.plist has no Sparkle feed/key; configure with --enable-dainvo-updater")
        if not os.path.isdir(os.path.join(app, "Contents", "Frameworks", "Sparkle.framework")):
            fail(f"{path}: Sparkle.framework missing from the app")
        signature = subprocess.run(["codesign", "--verify", "--deep", "--strict", app],
                                   capture_output=True, text=True)
        if signature.returncode != 0:
            print(f"warning: {apps[0]} is not validly code signed (no Developer ID yet); "
                  "Sparkle still checks the EdDSA signature", file=sys.stderr)
    finally:
        subprocess.run(["hdiutil", "detach", mount, "-quiet"], check=False)
        os.rmdir(mount)


def notes_html(notes_path, version):
    if not notes_path or not os.path.exists(notes_path):
        return f"<p>{html.escape(PRODUCT)} {html.escape(version)}</p>"
    out, in_list = [], False
    for line in open(notes_path, encoding="utf-8").read().splitlines():
        line = line.rstrip()
        bullet = re.match(r"^\s*[-*]\s+(.*)$", line)
        if bullet:
            if not in_list:
                out.append("<ul>")
                in_list = True
            out.append(f"<li>{html.escape(bullet.group(1))}</li>")
            continue
        if in_list:
            out.append("</ul>")
            in_list = False
        heading = re.match(r"^#+\s+(.*)$", line)
        if heading:
            out.append(f"<h3>{html.escape(heading.group(1))}</h3>")
        elif line:
            out.append(f"<p>{html.escape(line)}</p>")
    if in_list:
        out.append("</ul>")
    return "\n".join(out)


def make_item(version, build, name, os_name, signature, length, notes, pub_date):
    tag = f"v{version}"
    item = ET.Element("item")
    ET.SubElement(item, "title").text = f"{PRODUCT} {version}"
    ET.SubElement(item, "link").text = f"https://github.com/{REPO}/releases/tag/{tag}"
    ET.SubElement(item, sp("version")).text = str(build)
    ET.SubElement(item, sp("shortVersionString")).text = version
    ET.SubElement(item, "description").text = notes
    ET.SubElement(item, "pubDate").text = pub_date
    if os_name == "macos":
        ET.SubElement(item, sp("minimumSystemVersion")).text = MACOS_MINIMUM
        if "-arm64." in name:
            ET.SubElement(item, sp("hardwareRequirements")).text = "arm64"
    enclosure = ET.SubElement(item, "enclosure")
    enclosure.set("url", f"https://github.com/{REPO}/releases/download/{tag}/{name}")
    enclosure.set(sp("os"), os_name)
    enclosure.set("length", str(length))
    enclosure.set("type", "application/octet-stream")
    enclosure.set(sp("edSignature"), signature)
    if name.endswith(".msi"):
        enclosure.set(sp("installerArguments"), "/passive")
    return item


def write_appcast(path, items):
    rss = ET.Element("rss", {"version": "2.0"})
    channel = ET.SubElement(rss, "channel")
    ET.SubElement(channel, "title").text = f"{PRODUCT} updates"
    ET.SubElement(channel, "link").text = f"https://github.com/{REPO}/releases"
    ET.SubElement(channel, "description").text = f"Releases of {PRODUCT}"
    ET.SubElement(channel, "language").text = "en"
    for item in items:
        channel.append(item)
    ET.indent(rss)
    ET.ElementTree(rss).write(path, encoding="utf-8", xml_declaration=True)


# --- commands -------------------------------------------------------------------

def cmd_prepare(args):
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        fail("version must look like 0.2.0 (no leading v)")
    if args.build <= 0:
        fail("build must be a positive integer")
    current_version, current_build = read_version()
    highest = highest_published_build()
    if args.build <= highest:
        fail(f"build {args.build} is not higher than the highest released build {highest}")
    if args.build < current_build:
        fail(f"build {args.build} is lower than the configured build {current_build}")
    write_version(args.version, args.build)
    print(f"Dainvo Office {current_version} ({current_build}) -> {args.version} ({args.build})")
    print("Next: ./autogen.sh, then make && make instsetoo_native PKGFORMAT=dmg, "
          "then dainvo/tools/release.py package")


def cmd_status(_args):
    version, build = read_version()
    cfg_version, cfg_build = configured_version()
    print(f"DainvoVersion.conf: {version} (build {build})")
    print(f"configured tree:    {cfg_version} (build {cfg_build})")
    for tag, draft, pre in published_releases():
        print(f"release {tag}{' draft' if draft else ''}{' prerelease' if pre else ''}")
    latest = appcast_items(download_appcast(FEED))
    for item in latest:
        print(f"live appcast: {item_os(item)} build {item_build(item)}")
    if not latest:
        print("live appcast: none (no published release with appcast.xml yet)")


def cmd_package(args):
    version, build = read_version()
    cfg_version, cfg_build = configured_version()
    if (cfg_version, cfg_build) != (version, build):
        fail(f"the build tree is configured for {cfg_version} ({cfg_build}) but "
             f"DainvoVersion.conf says {version} ({build}); run ./autogen.sh and rebuild")
    out = os.path.join(DIST, version)
    os.makedirs(out, exist_ok=True)

    sources = list(args.add or [])
    dmg_dir = os.path.join(ROOT, "workdir", "installation", "DainvoOffice", "dmg", "install", "en-US")
    sources += glob.glob(os.path.join(dmg_dir, f"{FILE_PREFIX}-{version}-mac-*.dmg"))
    if not sources:
        fail(f"no installers found in {dmg_dir}; run make instsetoo_native PKGFORMAT=dmg")

    for source in sources:
        name = os.path.basename(source)
        match = ARTIFACT_RE.match(name)
        if not match:
            fail(f"{name}: not a {FILE_PREFIX}-<version>-<os>-<arch> installer name")
        if match.group("version") != version:
            fail(f"{name}: version is not {version}")
        if name.endswith(".dmg"):
            check_dmg(source, version, build)
        if os.path.abspath(source) != os.path.join(out, name):
            shutil.copy2(source, os.path.join(out, name))
        print(f"collected {name}")

    notes = notes_html(args.notes, version)
    pub_date = email.utils.format_datetime(datetime.datetime.now(datetime.timezone.utc))
    items, covered = [], set()
    for name in sorted(os.listdir(out)):
        os_name = artifact_os(name)
        if not os_name:
            continue
        signature, length = ed_sign(os.path.join(out, name))
        if not ed_verify(os.path.join(out, name), signature):
            fail(f"{name}: signature does not verify against the keychain key")
        items.append(make_item(version, build, name, os_name, signature, length, notes, pub_date))
        covered.add(os_name)
        print(f"signed {name} ({length} bytes)")

    # Keep the newest item for each platform this release doesn't ship, so a
    # macOS-only release doesn't hide the last Windows or Linux update.
    previous = {}
    for item in appcast_items(download_appcast(FEED)):
        os_name = item_os(item)
        if os_name in covered or item_build(item) >= build:
            continue
        if os_name not in previous or item_build(item) > item_build(previous[os_name]):
            previous[os_name] = item
    items += previous.values()

    appcast = os.path.join(out, "appcast.xml")
    write_appcast(appcast, items)
    # Embed an EdDSA signature of the feed itself (checked by Sparkle).
    run([sparkle_bin("sign_update"), "--account", KEY_ACCOUNT, "--disable-signing-warning", appcast])
    if args.notes:
        shutil.copy2(args.notes, os.path.join(out, "NOTES.md"))
    print(f"wrote {appcast} with {len(items)} item(s)")
    print("Next: dainvo/tools/release.py publish")


def cmd_verify(args):
    version, _build = read_version()
    out = args.dir or os.path.join(DIST, version)
    appcast = os.path.join(out, "appcast.xml")
    ok = True
    for item in appcast_items(open(appcast, "rb").read()):
        enclosure = item.find("enclosure")
        name = enclosure.get("url").rsplit("/", 1)[-1]
        path = os.path.join(out, name)
        if not os.path.exists(path):
            print(f"{name}: carried over from an earlier release, not checked")
            continue
        good = ed_verify(path, enclosure.get(sp("edSignature")))
        size = os.path.getsize(path) == int(enclosure.get("length"))
        print(f"{name}: signature {'ok' if good else 'BAD'}, length {'ok' if size else 'BAD'}")
        ok = ok and good and size
    if not ok:
        sys.exit(1)


def cmd_publish(args):
    version, build = read_version()
    out = os.path.join(DIST, version)
    tag = f"v{version}"
    if not os.path.exists(os.path.join(out, "appcast.xml")):
        fail(f"{out}/appcast.xml missing; run release.py package first")
    cmd_verify(argparse.Namespace(dir=out))
    if tag in [t for t, _d, _p in published_releases()]:
        fail(f"release {tag} already exists in {REPO}; pick a new version or delete the draft")
    files = [os.path.join(out, f) for f in sorted(os.listdir(out))
             if artifact_os(f) or f == "appcast.xml"]
    notes = os.path.join(out, "NOTES.md")
    body = open(notes, encoding="utf-8").read() if os.path.exists(notes) else f"{PRODUCT} {version}"
    body += f"\n\nBuild {build}. Based on LibreOffice."
    cmd = ["gh", "release", "create", tag, "--repo", REPO, "--draft", "--title",
           f"{PRODUCT} {tag}", "--notes", body]
    if args.prerelease:
        cmd.append("--prerelease")
    print(run(cmd + files).strip())
    print("Draft created. Users see it only after you publish it on GitHub "
          "(drafts and prereleases are skipped by releases/latest).")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("version")
    p.add_argument("build", type=int)
    p.set_defaults(func=cmd_prepare)
    p = sub.add_parser("package")
    p.add_argument("--add", action="append", help="extra installer (Windows/Linux) to include")
    p.add_argument("--notes", help="release notes (Markdown)")
    p.set_defaults(func=cmd_package)
    p = sub.add_parser("publish")
    p.add_argument("--prerelease", action="store_true", help="mark as prerelease (testing)")
    p.set_defaults(func=cmd_publish)
    p = sub.add_parser("verify")
    p.add_argument("dir", nargs="?")
    p.set_defaults(func=cmd_verify)
    p = sub.add_parser("status")
    p.set_defaults(func=cmd_status)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
