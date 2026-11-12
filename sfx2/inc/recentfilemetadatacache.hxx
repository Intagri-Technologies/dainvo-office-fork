/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <osl/time.h>
#include <rtl/ustring.hxx>

#include <map>
#include <optional>
#include <vector>

namespace sfx2
{
/// A Start Center snapshot, not a read-through cache: presentation must never do I/O.
/// refresh() is the only load boundary. Missing files are cached as empty values,
/// and replacing the snapshot evicts URLs no longer present in recent history.
class RecentFileMetadataCache
{
public:
    using ModificationTime = std::optional<TimeValue>;

    template <typename Loader> void refresh(const std::vector<OUString>& rURLs, Loader load)
    {
        std::map<OUString, ModificationTime> aNext;
        for (const auto& rURL : rURLs)
        {
            if (!aNext.contains(rURL))
                aNext.emplace(rURL, load(rURL));
        }
        maModifiedTimes.swap(aNext);
    }

    ModificationTime lookup(const OUString& rURL) const
    {
        const auto it = maModifiedTimes.find(rURL);
        return it == maModifiedTimes.end() ? ModificationTime() : it->second;
    }

    std::size_t size() const { return maModifiedTimes.size(); }

private:
    std::map<OUString, ModificationTime> maModifiedTimes;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
