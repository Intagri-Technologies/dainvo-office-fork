/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/config.h>

#include <optional>
#include <string_view>

#include <rtl/ustring.hxx>
#include <sfx2/dllapi.h>

class SfxViewFrame;
namespace weld
{
class Window;
}

/// Dainvo Office updates (--enable-dainvo-updater).
///
/// All platforms read the same signed Sparkle appcast (DAINVO_UPDATE_FEED) and
/// compare its sparkle:version with this build's DAINVO_BUILD:
/// - macOS: Sparkle 2 (download, EdDSA check, install, relaunch)
/// - Windows: WinSparkle (same, for the MSI)
/// - elsewhere, or if those libraries are missing: a built-in daily check
///   that shows an infobar with a link to the release page.
/// See dainvo/docs/RELEASES.md.
namespace sfx2::DainvoUpdater
{
/// Whether this build has an updater at all (drives Help > Check for Updates...).
SFX2_DLLPUBLIC bool IsAvailable();

/// Start automatic checks. Called once from desktop after start-up.
SFX2_DLLPUBLIC void Start();

/// Stop background work. Called from desktop at shutdown.
SFX2_DLLPUBLIC void Stop();

/// Help > Check for Updates...: check now and always report the result.
SFX2_DLLPUBLIC void CheckNow(weld::Window* pParent);

/// Show an infobar for an update found by the built-in checker, once per session.
SFX2_DLLPUBLIC void ShowPendingNotice(SfxViewFrame& rFrame);

struct Release
{
    sal_Int64 nBuild = 0;
    OUString aVersion; ///< sparkle:shortVersionString, e.g. "0.2.0"
    OUString aPageURL; ///< <link>: the release page
    OUString aDownloadURL; ///< enclosure url, may be empty
};

/// Pick the newest item for rOS ("macos", "windows", "linux") from a Sparkle
/// appcast. Items without an enclosure count for every OS. Returns nothing if
/// the feed can't be parsed or has no item for rOS.
SFX2_DLLPUBLIC std::optional<Release> ParseAppcast(std::string_view aXml,
                                                   std::u16string_view aOS);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
