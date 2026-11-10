/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <algorithm>
#include <string.h>

#include <pagedata.hxx>

#include <osl/diagnose.h>

ScPrintRangeData::ScPrintRangeData()
{
    bTopDown = bAutomatic = true;
    nFirstPage = 1;
}

ScPrintRangeData::~ScPrintRangeData()
{
}

void ScPrintRangeData::SetPagesX( size_t nCount, const SCCOL* pData )
{
    mvPageEndX.resize( nCount );
    std::copy_n( pData, nCount, mvPageEndX.data() );
}

void ScPrintRangeData::SetPagesY( size_t nCount, const SCROW* pData )
{
    mvPageEndY.resize(nCount);
    std::copy_n( pData, nCount, mvPageEndY.data() );
}

tools::Long ScPrintRangeData::GetPageNumber(SCCOL nCol, SCROW nRow) const
{
    size_t nColPos = 0;
    while (nColPos + 1 < mvPageEndX.size() && nCol > mvPageEndX[nColPos])
        ++nColPos;

    size_t nRowPos = 0;
    while (nRowPos + 1 < mvPageEndY.size() && nRow > mvPageEndY[nRowPos])
        ++nRowPos;

    if (bTopDown)
        return nFirstPage
               + static_cast<tools::Long>(nColPos) * mvPageEndY.size()
               + nRowPos;

    return nFirstPage
           + static_cast<tools::Long>(nRowPos) * mvPageEndX.size()
           + nColPos;
}

ScPageBreakData::ScPageBreakData(size_t nMax)
{
    nUsed = 0;
    if (nMax)
        pData.reset( new ScPrintRangeData[nMax] );
    nAlloc = nMax;
}

ScPageBreakData::~ScPageBreakData()
{
}

ScPrintRangeData& ScPageBreakData::GetData(size_t nPos)
{
    OSL_ENSURE(nPos < nAlloc, "ScPageBreakData::GetData bumm");

    if ( nPos >= nUsed )
    {
        OSL_ENSURE(nPos == nUsed, "ScPageBreakData::GetData wrong order");
        nUsed = nPos+1;
    }

    return pData[nPos];
}

const ScPrintRangeData& ScPageBreakData::GetData(size_t nPos) const
{
    OSL_ENSURE(nPos < nUsed, "ScPageBreakData::GetData out of bounds");
    return pData[nPos];
}

bool ScPageBreakData::operator==( const ScPageBreakData& rOther ) const
{
    if ( nUsed != rOther.nUsed )
        return false;

    for (size_t i=0; i<nUsed; i++)
        if ( pData[i].GetPrintRange() != rOther.pData[i].GetPrintRange() )
            return false;

    //! compare ScPrintRangeData completely ??

    return true;
}

void ScPageBreakData::AddPages()
{
    if ( nUsed > 1 )
    {
        tools::Long nPage = pData[0].GetFirstPage();
        for (size_t i=0; i+1<nUsed; i++)
        {
            nPage += static_cast<tools::Long>(pData[i].GetPagesX())*pData[i].GetPagesY();
            pData[i+1].SetFirstPage( nPage );
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
