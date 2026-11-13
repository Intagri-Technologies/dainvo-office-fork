#!/usr/bin/env bash
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Writes autogen.input for a CI release build.
# Usage: dainvo/ci/write-autogen-input.sh macos|windows|linux
# Optional environment:
#   MACOS_SIGNING_IDENTITY   Developer ID Application certificate fingerprint -> signed app
#   WINDOWS_SIGNING=1        sign Windows binaries (needs PFXFILE/PFXPASSWORD)
#   CI_PYTHON                build-time Python (3.11+) if not "python3"
set -euo pipefail
platform=${1:?usage: write-autogen-input.sh macos|windows|linux}
src=$(cd "$(dirname "$0")/../.." && pwd)
case $(uname -s) in
    CYGWIN*) branding=$(cygpath -m "$src/dainvo/branding") ;;
    *) branding="$src/dainvo/branding" ;;
esac
jobs=$( (nproc || sysctl -n hw.ncpu) 2>/dev/null )

{
    case $platform in
        macos) echo "INCLUDE:DainvoMacOS" ;;
        windows) echo "INCLUDE:DainvoWindows" ;;
        linux) echo "INCLUDE:DainvoLinux" ;;
        *) echo "unknown platform $platform" >&2; exit 1 ;;
    esac
    echo "--with-branding=$branding"
    echo "--with-parallelism=$jobs"
    case $platform in
        macos)
            echo "--enable-ccache"
            echo "PKG_CONFIG=$(command -v pkg-config)"
            if [ -n "${MACOS_SIGNING_IDENTITY:-}" ]; then
                echo "--enable-macosx-code-signing=$MACOS_SIGNING_IDENTITY"
            fi
            ;;
        linux)
            echo "--enable-ccache"
            ;;
        windows)
            echo "--with-visual-studio=2022"
            if [ -n "${WINDOWS_SIGNING:-}" ]; then
                echo "--enable-windows-build-signing"
            fi
            ;;
    esac
    if [ -n "${CI_PYTHON:-}" ]; then
        echo "PYTHON=$CI_PYTHON"
    fi
} > "$src/autogen.input"

echo "autogen.input:"
sed 's/^/    /' "$src/autogen.input"
