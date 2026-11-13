#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Check a mounted installer or an extracted package tree for upstream branding."""

import argparse
from io import BytesIO
from pathlib import Path, PurePosixPath
import re
import sys
from zipfile import ZipFile, is_zipfile


BRAND = re.compile(
    b"|".join(re.escape("libreoffice".encode(encoding)) for encoding in (
        "ascii", "utf-16le", "utf-16be", "utf-32le", "utf-32be"
    )),
    re.IGNORECASE,
)


def is_notice(path):
    parts = path.as_posix().lower().split("/")
    return (
        any(part in {"licenses", "license", "copying"} for part in parts[:-1])
        or parts[-1] in {
            "license", "license.html", "license.txt", "license.fodt",
            "notice", "notice.txt", "copying", "copying.mpl", "copying.lgpl",
            "credits.fodt",
        }
    )


def check_content(path, data):
    if not is_notice(path) and BRAND.search(data):
        yield f"{path} (contents)"
    if is_zipfile(BytesIO(data)):
        with ZipFile(BytesIO(data)) as archive:
            for member in archive.infolist():
                member_path = path / member.filename
                if BRAND.search(member.filename.encode("utf-8")):
                    yield f"{member_path} (name)"
                if not member.is_dir():
                    yield from check_content(member_path, archive.read(member))


def audit(root):
    for path in sorted(root.rglob("*")):
        relative = PurePosixPath(path.relative_to(root).as_posix())
        if BRAND.search(path.name.encode("utf-8")):
            yield f"{relative} (name)"
        if path.is_symlink():
            if BRAND.search(str(path.readlink()).encode("utf-8")):
                yield f"{relative} (link target)"
        elif path.is_file():
            yield from check_content(relative, path.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="mounted installer or extracted package directory")
    args = parser.parse_args()
    if not args.root.is_dir():
        parser.error(f"expected a directory: {args.root}")
    count = 0
    for finding in audit(args.root):
        print(finding)
        count += 1
    print(f"{count} branding matches outside license and notice contents", file=sys.stderr)
    return int(count > 0)


if __name__ == "__main__":
    sys.exit(main())
