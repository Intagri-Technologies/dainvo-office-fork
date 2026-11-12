/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/types.h>
#include <array>
#include <cstddef>

namespace sfx2::metadata
{
// Private protocol between binaries from the same build/architecture. Each URL
// yields {valid, seconds, nanoseconds}. Never send paths or document contents back.
using Record = std::array<sal_uInt32, 3>;
inline constexpr std::size_t MaxURLs = 32;
static_assert(sizeof(Record) * MaxURLs <= 512); // POSIX minimum pipe capacity
}
