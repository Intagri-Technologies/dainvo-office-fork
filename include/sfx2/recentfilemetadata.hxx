/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sfx2/dllapi.h>

namespace sfx2
{
/// UI-thread shutdown barrier: cancel all current requests before joining the
/// application pool or tearing down VCL. Never waits for filesystem I/O in a child.
SFX2_DLLPUBLIC void cancelRecentFileMetadataRequests();
}
