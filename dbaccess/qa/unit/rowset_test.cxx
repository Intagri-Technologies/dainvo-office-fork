/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "dbtest_base.cxx"

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/sdb/CommandType.hpp>
#include <com/sun/star/sdb/XOfficeDatabaseDocument.hpp>
#include <com/sun/star/sdb/XParametersSupplier.hpp>
#include <com/sun/star/sdbc/XParameters.hpp>
#include <com/sun/star/sdbc/XRowSet.hpp>
#include <com/sun/star/sdbcx/XTablesSupplier.hpp>

#include <com/sun/star/util/XRefreshable.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

class RowSetTest : public DBTestBase
{
};

CPPUNIT_TEST_FIXTURE(RowSetTest, testParameters)
{
    uno::Reference<XConnection> xConnection = setUpDBConnection();

    Reference<XStatement> xStatement = xConnection->createStatement();
    CPPUNIT_ASSERT(xStatement.is());
    xStatement->execute(u"DROP TABLE \"TEST1\" IF EXISTS"_ustr);
    xStatement->execute(
        u"CREATE TABLE \"TEST1\" (\"ID\" INTEGER NOT NULL PRIMARY KEY, \"col2\" VARCHAR(50))"_ustr);

    xConnection->prepareStatement(u"INSERT INTO \"TEST1\" VALUES (?,?)"_ustr);

    Reference<XPreparedStatement> xPrepared
        = xConnection->prepareStatement(u"INSERT INTO \"TEST1\" VALUES (?,?)"_ustr);
    Reference<XParameters> xParams(xPrepared, UNO_QUERY);
    constexpr int MAX_TABLE_ROWS = 100;
    for (int i = 0; i < MAX_TABLE_ROWS; ++i)
    {
        xParams->setInt(1, i);
        xParams->setString(2, u"Test"_ustr + OUString::number(i));
        xPrepared->executeUpdate();
    }

    uno::Reference<sdbcx::XTablesSupplier> xTablesSupplier(xConnection, UNO_QUERY_THROW);
    uno::Reference<util::XRefreshable> xTablesRefresh(xTablesSupplier->getTables(),
                                                      UNO_QUERY_THROW);
    xTablesRefresh->refresh();

    uno::Reference<XRowSet> xRowSet(
        getMultiServiceFactory()->createInstance(u"com.sun.star.sdb.RowSet"_ustr), UNO_QUERY);
    CPPUNIT_ASSERT(xRowSet.is());
    uno::Reference<XPropertySet> xRowSetProperties(xRowSet, UNO_QUERY);
    CPPUNIT_ASSERT(xRowSetProperties.is());

    // testTableParameters
    // for a row set simply based on a table, there should be not parameters at all
    xRowSetProperties->setPropertyValue(u"Command"_ustr, Any(u"PRODUCTS"_ustr));
    xRowSetProperties->setPropertyValue(u"CommandType"_ustr, Any(CommandType::TABLE));
    xRowSetProperties->setPropertyValue(u"ActiveConnection"_ustr, Any(xConnection));

    uno::Reference<XParametersSupplier> xParametersSupplier(xRowSet, UNO_QUERY);
    uno::Reference<container::XIndexAccess> xIndex = xParametersSupplier->getParameters();

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), xIndex->getCount());

    // testParametrizedQuery
    // for a row set based on a parametrized query, those parameters should be properly recognized
    uno::Reference<sdb::XOfficeDatabaseDocument> xDocument(mxComponent, UNO_QUERY_THROW);
    createQuery(u"SELECT * FROM \"PRODUCTS\" WHERE \"NAME\" LIKE :product_name"_ustr, true,
                u"products like"_ustr, xDocument->getDataSource());

    xRowSetProperties->setPropertyValue(u"Command"_ustr, Any(u"products like"_ustr));
    xRowSetProperties->setPropertyValue(u"CommandType"_ustr, Any(CommandType::QUERY));
    xRowSetProperties->setPropertyValue(u"ActiveConnection"_ustr, Any(xConnection));

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), xIndex->getCount());
    uno::Reference<beans::XPropertySet> xPropertySet(xIndex->getByIndex(0), UNO_QUERY_THROW);

    CPPUNIT_ASSERT_EQUAL(u"product_name"_ustr,
                         xPropertySet->getPropertyValue(u"Name"_ustr).get<OUString>());

    // testParametersInFilter
    xRowSetProperties->setPropertyValue(u"Command"_ustr, Any(u"SELECT * FROM \"CUSTOMERS\""_ustr));
    xRowSetProperties->setPropertyValue(u"CommandType"_ustr, Any(CommandType::COMMAND));
    xRowSetProperties->setPropertyValue(u"ActiveConnection"_ustr, Any(xConnection));

    xRowSetProperties->setPropertyValue(u"Filter"_ustr, Any(u"\"CITY\" = :city"_ustr));
    xRowSetProperties->setPropertyValue(u"ApplyFilter"_ustr, Any(true));

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), xIndex->getCount());
    xPropertySet.set(xIndex->getByIndex(0), UNO_QUERY_THROW);

    CPPUNIT_ASSERT_EQUAL(u"city"_ustr,
                         xPropertySet->getPropertyValue(u"Name"_ustr).get<OUString>());

    xRowSetProperties->setPropertyValue(u"ApplyFilter"_ustr, Any(false));

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), xIndex->getCount());

    // testParametersAfterNormalExecute
    xRowSetProperties->setPropertyValue(u"Command"_ustr, Any(u"SELECT * FROM \"CUSTOMERS\""_ustr));
    xRowSetProperties->setPropertyValue(u"CommandType"_ustr, Any(CommandType::COMMAND));
    xRowSetProperties->setPropertyValue(u"ActiveConnection"_ustr, Any(xConnection));

    xRowSet->execute();

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), xIndex->getCount());

    xRowSetProperties->setPropertyValue(
        u"Command"_ustr, Any(u"SELECT * FROM \"CUSTOMERS\" WHERE \"CITY\" = :city"_ustr));
    xParams.set(xRowSet, UNO_QUERY);
    xParams->setString(1, u"London"_ustr);

    xRowSet->execute();

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), xIndex->getCount());
    xPropertySet.set(xIndex->getByIndex(0), UNO_QUERY_THROW);

    CPPUNIT_ASSERT_EQUAL(u"city"_ustr,
                         xPropertySet->getPropertyValue(u"Name"_ustr).get<OUString>());
    CPPUNIT_ASSERT_EQUAL(u"London"_ustr,
                         xPropertySet->getPropertyValue(u"Value"_ustr).get<OUString>());

    //testParametersInteraction
    xRowSetProperties->setPropertyValue(u"Command"_ustr, Any(u"products like"_ustr));
    xRowSetProperties->setPropertyValue(u"CommandType"_ustr, Any(CommandType::QUERY));
    xRowSetProperties->setPropertyValue(u"ActiveConnection"_ustr, Any(xConnection));

    // let's fill in a parameter value via XParameters, and see whether it is respected by the parameters container
    xParams->setString(1, u"Apples"_ustr);

    xIndex.set(xParametersSupplier->getParameters());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), xIndex->getCount());
    xPropertySet.set(xIndex->getByIndex(0), UNO_QUERY_THROW);

    CPPUNIT_ASSERT_EQUAL(u"product_name"_ustr,
                         xPropertySet->getPropertyValue(u"Name"_ustr).get<OUString>());
    CPPUNIT_ASSERT_EQUAL(u"Apples"_ustr,
                         xPropertySet->getPropertyValue(u"Value"_ustr).get<OUString>());

    // let's fill in a parameter value via XParameters, and see whether it is respected by the parameters container
    xParams->setString(1, u"Oranges"_ustr);
    xRowSet->execute();

    CPPUNIT_ASSERT_EQUAL(u"product_name"_ustr,
                         xPropertySet->getPropertyValue(u"Name"_ustr).get<OUString>());
    CPPUNIT_ASSERT_EQUAL(u"Oranges"_ustr,
                         xPropertySet->getPropertyValue(u"Value"_ustr).get<OUString>());
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
