/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_version.h>
#include <dataprovider.hxx>
#include <com/sun/star/ucb/XSimpleFileAccess3.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <rtl/strbuf.hxx>
#include <sal/log.hxx>
#include <unotools/charclass.hxx>
#include <tools/stream.hxx>
#include <comphelper/processfactory.hxx>
#include <tools/hostfilter.hxx>

#include "htmldataprovider.hxx"
#include "xmldataprovider.hxx"
#include "sqldataprovider.hxx"
#include <datamapper.hxx>
#include <dbdata.hxx>
#include <docsh.hxx>
#include <document.hxx>
#include <comphelper/embeddedobjectcontainer.hxx>
#include <sfx2/lnkbase.hxx>
#include <sfx2/linkmgr.hxx>
#include <utility>

using namespace com::sun::star;

namespace sc {

std::unique_ptr<SvStream> DataProvider::FetchStreamFromURL(const OUString& rURL, OStringBuffer& rBuffer)
{
    if (HostFilter::isUrlForbidden(rURL))
    {
        SAL_WARN("sc.ui", "DataProvider::FetchStreamFromURL: blocked url: \"" << rURL << "\"");
        return nullptr;
    }

    try
    {
        uno::Reference< ucb::XSimpleFileAccess3 > xFileAccess = ucb::SimpleFileAccess::create( comphelper::getProcessComponentContext() );

        uno::Reference< io::XInputStream > xStream = xFileAccess->openFileRead( rURL );

        const sal_Int32 BUF_LEN = 8000;
        uno::Sequence< sal_Int8 > buffer( BUF_LEN );

        sal_Int32 nRead = 0;
        while ( ( nRead = xStream->readBytes( buffer, BUF_LEN ) ) == BUF_LEN )
        {
            rBuffer.append( reinterpret_cast< const char* >( buffer.getConstArray() ), nRead );
        }

        if ( nRead > 0 )
        {
            rBuffer.append( reinterpret_cast< const char* >( buffer.getConstArray() ), nRead );
        }

        xStream->closeInput();

        SvStream* pStream = new SvMemoryStream(const_cast<char*>(rBuffer.getStr()), rBuffer.getLength(), StreamMode::READ);
        return std::unique_ptr<SvStream>(pStream);
    }
    catch(...)
    {
        rBuffer.setLength(0);
        return nullptr;
    }
}

namespace
{
constexpr std::u16string_view LEGACY_PROVIDER_PREFIX = u"org.libreoffice.calc.";
constexpr std::u16string_view DAINVO_PROVIDER_PREFIX = u"com.dainvo.office.calc.";

OUString normalizeProvider(const OUString& rProvider)
{
#ifdef DAINVO_VERSION
    if (rProvider.startsWith(LEGACY_PROVIDER_PREFIX))
        return rProvider.replaceAt(0, LEGACY_PROVIDER_PREFIX.size(), DAINVO_PROVIDER_PREFIX);
#else
    if (rProvider.startsWith(DAINVO_PROVIDER_PREFIX))
        return rProvider.replaceAt(0, DAINVO_PROVIDER_PREFIX.size(), LEGACY_PROVIDER_PREFIX);
#endif
    return rProvider;
}
}

DataProviderKind getDataProviderKind(std::u16string_view rProvider)
{
    if (rProvider.starts_with(DAINVO_PROVIDER_PREFIX))
        rProvider.remove_prefix(DAINVO_PROVIDER_PREFIX.size());
    else if (rProvider.starts_with(LEGACY_PROVIDER_PREFIX))
        rProvider.remove_prefix(LEGACY_PROVIDER_PREFIX.size());
    else
        return DataProviderKind::Unknown;

    if (rProvider == u"csv")
        return DataProviderKind::Csv;
    if (rProvider == u"html")
        return DataProviderKind::Html;
    if (rProvider == u"xml")
        return DataProviderKind::Xml;
    if (rProvider == u"sql")
        return DataProviderKind::Sql;
    return DataProviderKind::Unknown;
}

ExternalDataSource::ExternalDataSource(OUString aURL,
        OUString aProvider, ScDocument* pDoc)
    : maURL(std::move(aURL))
    , maProvider(normalizeProvider(aProvider))
    , mpDoc(pDoc)
{
}

void ExternalDataSource::setID(const OUString& rID)
{
    maID = rID;
}

void ExternalDataSource::setXMLImportParam(const ScOrcusImportXMLParam& rParam)
{
    maParam = rParam;
}



void ExternalDataSource::setURL(const OUString& rURL)
{
    maURL = rURL;
}

void ExternalDataSource::setProvider(const OUString& rProvider)
{
    maProvider = normalizeProvider(rProvider);
    mpDataProvider.reset();
}

const OUString& ExternalDataSource::getURL() const
{
    return maURL;
}

const OUString& ExternalDataSource::getProvider() const
{
    return maProvider;
}

const OUString& ExternalDataSource::getID() const
{
    return maID;
}

const ScOrcusImportXMLParam& ExternalDataSource::getXMLImportParam() const
{
    return maParam;
}

OUString ExternalDataSource::getDBName() const
{
    if (mpDBDataManager)
    {
        ScDBData* pDBData = mpDBDataManager->getDBData();
        if (pDBData)
            return pDBData->GetName();
    }
    return OUString();
}

void ExternalDataSource::setDBData(const OUString& rDBName)
{
    if (!mpDBDataManager)
    {
        mpDBDataManager = std::make_shared<ScDBDataManager>(rDBName, mpDoc);
    }
    else
    {
        mpDBDataManager->SetDatabase(rDBName);
    }
}

double ExternalDataSource::getUpdateFrequency()
{
    return 0;
}

ScDBDataManager* ExternalDataSource::getDBManager()
{
    return mpDBDataManager.get();
}

void ExternalDataSource::refresh(ScDocument* pDoc, bool bDeterministic)
{
    // no DB data available
    if (!mpDBDataManager)
        return;

    // tdf#169541 An existing mpDataProvider might have a corrupted backreference to `this`.
    // Thus create a new one in any case.
    mpDataProvider = DataProviderFactory::getDataProvider(pDoc, *this);

    // if we have not been able to create one, we can not refresh the data
    if (!mpDataProvider)
        return;

    if (bDeterministic)
        mpDataProvider->setDeterministic();

    mpDataProvider->Import();
}

void ExternalDataSource::AddDataTransformation(
    const std::shared_ptr<sc::DataTransformation>& mpDataTransformation)
{
    maDataTransformations.push_back(mpDataTransformation);
}

const std::vector<std::shared_ptr<sc::DataTransformation>>& ExternalDataSource::getDataTransformation() const
{
    return maDataTransformations;
}

namespace {

// A data mapping fetches external data (CSV, HTML, XML) into a sheet. It is
// registered in the LinkManager so it passes through the same link update
// control as sheet links, area links, and external references. The link holds
// the index of its data source in the mapper. That index is stable because
// data sources are only ever appended, never removed.
class ScDataProviderLink final : public sfx2::SvBaseLink
{
    ScDocShell& mrDocShell;
    size_t mnSourceIndex;

public:
    ScDataProviderLink(ScDocShell& rDocShell, size_t nSourceIndex)
        : sfx2::SvBaseLink(SfxLinkUpdateMode::ONCALL, SotClipboardFormatId::SIMPLE_FILE)
        , mrDocShell(rDocShell)
        , mnSourceIndex(nSourceIndex)
    {
    }

