#!/bin/sh
# Refresh the desktop menu and icon caches after install/remove (.deb, .rpm).
# Failures are ignored: the caches are an optimisation only.
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
exit 0
