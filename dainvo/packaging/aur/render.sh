#!/usr/bin/env bash
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Writes PKGBUILD and .SRCINFO for the AUR package dainvo-office-bin.
# Usage: render.sh <version> <linux-x86_64.tar.gz> <output-dir> [pkgrel]
# The release workflow runs it after a release is published and pushes the
# result to ssh://aur@aur.archlinux.org/dainvo-office-bin.git.
set -euo pipefail
version=${1:?version}
tarball=${2:?tarball}
out=${3:?output dir}
pkgrel=${4:-1}
here=$(cd "$(dirname "$0")" && pwd)
sha=$( (sha256sum "$tarball" 2>/dev/null || shasum -a 256 "$tarball") | cut -d' ' -f1)
maintainer=${AUR_MAINTAINER:-Intagri Technologies}
mkdir -p "$out"
for f in PKGBUILD:PKGBUILD.in .SRCINFO:SRCINFO.in; do
    sed -e "s|@VERSION@|$version|g" -e "s|@PKGREL@|$pkgrel|g" -e "s|@SHA256@|$sha|g" \
        -e "s|@MAINTAINER@|$maintainer|g" "$here/${f#*:}" > "$out/${f%%:*}"
done
echo "wrote $out/PKGBUILD and $out/.SRCINFO for $version-$pkgrel ($sha)"
