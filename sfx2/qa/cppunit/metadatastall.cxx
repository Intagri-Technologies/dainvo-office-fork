/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <osl/file.hxx>
#include <osl/process.h>
#include <osl/thread.h>
#include <rtl/ustring.hxx>
#include <sal/main.h>

// Test-only child. Signal that the simulated blocking syscall has started, then
// stay blocked much longer than the supervisor's deadline. Never installed.
SAL_IMPLEMENT_MAIN()
{
    OUString aMarkerURL;
    if (osl_getCommandArg(0, &aMarkerURL.pData) != osl_Process_E_None)
        return 1;
    osl::File aMarker(aMarkerURL);
    if (aMarker.open(osl_File_OpenFlag_Write) != osl::FileBase::E_None)
        return 1;
    oslProcessInfo aInfo{};
    aInfo.Size = sizeof(aInfo);
    if (osl_getProcessInfo(nullptr, osl_Process_IDENTIFIER, &aInfo) != osl_Process_E_None)
        return 1;
    sal_uInt64 nWritten = 0;
    if (aMarker.write(&aInfo.Ident, sizeof(aInfo.Ident), nWritten) != osl::FileBase::E_None
        || nWritten != sizeof(aInfo.Ident))
        return 1;
    aMarker.close();
    const TimeValue aDelay(30, 0);
    osl_waitThread(&aDelay);
    return 1;
}
