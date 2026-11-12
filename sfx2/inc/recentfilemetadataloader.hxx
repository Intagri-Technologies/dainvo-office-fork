/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <recentfilemetadatacache.hxx>
#include <sfx2/dllapi.h>

#include <functional>
#include <memory>

namespace sfx2
{
/// UI-thread-owned client of a supervised metadata helper process. Each client has at most one
/// running batch and one pending request; refresh() replaces the pending request.
/// Completion runs on the VCL UI thread, and only for the latest generation.
/// Destruction cancels delivery without joining a potentially blocked file query.
class SFX2_DLLPUBLIC RecentFileMetadataLoader final
{
public:
    using Loader = std::function<RecentFileMetadataCache::ModificationTime(const OUString&)>;
    using Completion = std::function<void(RecentFileMetadataCache)>;

    // Reader/helper overrides are test seams; production reads files only in the helper.
    explicit RecentFileMetadataLoader(Completion complete, Loader load = {},
                                      OUString aHelperURL = {});
    static void cancelAll();
    ~RecentFileMetadataLoader();
    RecentFileMetadataLoader(const RecentFileMetadataLoader&) = delete;
    RecentFileMetadataLoader& operator=(const RecentFileMetadataLoader&) = delete;

    void refresh(std::vector<OUString> aURLs);

private:
    struct Impl;
    std::shared_ptr<Impl> mpImpl;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