    virtual sfx2::SvBaseLink::UpdateResult DataChanged(const OUString&,
                                                       const css::uno::Any&) override
    {
        if (!mrDocShell.GetEmbeddedObjectContainer().getUserAllowsLinkUpdate())
            return SUCCESS;

        ScDocument& rDoc = mrDocShell.GetDocument();
        std::vector<sc::ExternalDataSource>& rSources
            = rDoc.GetExternalDataMapper().getDataSources();
        if (mnSourceIndex < rSources.size())
            rSources[mnSourceIndex].refresh(&rDoc, true);

        return SUCCESS;
    }
};

}

ExternalDataMapper::ExternalDataMapper(ScDocument& rDoc)
    : mrDoc(rDoc)
{
}

ExternalDataMapper::~ExternalDataMapper()
{
}

void ExternalDataMapper::insertDataSource(const sc::ExternalDataSource& rSource)
{
    maDataSources.push_back(rSource);

    ScDocShell* pDocShell = mrDoc.GetDocumentShell();
    if (!pDocShell)
        return;

    sfx2::LinkManager* pLinkManager = mrDoc.GetLinkManager();
    if (!pLinkManager)
        return;

    ScDataProviderLink* pLink = new ScDataProviderLink(*pDocShell, maDataSources.size() - 1);
    const OUString& rProvider = rSource.getProvider();
    pLinkManager->InsertFileLink(*pLink, sfx2::SvBaseLinkObjectType::ClientFile,
                                 rSource.getURL(), &rProvider, nullptr);
}

const std::vector<sc::ExternalDataSource>& ExternalDataMapper::getDataSources() const
{
    return maDataSources;
}

std::vector<sc::ExternalDataSource>& ExternalDataMapper::getDataSources()
{
    return maDataSources;
}

DataProvider::DataProvider(sc::ExternalDataSource& rDataSource):
    mbDeterministic(false),
    mrDataSource(rDataSource)
{
}

void DataProvider::setDeterministic()
{
    mbDeterministic = true;
}

DataProvider::~DataProvider()
{
}

void ScDBDataManager::WriteToDoc(ScDocument& rDoc)
{
    // first apply all data transformations

    bool bShrunk = false;
    SCCOL nStartCol = 0;
    SCROW nStartRow = 0;
    SCCOL nEndCol = rDoc.MaxCol();
    SCROW nEndRow = rDoc.MaxRow();
    rDoc.ShrinkToUsedDataArea(bShrunk, 0, nStartCol, nStartRow, nEndCol, nEndRow, false, true, true);
    ScRange aClipRange(nStartCol, nStartRow, 0, nEndCol, nEndRow, 0);
    rDoc.SetClipArea(aClipRange);

    ScRange aDestRange;
    getDBData()->GetArea(aDestRange);
    SCCOL nColSize = std::min<SCCOL>(aDestRange.aEnd.Col() - aDestRange.aStart.Col(), nEndCol);
    aDestRange.aEnd.SetCol(aDestRange.aStart.Col() + nColSize);

    SCROW nRowSize = std::min<SCROW>(aDestRange.aEnd.Row() - aDestRange.aStart.Row(), nEndRow);
    aDestRange.aEnd.SetRow(aDestRange.aStart.Row() + nRowSize);

    ScMarkData aMark(mpDoc->GetSheetLimits());
    aMark.SelectTable(aDestRange.aStart.Tab(), true);
    mpDoc->CopyFromClip(aDestRange, aMark, InsertDeleteFlags::CONTENTS, nullptr, &rDoc);
    ScDocShell* pDocShell = mpDoc->GetDocumentShell();
    if (pDocShell)
        pDocShell->PostPaint(aDestRange, PaintPartFlags::All);
}

ScDBDataManager::ScDBDataManager(OUString aDBName, ScDocument* pDoc):
    maDBName(std::move(aDBName)),
    mpDoc(pDoc)
{
}

ScDBDataManager::~ScDBDataManager()
{
}

void ScDBDataManager::SetDatabase(const OUString& rDBName)
{
    maDBName = rDBName;
}

ScDBData* ScDBDataManager::getDBData()
{
    ScDBData* pDBData = mpDoc->GetDBCollection()->getNamedDBs().findByUpperName(ScGlobal::getCharClass().uppercase(maDBName));
    return pDBData;
}

std::shared_ptr<DataProvider> DataProviderFactory::getDataProvider(ScDocument* pDoc,
        sc::ExternalDataSource& rDataSource)
{
    switch (getDataProviderKind(rDataSource.getProvider()))
    {
        case DataProviderKind::Csv:
            return std::make_shared<CSVDataProvider>(pDoc, rDataSource);
        case DataProviderKind::Html:
            return std::make_shared<HTMLDataProvider>(pDoc, rDataSource);
        case DataProviderKind::Xml:
            return std::make_shared<XMLDataProvider>(pDoc, rDataSource);
        case DataProviderKind::Sql:
            return std::make_shared<SQLDataProvider>(pDoc, rDataSource);
        case DataProviderKind::Unknown:
            SAL_WARN("sc", "no external data provider supported yet");
            return {};
    }
    return {};
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
