#!/usr/bin/env bash
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Makes the Dainvo Office Linux packages from a finished build:
#
#   Dainvo-Office-<version>-linux-x86_64.tar.gz   install tree (also used by the AUR package)
#   Dainvo-Office-<version>-linux-amd64.deb       Debian, Ubuntu
#   Dainvo-Office-<version>-linux-x86_64.rpm      Fedora, openSUSE, RHEL
#
# Usage: dainvo/packaging/linux/package.sh <output-dir>
# Run after "make" in a tree configured with distro-configs/DainvoLinux.conf.
# The .deb and .rpm need nfpm (https://nfpm.goreleaser.com) on PATH; without
# it only the .tar.gz is made.
#
# Layout of all three: the program in /opt/dainvo-office, the launcher
# /usr/bin/dainvo-office, desktop entries and icons in /usr/share.

set -euo pipefail

out=${1:?usage: package.sh <output-dir>}
here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../../.." && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)

conf() { sed -n "s/^export $1=//p" "$src/config_host.mk"; }
version=$(conf DAINVO_VERSION)
build=$(conf DAINVO_BUILD)
[ -n "$version" ] && [ -n "$build" ] || { echo "error: config_host.mk has no DAINVO_VERSION/DAINVO_BUILD; configure with DainvoLinux.conf" >&2; exit 1; }
case $(uname -m) in
    x86_64) arch=x86_64 debarch=amd64 ;;
    aarch64) arch=aarch64 debarch=arm64 ;;
    *) echo "error: unsupported machine $(uname -m)" >&2; exit 1 ;;
esac
name=dainvo-office
base="Dainvo-Office-$version-linux"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
root="$work/root"

echo "Installing the program tree (stripped) into /opt/$name"
make -C "$src" cmd cmd="$src/solenv/bin/ooinstall --strip $root/opt/$name"
test -x "$root/opt/$name/program/soffice" || { echo "error: ooinstall left no program/soffice" >&2; exit 1; }

install -d "$root/usr/bin" "$root/usr/share/applications"
cat > "$root/usr/bin/$name" <<EOF
#!/bin/sh
exec /opt/$name/program/soffice "\$@"
EOF
chmod 755 "$root/usr/bin/$name"
install -m 644 "$here"/*.desktop "$root/usr/share/applications/"

icons="$src/sysui/desktop/icons/hicolor"
for dir in "$icons"/*/apps; do
    size=$(basename "$(dirname "$dir")")
    case $size in *@*) continue ;; esac
    for icon in main writer calc impress; do
        for ext in png svg; do
            if [ -f "$dir/$icon.$ext" ]; then
                install -D -m 644 "$dir/$icon.$ext" "$root/usr/share/icons/hicolor/$size/apps/$name-$icon.$ext"
            fi
        done
    done
done

tarball="$out/$base-$arch.tar.gz"
echo "Writing $tarball"
tar -C "$root" --owner=0 --group=0 --numeric-owner -czf "$tarball" opt usr

if ! command -v nfpm >/dev/null; then
    echo "nfpm not found: skipping .deb and .rpm"
    exit 0
fi

# nfpm wants every file under /usr/share listed, so the package doesn't claim
# /usr/share itself (rpm would conflict with the filesystem package).
contents=$(cd "$root" && find usr -type f | sort | while read -r f; do
    printf '  - src: %s\n    dst: /%s\n' "$root/$f" "$f"
done)

cat > "$work/nfpm.yaml" <<EOF
name: $name
arch: $debarch
platform: linux
version: $version
release: $build
section: editors
priority: optional
maintainer: ${DAINVO_PACKAGE_MAINTAINER:-Intagri Technologies}
vendor: Dainvo
homepage: https://github.com/Intagri-Technologies/dainvo-office-releases
license: MPL-2.0
description: |-
  Dainvo Office office suite
  Word processor (Writer), spreadsheets (Calc) and presentations (Impress).
contents:
  - src: $root/opt/$name
    dst: /opt/$name
    type: tree
$contents
scripts:
  postinstall: $here/postinst.sh
  postremove: $here/postinst.sh
overrides:
  deb:
    depends:
      - libc6 (>= 2.28)
      - libfontconfig1
      - libfreetype6
      - libcups2 | libcups2t64
      - libgtk-3-0 | libgtk-3-0t64
      - libglib2.0-0 | libglib2.0-0t64
      - libdbus-1-3
      - libx11-6
      - libxext6
      - libxrender1
      - libxinerama1
      - libxrandr2
      - libsm6
      - libice6
    recommends:
      - hunspell-en-us
      - hyphen-en-us
      - mythes-en-us
      - gstreamer1.0-plugins-base
      - fonts-crosextra-carlito
      - fonts-crosextra-caladea
  rpm:
    depends:
      - glibc >= 2.28
      - fontconfig
      - freetype
      - cups-libs
      - gtk3
      - glib2
      - dbus-libs
      - libX11
      - libXext
      - libXrender
      - libXinerama
      - libXrandr
      - libSM
      - libICE
    recommends:
      - hunspell-en-US
      - hyphen-en
      - mythes-en
      - gstreamer1-plugins-base
EOF

echo "Writing $base-$debarch.deb"
nfpm package --config "$work/nfpm.yaml" --packager deb --target "$out/$base-$debarch.deb"
echo "Writing $base-$arch.rpm"
nfpm package --config "$work/nfpm.yaml" --packager rpm --target "$out/$base-$arch.rpm"
