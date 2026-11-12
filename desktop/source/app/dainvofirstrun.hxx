/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace desktop
{
/// Dainvo: ask once which file formats new documents are saved in
/// (Microsoft Office or OpenDocument). Does nothing in headless mode or after
/// the user has answered.
void DainvoAskDefaultFileFormats();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
