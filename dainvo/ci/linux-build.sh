#!/usr/bin/env bash
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Linux release build, run by .github/workflows/release.yml inside the
# quay.io/pypa/manylinux_2_34_x86_64 container (AlmaLinux 9, glibc 2.34,
# gcc-toolset with a static libstdc++ supplement). AlmaLinux 9 supplies the
# GTK, GLib and ATK versions required by this LibreOffice source.
#
# Usage (from the repository root, as root in the container):
#   dainvo/ci/linux-build.sh <output-dir>
set -euo pipefail
out=${1:?usage: linux-build.sh <output-dir>}
src=$(cd "$(dirname "$0")/../.." && pwd)
cd "$src"

NFPM_VERSION=2.43.0
NFPM_SHA256=${NFPM_SHA256:-}

echo "::group::Install build dependencies"
dnf -y install dnf-plugins-core epel-release
dnf config-manager --set-enabled crb
dnf -y install \
    autoconf automake libtool bison flex gperf make patch which file \
    zip unzip xz wget git ccache nasm ninja-build \
    perl perl-Digest-MD5 'perl(FindBin)' perl-IPC-Cmd 'perl(lib)' perl-Archive-Zip \
    gtk3-devel atk-devel cups-devel fontconfig-devel freetype-devel zlib-devel \
    libX11-devel libXext-devel libXrender-devel libXrandr-devel libXinerama-devel \
    libXt-devel libSM-devel libICE-devel dbus-devel glib2-devel krb5-devel \
    gstreamer1-devel gstreamer1-plugins-base-devel
echo "::endgroup::"

# Build-time Python (3.11+); the product itself ships without Python.
export CI_PYTHON=/opt/python/cp312-cp312/bin/python3
export PATH="/opt/python/cp312-cp312/bin:$PATH"
gcc --version | head -1

echo "::group::nfpm $NFPM_VERSION"
nfpm_tar="nfpm_${NFPM_VERSION}_Linux_x86_64.tar.gz"
wget -q "https://github.com/goreleaser/nfpm/releases/download/v${NFPM_VERSION}/$nfpm_tar" -O "/tmp/$nfpm_tar"
wget -q "https://github.com/goreleaser/nfpm/releases/download/v${NFPM_VERSION}/checksums.txt" -O /tmp/nfpm-checksums.txt
(cd /tmp && grep " $nfpm_tar\$" nfpm-checksums.txt | sha256sum -c -)
tar -xzf "/tmp/$nfpm_tar" -C /usr/local/bin nfpm
nfpm --version
echo "::endgroup::"

export CCACHE_DIR=${CCACHE_DIR:-/ccache}
export CCACHE_MAXSIZE=${CCACHE_MAXSIZE:-3G}
export CCACHE_COMPRESS=1
ccache --zero-stats

echo "::group::Configure"
dainvo/ci/write-autogen-input.sh linux
./autogen.sh
echo "::endgroup::"

echo "::group::Build"
make
echo "::endgroup::"
ccache --show-stats

echo "::group::Package"
dainvo/packaging/linux/package.sh "$out"
ls -l "$out"
echo "::endgroup::"
