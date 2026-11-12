#!/usr/bin/env python3
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Dainvo Office language packs tool. See dainvo/docs/LANGUAGE_TOOLS.md.

  langpacks.py build [--only de,fr] [--out DIR]   zip packs from the dictionaries submodule
  langpacks.py licenses                           print licence files to review per component
  langpacks.py index --tag TAG [--out DIR]        write index.json for the built packs
  langpacks.py sign [--out DIR]                   sign index.json -> index.json.sig
  langpacks.py verify [DIR | --remote]            check signature and checksums
  langpacks.py publish --tag TAG [--out DIR]      upload to the dainvo-office-languages repo
  langpacks.py keygen --out FILE                  make a new signing key (only once)

Works on macOS, Windows and Linux with Python 3.11+. Signing uses the
'cryptography' package if installed, otherwise the openssl command (3.0+).
The private key (base64 of the 32-byte Ed25519 seed) is read from
--key FILE, the DAINVO_LANGUAGES_KEY environment variable, or on macOS the
login keychain item "Dainvo language packs signing key" (account dainvo-languages).
"""

import argparse
import base64
import bisect
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DICTIONARIES = os.path.join(ROOT, "dictionaries")
OUT = os.path.join(ROOT, "dainvo", "dist", "languages")
LICENSES = os.path.join(ROOT, "dainvo", "languages", "licenses.json")
SCHEMA = os.path.join(ROOT, "officecfg", "registry", "schema", "org", "openoffice", "Office",
                      "Linguistic.xcs")
REPO = "Intagri-Technologies/dainvo-office-languages"
INDEX_URL = f"https://github.com/{REPO}/releases/latest/download/index.json"
KEYCHAIN_ACCOUNT = "dainvo-languages"
KEYCHAIN_SERVICE = "Dainvo language packs signing key"
OOR = "{http://openoffice.org/2001/registry}"
KINDS = {"DICT_SPELL": "spell", "DICT_HYPH": "hyph", "DICT_THES": "thes"}
ZIP_DATE = (2020, 1, 1, 0, 0, 0)
# PKCS#8 / SubjectPublicKeyInfo prefixes for raw Ed25519 keys
PKCS8_PREFIX = bytes.fromhex("302e020100300506032b657004220420")
SPKI_PREFIX = bytes.fromhex("302a300506032b6570032100")
LICENSE_FILE = re.compile(r"(readme|licen[cs]e|copying|copyright|authors|wordnet)", re.I)


def fail(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ------------------------------------------------------------------ keys

def public_key_from_schema():
    text = open(SCHEMA, encoding="utf-8").read()
    m = re.search(r'oor:name="PublicKey".*?<value>([^<]+)</value>', text, re.S)
    if not m:
        fail(f"no DainvoLanguagePacks/PublicKey in {SCHEMA}")
    return base64.b64decode(m.group(1).strip())


def load_seed(key_file):
    if key_file:
        text = open(key_file, encoding="ascii").read()
    elif os.environ.get("DAINVO_LANGUAGES_KEY"):
        text = os.environ["DAINVO_LANGUAGES_KEY"]
    elif sys.platform == "darwin":
        try:
            text = subprocess.run(["security", "find-generic-password", "-a", KEYCHAIN_ACCOUNT,
                                   "-s", KEYCHAIN_SERVICE, "-w"], check=True, text=True,
                                  capture_output=True).stdout
        except subprocess.CalledProcessError:
            fail("signing key not found in the keychain; pass --key or set DAINVO_LANGUAGES_KEY")
    else:
        fail("no signing key: pass --key FILE or set DAINVO_LANGUAGES_KEY")
    seed = base64.b64decode(text.strip())
    if len(seed) != 32:
        fail("the signing key must be base64 of a 32-byte Ed25519 seed")
    return seed


def ed25519_sign(seed, data):
    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        return Ed25519PrivateKey.from_private_bytes(seed).sign(data)
    except ImportError:
        pass
    with tempfile.TemporaryDirectory() as tmp:
        key = os.path.join(tmp, "key.der")
        msg = os.path.join(tmp, "msg")
        fd = os.open(key, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(PKCS8_PREFIX + seed)
        with open(msg, "wb") as f:
            f.write(data)
        return subprocess.run(["openssl", "pkeyutl", "-sign", "-inkey", key, "-keyform", "DER",
                               "-rawin", "-in", msg], check=True, capture_output=True).stdout


def ed25519_public(seed):
    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        from cryptography.hazmat.primitives import serialization
        return Ed25519PrivateKey.from_private_bytes(seed).public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    except ImportError:
        pass
    with tempfile.TemporaryDirectory() as tmp:
        key = os.path.join(tmp, "key.der")
        fd = os.open(key, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(PKCS8_PREFIX + seed)
        der = subprocess.run(["openssl", "pkey", "-inform", "DER", "-in", key, "-pubout",
                              "-outform", "DER"], check=True, capture_output=True).stdout
        return der[-32:]


def ed25519_verify(public, signature, data):
    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        from cryptography.exceptions import InvalidSignature
        try:
            Ed25519PublicKey.from_public_bytes(public).verify(signature, data)
            return True
        except InvalidSignature:
            return False
    except ImportError:
        pass
    with tempfile.TemporaryDirectory() as tmp:
        files = {"pub.der": SPKI_PREFIX + public, "sig": signature, "msg": data}
        for name, content in files.items():
            with open(os.path.join(tmp, name), "wb") as f:
                f.write(content)
        r = subprocess.run(["openssl", "pkeyutl", "-verify", "-pubin", "-inkey",
                            os.path.join(tmp, "pub.der"), "-keyform", "DER", "-rawin",
                            "-in", os.path.join(tmp, "msg"), "-sigfile",
                            os.path.join(tmp, "sig")], capture_output=True)
        return r.returncode == 0


# ------------------------------------------------------------------ dictionaries

def safe_version(text):
    v = re.sub(r"[^A-Za-z0-9.]+", ".", text or "").strip(".")
    return v or "0"


def folder_version(folder):
    try:
        root = ET.parse(os.path.join(folder, "description.xml")).getroot()
        for el in root.iter():
            if el.tag.endswith("version") and el.get("value"):
                return safe_version(el.get("value"))
    except (OSError, ET.ParseError):
        pass
    return "0"


def find_file(folder, name):
    for candidate in (os.path.join(folder, name), os.path.join(folder, "dictionaries", name)):
        if os.path.isfile(candidate):
            return candidate
    return None


def scan_dictionaries(only=None):
    """One component per dictionaries.xcu node."""
    comps = {}
    for d in sorted(os.listdir(DICTIONARIES)):
        folder = os.path.join(DICTIONARIES, d)
        xcu = os.path.join(folder, "dictionaries.xcu")
        if not os.path.isfile(xcu):
            continue
        version = folder_version(folder)
        for node in ET.parse(xcu).getroot().iter("node"):
            props = {p.get(OOR + "name"): (p.findtext("value") or "") for p in node.findall("prop")}
            kind = KINDS.get(props.get("Format"))
            if not kind:
                continue
            locales = props.get("Locales", "").split()
            if not locales:
                continue
            cid = f"{locales[0]}.{kind}"
            if only and not any(locales[0] == o or locales[0].startswith(o + "-") for o in only):
                continue
            files = []
            for loc in props.get("Locations", "").split():
                name = loc.replace("%origin%/", "")
                if kind == "thes" and not name.endswith(".dat"):
                    continue  # the .idx is generated next to the .dat
                path = find_file(folder, name)
                if not path:
                    fail(f"{d}: {name} not found")
                files.append(path)
            if cid in comps:
                fail(f"two dictionaries would both be {cid} ({comps[cid]['folder']} and {d})")
            comps[cid] = {"id": cid, "kind": kind, "format": props["Format"], "version": version,
                          "locales": locales, "files": files, "folder": d}
    return comps


def make_idx(dat_path):
    """Python port of l10ntools idxdict: index of a MyThes .dat file."""
    # Same rules as idxdict: a word runs up to the next '|' (even across a
    # line break), then a count line, then that many meaning lines.
    data = open(dat_path, "rb").read()
    nl = data.find(b"\n")
    encoding = data[:nl] if nl >= 0 else data
    pos = len(encoding) + 1
    entries = []
    while True:
        bar = data.find(b"|", pos)
        if bar < 0:
            break
        word = data[pos:bar]
        entries.append((word, pos))
        end = data.find(b"\n", bar + 1)
        if end < 0:
            end = len(data)
        count = int(data[bar + 1:end])
        pos = end + 1
        for _ in range(count):
            end = data.find(b"\n", pos)
            pos = (end if end >= 0 else len(data)) + 1
    # idxdict keeps entries in a std::multimap filled with hinted inserts
    # (hint = the previous insert); replay libc++'s rule so repeated words
    # come out in the same order and the .idx is byte-identical.
    keys, ordered = [], []
    hint = 0
    for word, offset in entries:
        if hint == len(keys) or not keys[hint] < word:
            if hint == 0 or not word < keys[hint - 1]:
                pos = hint
            else:
                pos = bisect.bisect_right(keys, word)
        else:
            pos = bisect.bisect_left(keys, word)
        keys.insert(pos, word)
        ordered.insert(pos, (word, offset))
        hint = pos
    entries = ordered
    out = encoding + b"\n" + str(len(entries)).encode() + b"\n"
    return out + b"".join(w + b"|" + str(o).encode() + b"\n" for w, o in entries)


def write_zip(path, members):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in sorted(members):
            info = zipfile.ZipInfo(name, date_time=ZIP_DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, members[name])


def load_licenses():
    if not os.path.isfile(LICENSES):
        return {}
    return json.load(open(LICENSES, encoding="utf-8"))


# ------------------------------------------------------------------ commands

def cmd_build(args):
    only = args.only.split(",") if args.only else None
    comps = scan_dictionaries(only)
    licenses = load_licenses()
    missing = [c for c in comps if not licenses.get(c)]
    if missing and args.skip_unreviewed:
        print(f"leaving out (no reviewed licence): {', '.join(sorted(missing))}")
        comps = {k: v for k, v in comps.items() if k not in missing}
        missing = []
    if missing and not args.allow_unreviewed:
        fail(f"{len(missing)} components have no reviewed licence in {LICENSES}, e.g. "
             f"{', '.join(missing[:5])}. Run 'langpacks.py licenses' and add them, or pass "
             "--allow-unreviewed for a local test build.")
    os.makedirs(args.out, exist_ok=True)
    built = []
    for cid, c in sorted(comps.items()):
        members = {}
        names = []
        for path in c["files"]:
            name = os.path.basename(path)
            members[name] = open(path, "rb").read()
            names.append(name)
            if c["kind"] == "thes":
                idx = path[:-4] + ".idx"
                members[name[:-4] + ".idx"] = (open(idx, "rb").read() if os.path.isfile(idx)
                                               else make_idx(path))
        folder = os.path.join(DICTIONARIES, c["folder"])
        for base in (folder, os.path.join(folder, "dictionaries")):
            if os.path.isdir(base):
                for f in sorted(os.listdir(base)):
                    if LICENSE_FILE.search(f) and os.path.isfile(os.path.join(base, f)):
                        members[f"licenses/{f}"] = open(os.path.join(base, f), "rb").read()
        pack = {"schema": 1, "id": cid, "kind": c["kind"], "version": c["version"],
                "license": licenses.get(cid, "LicenseRef-unreviewed"),
                "source": f"https://git.libreoffice.org/dictionaries/+/refs/heads/master/{c['folder']}/",
                "dictionaries": [{"format": c["format"], "files": names, "locales": c["locales"]}]}
        members["pack.json"] = (json.dumps(pack, indent=1, ensure_ascii=False) + "\n").encode()
        zname = f"{cid}-{c['version']}.zip"
        zpath = os.path.join(args.out, zname)
        write_zip(zpath, members)
        built.append({"id": cid, "kind": c["kind"], "version": c["version"], "file": zname,
                      "size": os.path.getsize(zpath), "sha256": sha256(zpath),
                      "installed_size": sum(len(v) for v in members.values()),
                      "locales": c["locales"], "license": pack["license"]})
        print(f"{zname}  {os.path.getsize(zpath) / 1e6:.1f} MB")
    json.dump(built, open(os.path.join(args.out, "components.json"), "w"), indent=1)
    print(f"{len(built)} packs in {args.out}")


def cmd_licenses(args):
    comps = scan_dictionaries(args.only.split(",") if args.only else None)
    licenses = load_licenses()
    for cid, c in sorted(comps.items()):
        folder = os.path.join(DICTIONARIES, c["folder"])
        files = [f for base in (folder, os.path.join(folder, "dictionaries")) if os.path.isdir(base)
                 for f in sorted(os.listdir(base)) if LICENSE_FILE.search(f)]
        print(f"{cid:14} {licenses.get(cid, '(not reviewed)'):40} {c['folder']}/: {', '.join(files)}")


def fetch(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()


def cmd_index(args):
    built = json.load(open(os.path.join(args.out, "components.json")))
    previous = {"sequence": 0, "components": {}}
    if args.previous:
        data = (fetch(args.previous) if re.match(r"https?:", args.previous)
                else open(args.previous, "rb").read())
        previous = json.loads(data)
    elif not args.first:
        try:
            previous = json.loads(fetch(INDEX_URL))
        except Exception as e:  # noqa: BLE001 - any network problem
            fail(f"could not read the published index ({e}); pass --first for the very first one")
    base = args.base_url if args.base_url is not None else \
        f"https://github.com/{REPO}/releases/download/{args.tag}/"
    comps, new_files = {}, []
    for b in built:
        old = previous.get("components", {}).get(b["id"])
        if old and old.get("sha256") == b["sha256"]:
            url = old["url"]  # unchanged: keep the asset of an earlier release
        else:
            url = base + b["file"]
            new_files.append(b["file"])
        comps[b["id"]] = {k: b[k] for k in ("kind", "version", "size", "installed_size", "sha256",
                                             "locales", "license")}
        comps[b["id"]]["url"] = url
    languages = {}
    for cid, c in sorted(comps.items()):
        for loc in c["locales"]:
            row = languages.setdefault(loc, {"tag": loc, "name": loc, "autonym": "",
                                             "components": {}})
            current = row["components"].get(c["kind"])
            # the component made for this exact locale wins over a shared one
            if current is None or cid.startswith(loc + "."):
                row["components"][c["kind"]] = cid
    index = {"schema": 1, "sequence": previous.get("sequence", 0) + 1,
             "min_app_build": args.min_app_build,
             "source": {"repo": "https://git.libreoffice.org/dictionaries",
                        "commit": subprocess.run(["git", "-C", DICTIONARIES, "rev-parse", "HEAD"],
                                                 text=True, capture_output=True).stdout.strip()},
             "languages": [dict(r, components=[r["components"][k] for k in ("spell", "hyph", "thes")
                                               if k in r["components"]])
                           for _, r in sorted(languages.items())],
             "components": comps}
    with open(os.path.join(args.out, "index.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1, ensure_ascii=False)
        f.write("\n")
    json.dump(new_files, open(os.path.join(args.out, "new-files.json"), "w"))
    print(f"index.json: sequence {index['sequence']}, {len(comps)} components, "
          f"{len(index['languages'])} languages, {len(new_files)} new or changed packs")


def cmd_sign(args):
    path = os.path.join(args.out, "index.json")
    data = open(path, "rb").read()
    seed = load_seed(args.key)
    public = public_key_from_schema()
    if ed25519_public(seed) != public:
        fail("the signing key does not match DainvoLanguagePacks/PublicKey in Linguistic.xcs")
    sig = ed25519_sign(seed, data)
    if not ed25519_verify(public, sig, data):
        fail("signature check failed")
    open(path + ".sig", "w").write(base64.b64encode(sig).decode() + "\n")
    print(f"signed {path}")


def cmd_verify(args):
    public = public_key_from_schema()
    if args.remote:
        data, sig = fetch(INDEX_URL), fetch(INDEX_URL + ".sig")
        folder = None
    else:
        folder = args.dir or args.out
        data = open(os.path.join(folder, "index.json"), "rb").read()
        sig = open(os.path.join(folder, "index.json.sig"), "rb").read()
    if not ed25519_verify(public, base64.b64decode(sig.strip()), data):
        fail("index.json is not correctly signed")
    index = json.loads(data)
    print(f"signature OK, sequence {index['sequence']}, {len(index['components'])} components")
    bad = 0
    for cid, c in sorted(index["components"].items()):
        local = folder and os.path.join(folder, c["url"].rsplit("/", 1)[-1])
        if local and os.path.isfile(local):
            if sha256(local) != c["sha256"]:
                print(f"checksum mismatch: {cid}")
                bad += 1
        elif args.remote and args.download:
            if hashlib.sha256(fetch(c["url"])).hexdigest() != c["sha256"]:
                print(f"checksum mismatch: {cid}")
                bad += 1
    if bad:
        fail(f"{bad} packs do not match the index")
    print("checksums OK")


def cmd_publish(args):
    new_files = json.load(open(os.path.join(args.out, "new-files.json")))
    assets = [os.path.join(args.out, f) for f in ["index.json", "index.json.sig"] + new_files]
    for a in assets:
        if not os.path.isfile(a):
            fail(f"missing {a}")
    print(f"Creating release {args.tag} in {REPO} with {len(assets)} files.")
    if not args.yes and input("Continue? [y/N] ").strip().lower() != "y":
        fail("cancelled")
    subprocess.run(["gh", "release", "create", args.tag, "--repo", REPO, "--title",
                    f"Language packs {args.tag}", "--notes",
                    "Dainvo Office language packs. Installed from inside the app (Review > "
                    "Languages); not meant to be downloaded by hand.", "--latest"] + assets,
                   check=True)


def cmd_keygen(args):
    if os.path.exists(args.out):
        fail(f"{args.out} exists")
    seed = os.urandom(32)
    fd = os.open(args.out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write(base64.b64encode(seed).decode() + "\n")
    print("public key for DainvoLanguagePacks/PublicKey in Linguistic.xcs:")
    print(base64.b64encode(ed25519_public(seed)).decode())


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--only", help="comma-separated language prefixes, e.g. de,fr,pt-BR")
    b.add_argument("--allow-unreviewed", action="store_true")
    b.add_argument("--skip-unreviewed", action="store_true",
                   help="leave out components without a reviewed licence")
    lic = sub.add_parser("licenses")
    lic.add_argument("--only")
    i = sub.add_parser("index")
    i.add_argument("--tag", required=True, help="release tag, e.g. 2026.11.1")
    i.add_argument("--previous", help="previous index.json (file or URL); default: the published one")
    i.add_argument("--first", action="store_true", help="no published index yet")
    i.add_argument("--base-url", help="URL prefix for new packs; '' writes relative names")
    i.add_argument("--min-app-build", type=int, default=0)
    s = sub.add_parser("sign")
    s.add_argument("--key", help="file with the base64 Ed25519 seed")
    v = sub.add_parser("verify")
    v.add_argument("dir", nargs="?")
    v.add_argument("--remote", action="store_true", help="check the published index")
    v.add_argument("--download", action="store_true", help="with --remote: download every pack")
    pub = sub.add_parser("publish")
    pub.add_argument("--tag", required=True)
    pub.add_argument("--yes", action="store_true")
    k = sub.add_parser("keygen")
    k.add_argument("--out", required=True)
    for sp in (b, i, s, v, pub):
        sp.add_argument("--out", default=OUT, help=f"pack folder (default {OUT})")
    args = p.parse_args()
    {"build": cmd_build, "licenses": cmd_licenses, "index": cmd_index, "sign": cmd_sign,
     "verify": cmd_verify, "publish": cmd_publish, "keygen": cmd_keygen}[args.cmd](args)


if __name__ == "__main__":
    main()
