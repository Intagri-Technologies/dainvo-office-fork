/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <recentfilemetadataprotocol.hxx>
#include <osl/file.hxx>
#include <osl/process.h>
#include <rtl/ustring.hxx>
#include <sal/main.h>

#include <cstdio>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

// Keep this helper independent of VCL, UNO, configuration and document loading.
// It is safe to terminate at any point: it only reads filesystem metadata.
SAL_IMPLEMENT_MAIN()
{
    const auto nCount = osl_getCommandArgCount();
    if (nCount > sfx2::metadata::MaxURLs)
        return 1;
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    for (sal_uInt32 i = 0; i < nCount; ++i)
    {
        OUString aURL;
        if (osl_getCommandArg(i, &aURL.pData) != osl_Process_E_None)
            return 1;
        sfx2::metadata::Record aRecord{};
        osl::DirectoryItem aItem;
        osl::FileStatus aStatus(osl_FileStatus_Mask_ModifyTime);
        if (aURL.startsWith("file:")
            && osl::DirectoryItem::get(aURL, aItem) == osl::FileBase::E_None
            && aItem.getFileStatus(aStatus) == osl::FileBase::E_None
            && aStatus.isValid(osl_FileStatus_Mask_ModifyTime))
        {
            const auto aTime = aStatus.getModifyTime();
            aRecord = { 1, aTime.Seconds, aTime.Nanosec };
        }
        if (std::fwrite(aRecord.data(), sizeof(aRecord), 1, stdout) != 1)
            return 1;
    }
    return std::fflush(stdout) == 0 ? 0 : 1;
}
