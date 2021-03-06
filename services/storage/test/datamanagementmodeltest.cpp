/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011-13 Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "datamanagementmodeltest.h"
#include "../datamanagementmodel.h"
#include "../classandpropertytree.h"
#include "../virtuosoinferencemodel.h"
#include "simpleresource.h"
#include "simpleresourcegraph.h"

#include <QtTest>
#include "qtest_kde.h"
#include "qtest_dms.h"

#include <Soprano/Soprano>
#include <Soprano/Graph>
#define USING_SOPRANO_NRLMODEL_UNSTABLE_API
#include <Soprano/NRLModel>

#include <KTemporaryFile>
#include <KTempDir>
#include <KProtocolInfo>
#include <KDebug>

#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"
#include "resourcemanager.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;
using namespace Nepomuk2::Vocabulary;


// TODO: test nao:created and nao:lastModified, these should always be correct for existing resources. This is especially important in the removeDataByApplication methods.

void DataManagementModelTest::resetModel()
{
    // remove all the junk from previous tests
    m_model->removeAllStatements();

    // add some classes and properties
    QUrl graph("graph:/onto");
    Nepomuk2::insertOntologies( m_model, graph );

    // rebuild the internals of the data management model
    m_classAndPropertyTree->rebuildTree(m_dmModel);
    m_inferenceModel->updateOntologyGraphs(true);
    m_dmModel->clearCache();
}


void DataManagementModelTest::initTestCase()
{
    const Soprano::Backend* backend = Soprano::PluginManager::instance()->discoverBackendByName( "virtuosobackend" );
    QVERIFY( backend );
    m_storageDir = new KTempDir();

    Soprano::BackendSettings settings;
    settings << Soprano::BackendSetting( "noStatementSignals", true );
    settings << Soprano::BackendSetting( "fakeBooleans", false );
    settings << Soprano::BackendSetting( "emptyGraphs", false );
    settings << Soprano::BackendSetting( Soprano::BackendOptionStorageDir, m_storageDir->name() );

    m_model = backend->createModel( settings );
    QVERIFY( m_model );

    // DataManagementModel relies on the usage of a NRLModel in the storage service
    m_nrlModel = new Soprano::NRLModel(m_model);
    Nepomuk2::insertNamespaceAbbreviations( m_model );

    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);
    m_inferenceModel = new Nepomuk2::VirtuosoInferenceModel(m_nrlModel);
    m_dmModel = new Nepomuk2::DataManagementModel(m_classAndPropertyTree, m_inferenceModel);
}

void DataManagementModelTest::cleanupTestCase()
{
    delete m_dmModel;
    delete m_inferenceModel;
    delete m_nrlModel;
    delete m_model;
    delete m_storageDir;
    delete m_classAndPropertyTree;
}

void DataManagementModelTest::init()
{
    resetModel();
}


void DataManagementModelTest::testAddProperty()
{
    // remember graph count
    const int initialGraphCount = m_model->listContexts().allElements().count();

    // we start by simply adding a property
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("foobar")), QLatin1String("Testapp"));

    QVERIFY(!m_dmModel->lastError());

    // check that the actual data is there
    QVERIFY(m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    // check that the app resource has been created with its corresponding graphs
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { ?r a %1 . ?r %2 %3 . } . "
                                                      "graph ?mg { ?g a %4 . ?mg a %5 . ?mg %6 ?g . } . }")
                                  .arg(Soprano::Node::resourceToN3(NAO::Agent()),
                                       Soprano::Node::resourceToN3(NAO::identifier()),
                                       Soprano::Node::literalToN3(QLatin1String("Testapp")),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // check that we have an InstanceBase with a GraphMetadata graph
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { <nepomuk:/res/A> <prop:/string> %1 . } . "
                                                      "graph ?mg { ?g a %2 . ?mg a %3 . ?mg %4 ?g . } . "
                                                      "}")
                                  .arg(Soprano::Node::literalToN3(QLatin1String("foobar")),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // extra graphs = 2 for TestApp
    QCOMPARE(m_model->listContexts().allElements().count(), initialGraphCount + 2);

    //
    // add another property value on top of the existing one
    //
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("hello world")), QLatin1String("Testapp"));

    // verify the values
    QCOMPARE(m_model->listStatements(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), Node()).allStatements().count(), 2);
    QVERIFY(m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testAddProperty_double()
{
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("hello world")), QLatin1String("Testapp"));

    Soprano::Graph existingStatements = m_model->listStatements().allStatements();

    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("hello world")), QLatin1String("Testapp"));

    // nothing should have changed
    // apart from the nao:lastModified
    QSet<Soprano::Statement> newSet = m_model->listStatements().allStatements().toSet();
    QSet<Soprano::Statement> existingSet = existingStatements.toSet();
    foreach(const Soprano::Statement& st, newSet) {
        if( st.predicate() != NAO::lastModified() )
            QVERIFY(existingSet.contains(st));
    }

    foreach(const Soprano::Statement& st, existingSet) {
        if( st.predicate() != NAO::lastModified() )
            QVERIFY(newSet.contains(st));
    }

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testAddProperty_diffApp()
{
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("hello world")), QLatin1String("Testapp"));

    Soprano::Graph existingStatements = m_model->listStatements().allStatements();

    // rewrite the same property with another app
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("hello world")), QLatin1String("Otherapp"));

    // there should only be the new app, nothing else
    // thus, all previous statements need to be there except for the NAO::lastModified which would have changed
    foreach(const Statement& s, existingStatements.toList()) {
        if( s.predicate() != NAO::lastModified() )
            QVERIFY(m_model->containsStatement(s));
    }


    // Check if the new app exists
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { ?r a %1 . ?r %2 %3 . } . "
                                                      "graph ?mg { ?g a %4 . ?mg a %5 . ?mg %6 ?g . } . }")
                                  .arg(Soprano::Node::resourceToN3(NAO::Agent()),
                                       Soprano::Node::resourceToN3(NAO::identifier()),
                                       Soprano::Node::literalToN3(QLatin1String("Otherapp")),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // Check if the data exists twice if 2 different graphs
    QList<Node> graphList = m_model->listStatements(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), LiteralValue("hello world")).iterateContexts().allNodes();
    QCOMPARE( graphList.size(), 2 );

    QList<Node> app1NodeList = m_model->listStatements( graphList.first(), NAO::maintainedBy(), QUrl() ).iterateObjects().allNodes();
    QList<Node> app2NodeList = m_model->listStatements( graphList.last(), NAO::maintainedBy(), QUrl() ).iterateObjects().allNodes();

    QCOMPARE(app1NodeList.size(), 1);
    QCOMPARE(app2NodeList.size(), 1);

    const QUrl app1 = app1NodeList.first().uri();
    const QUrl app2 = app2NodeList.first().uri();

    QVERIFY( app1 != app2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testAddProperty_cardinality()
{
    // adding the same value twice in one call should result in one insert. This also includes the cardinality check
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/AA"), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl("nepomuk:/res/B")) << QVariant(QUrl("nepomuk:/res/B")), QLatin1String("Testapp"));
    QVERIFY(!m_dmModel->lastError());
    QCOMPARE(m_model->listStatements(QUrl("nepomuk:/res/AA"), QUrl("prop:/res_c1"), QUrl("nepomuk:/res/B")).allStatements().count(), 1);

    // we now add two values for a property with cardinality 1
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl("nepomuk:/res/B")) << QVariant(QUrl("nepomuk:/res/C")), QLatin1String("Testapp"));
    QVERIFY(m_dmModel->lastError());

    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl("nepomuk:/res/B")), QLatin1String("Testapp"));
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl("nepomuk:/res/C")), QLatin1String("Testapp"));

    // the second call needs to fail
    QVERIFY(m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testAddProperty_file()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();
    QTemporaryFile fileC;
    fileC.open();

    m_dmModel->addProperty(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("foobar")), QLatin1String("Testapp"));

    // make sure the nie:url relation has been created
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())));
    QVERIFY(!m_model->containsAnyStatement(QUrl::fromLocalFile(fileA.fileName()), Node(), Node()));

    // get the resource uri
    const QUrl fileAResUri = m_model->listStatements(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())).allStatements().first().subject().uri();

    // make sure the resource is a file
    QVERIFY(m_model->containsAnyStatement(fileAResUri, RDF::type(), NFO::FileDataObject()));

    // make sure the actual value is there
    QVERIFY(m_model->containsAnyStatement(fileAResUri, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));


    // add relation from file to file
    m_dmModel->addProperty(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/res"), QVariantList() << QVariant(QUrl::fromLocalFile(fileB.fileName())), QLatin1String("Testapp"));

    // make sure the nie:url relation has been created
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), QUrl::fromLocalFile(fileB.fileName())));
    QVERIFY(!m_model->containsAnyStatement(QUrl::fromLocalFile(fileB.fileName()), Node(), Node()));

    // get the resource uri
    const QUrl fileBResUri = m_model->listStatements(Node(), NIE::url(), QUrl::fromLocalFile(fileB.fileName())).allStatements().first().subject().uri();

    // make sure the resource is a file
    QVERIFY(m_model->containsAnyStatement(fileBResUri, RDF::type(), NFO::FileDataObject()));

    // make sure the actual value is there
    QVERIFY(m_model->containsAnyStatement(fileAResUri, QUrl("prop:/res"), fileBResUri));

    // we now add two values for a property with cardinality 1
    m_dmModel->addProperty(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl::fromLocalFile(fileB.fileName())), QLatin1String("Testapp"));
    m_dmModel->addProperty(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/res_c1"), QVariantList() << QVariant(QUrl::fromLocalFile(fileC.fileName())), QLatin1String("Testapp"));

    // the second call needs to fail
    QVERIFY(m_dmModel->lastError());


    // test adding a property to both the file and the resource URI. The result should be the exact same as doing it with only one of them
    m_dmModel->addProperty(QList<QUrl>() << fileAResUri << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("Whatever")), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(fileAResUri, QUrl("prop:/string"), LiteralValue(QLatin1String("Whatever"))).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())).allStatements().count(), 1);

    // test the same with the file as object
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res"), QVariantList() << QVariant(KUrl(fileA.fileName())) << QVariant(fileAResUri), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(QUrl("nepomuk:/res/A"), QUrl("prop:/res"), fileAResUri).allStatements().count(), 1);
    QVERIFY(!m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/res"), QUrl::fromLocalFile(fileA.fileName())));
    QCOMPARE(m_model->listStatements(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())).allStatements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testAddProperty_invalidFile()
{
    KTemporaryFile f1;
    QVERIFY( f1.open() );
    QUrl f1Url( f1.fileName() );
    //f1Url.setScheme("file");

    m_dmModel->addProperty( QList<QUrl>() << f1Url, RDF::type(), QVariantList() << NAO::Tag(), QLatin1String("testapp") );

    // There should be some error that '' protocol doesn't exist
    QVERIFY(m_dmModel->lastError());

    // The support for plain file paths is in the DBus adaptor through the usage of KUrl. If
    // local path support is neccesary on the level of the model, simply use KUrl which
    // will automatically add the file:/ protocol to local paths.
    QVERIFY( !m_model->containsAnyStatement( Node(), NIE::url(), f1Url ) );

    m_dmModel->addProperty( QList<QUrl>() << QUrl("file:///Blah"), QUrl("prop:/string"),
                            QVariantList() << "Comment", QLatin1String("testapp") );

    // There should be some error as '/Blah' does not exist
    QVERIFY(m_dmModel->lastError());

    // A new empty graph for the testapp would have been constructed

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testAddProperty_invalid_args()
{
    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resource list
    m_dmModel->addProperty(QList<QUrl>(), QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty property uri
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), QUrl(), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty value list
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int"), QVariantList(), QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int"), QVariantList() << 42, QString());

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid range
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int"), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected properties 1
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), NAO::created(), QVariantList() << QDateTime::currentDateTime(), QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected properties 2
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), NAO::lastModified(), QVariantList() << QDateTime::currentDateTime(), QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // make sure we cannot add anything to non-existing files
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    m_dmModel->addProperty(QList<QUrl>() << nonExistingFileUrl, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nonExistingFileUrl should not exist
    QVERIFY( !m_model->containsAnyStatement(nonExistingFileUrl, QUrl(), QUrl()) );
    QVERIFY( !m_model->containsAnyStatement(QUrl(), QUrl(), nonExistingFileUrl) );

    // non-existing file as object
    m_dmModel->addProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/res"), QVariantList() << nonExistingFileUrl, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nonExistingFileUrl should not exist
    QVERIFY( !m_model->containsAnyStatement(nonExistingFileUrl, QUrl(), QUrl()) );
    QVERIFY( !m_model->containsAnyStatement(QUrl(), QUrl(), nonExistingFileUrl) );

    // TODO: try setting protected properties like nie:url, nfo:fileName, nie:isPartOf (only applies to files)
}


void DataManagementModelTest::testAddProperty_akonadi()
{
    // create our app
    const QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl nepomukG = m_dmModel->nepomukGraph();

    QUrl resA("nepomuk:/res/A");
    QUrl akonadiUrl("akonadi:id=5");
    m_model->addStatement( resA, RDF::type(), NIE::DataObject(), g1 );
    m_model->addStatement( resA, NIE::url(), akonadiUrl, nepomukG );

    // add a property using the akonadi URL
    // the tricky thing here is that nao:identifier does not have a range!
    m_dmModel->addProperty( QList<QUrl>() << akonadiUrl,
                            NAO::identifier(),
                            QVariantList() << QString("akon"),
                            QLatin1String("AppA") );

    QVERIFY(!m_dmModel->lastError());

    // check that the akonadi URL has been resolved to the resource URI
    QVERIFY(m_model->containsAnyStatement( resA, NAO::identifier(), Soprano::Node() ));

    // check that the property has the desired value
    QVERIFY(m_model->containsAnyStatement( resA, NAO::identifier(), LiteralValue("akon") ));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty()
{
    // remember graph count
    const int initialGraphCount = m_model->listContexts().allElements().count();

    // adding the most basic property
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("foobar")), QLatin1String("Testapp"));

    QVERIFY(!m_dmModel->lastError());

    // check that the actual data is there
    QVERIFY(m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    // check that the app resource has been created with its corresponding graphs
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { ?r a %1 . ?r %2 %3 . } . "
                                                      "graph ?mg { ?g a %4 . ?mg a %5 . ?mg %6 ?g . } . }")
                                  .arg(Soprano::Node::resourceToN3(NAO::Agent()),
                                       Soprano::Node::resourceToN3(NAO::identifier()),
                                       Soprano::Node::literalToN3(QLatin1String("Testapp")),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // check that we have an InstanceBase with a GraphMetadata graph
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { <nepomuk:/res/A> <prop:/string> %1 . } . "
                                                      "graph ?mg { ?g a %2 . ?mg a %3 . ?mg %4 ?g . } . "
                                                      "}")
                                  .arg(Soprano::Node::literalToN3(QLatin1String("foobar")),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // 2 extra graphs for the TestApp
    QCOMPARE(m_model->listContexts().allElements().count(), initialGraphCount + 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty_overwrite()
{
    // create an app graph
    const QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // add a resource with 2 properties
    QUrl mg;
    const QUrl g = m_nrlModel->createGraph(NRL::InstanceBase(), &mg);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);

    m_model->addStatement(g, NAO::maintainedBy(), QUrl("app:/A"), mg);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/A"), mg2);

    m_model->addStatement(QUrl("res:/A"), RDF::type(), NAO::Tag(), g);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42), g);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int3"), LiteralValue(42), g2);



    //
    // now overwrite the one property
    //
    m_dmModel->setProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int"), QVariantList() << 12, QLatin1String("testapp"));

    // now the model should have replaced the old value and added the new value in a new graph
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(12)));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42)));

    // a new graph
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(12)).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42)).allStatements().count(), 1);
    QVERIFY(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(12)).allStatements().first().context().uri() != g);
    QVERIFY(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42)).allStatements().first().context().uri() == g);

    // the testapp Agent as maintainer of the new graph
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { <res:/A> <prop:/int> %1 . } . "
                                                      "graph ?mg { ?g a %2 . ?mg a %3 . ?mg %4 ?g . } . "
                                                      "?g %5 ?a . ?a %6 %7 . "
                                                      "}")
                                  .arg(Soprano::Node::literalToN3(12),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor()),
                                       Soprano::Node::resourceToN3(NAO::maintainedBy()),
                                       Soprano::Node::resourceToN3(NAO::identifier()),
                                       Soprano::Node::literalToN3(QLatin1String("testapp"))),
                                  Soprano::Query::QueryLanguageSparql).boolValue());



    //
    // Rewrite the same value with a different app
    //
    m_dmModel->setProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int2"), QVariantList() << 42, QLatin1String("testapp"));

    QList<Node> graphList = m_model->listStatements(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42)).iterateContexts().allNodes();

    // the value should be there twice
    QCOMPARE(graphList.size(), 2);

    // Each graph should be maintained by different applications
    QList<Node> app1NodeList = m_model->listStatements( graphList.first(), NAO::maintainedBy(), QUrl() ).iterateObjects().allNodes();
    QList<Node> app2NodeList = m_model->listStatements( graphList.last(), NAO::maintainedBy(), QUrl() ).iterateObjects().allNodes();

    QCOMPARE(app1NodeList.size(), 1);
    QCOMPARE(app2NodeList.size(), 1);

    const QUrl app1 = app1NodeList.first().uri();
    const QUrl app2 = app2NodeList.first().uri();

    QVERIFY( app1 != app2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty_invalid_args()
{
    // Use setProperty so that testapp gets created.
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/testRes"), QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();

    // empty resource list
    m_dmModel->setProperty(QList<QUrl>(), QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty property uri
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl(), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty value list
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/int"), QVariantList(), QLatin1String("testapp"));

    // the call should NOT have failed
    QVERIFY(!m_dmModel->lastError());

    // but nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/int"), QVariantList() << 42, QString());

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid range
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/int"), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // make sure we cannot add anything to non-existing files
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    m_dmModel->setProperty(QList<QUrl>() << nonExistingFileUrl, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // non-existing file as object
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res"), QVariantList() << nonExistingFileUrl, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}

void DataManagementModelTest::testSetProperty_nieUrl1()
{
    QUrl uri = m_dmModel->createResource( QList<QUrl>() << NFO::FileDataObject(), QString(), QString(), QString("A") );
    QVERIFY(!m_dmModel->lastError());

    m_dmModel->setProperty(QList<QUrl>() << uri, NIE::url(), QVariantList() << QUrl("file:///tmp/A"), QLatin1String("testapp"));
    QVERIFY(!m_dmModel->lastError());

    QVERIFY(m_model->containsAnyStatement(uri, NIE::url(), QUrl("file:///tmp/A")));
    QVERIFY(m_model->containsAnyStatement(uri, RDF::type(), NFO::FileDataObject()));

    const QUrl nieUrlGraph = m_model->listStatements(uri, NIE::url(), QUrl("file:///tmp/A")).allStatements().first().context().uri();

    // the nie:url should always be in the nepomuk graph
    QCOMPARE( nieUrlGraph, m_dmModel->nepomukGraph() );

    // we reset the URL
    m_dmModel->setProperty(QList<QUrl>() << uri, NIE::url(), QVariantList() << QUrl("file:///tmp/B"), QLatin1String("testapp"));

    // the url should have changed
    QVERIFY(!m_model->containsAnyStatement(uri, NIE::url(), QUrl("file:///tmp/A")));
    QVERIFY(m_model->containsAnyStatement(uri, NIE::url(), QUrl("file:///tmp/B")));

    // the graph should have been kept
    QCOMPARE(m_model->listStatements(uri, NIE::url(), Node()).allStatements().first().context().uri(), nieUrlGraph);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty_nieUrl2()
{
    KTempDir* dir = createNieUrlTestData();

    // change the nie:url of one of the top level dirs
    const QUrl newDir1Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1-new"));

    // we first need to move the file, otherwise the file check in the dms kicks in
    QVERIFY(QFile::rename(dir->name() + QLatin1String("dir1"), newDir1Url.toLocalFile()));

    // now update the database
    m_dmModel->setProperty(QList<QUrl>() << QUrl("res:/dir1"), NIE::url(), QVariantList() << newDir1Url, QLatin1String("testapp"));

    // this should have updated the nie:urls of all children, too
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir1"), NIE::url(), newDir1Url));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir1"), NFO::fileName(), LiteralValue(QLatin1String("dir1-new"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir11"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir11"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir12"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir12"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir13"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir13"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/file11"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/file11"))));

    delete dir;

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// the same test as above only using the file URL
void DataManagementModelTest::testSetProperty_nieUrl3()
{
    KTempDir* dir = createNieUrlTestData();

    // change the nie:url of one of the top level dirs
    const QUrl oldDir1Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1"));
    const QUrl newDir1Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1-new"));

    // we first need to move the file, otherwise the file check in the dms kicks in
    QVERIFY(QFile::rename(oldDir1Url.toLocalFile(), newDir1Url.toLocalFile()));

    // now update the database
    m_dmModel->setProperty(QList<QUrl>() << oldDir1Url, NIE::url(), QVariantList() << newDir1Url, QLatin1String("testapp"));

    // this should have updated the nie:urls of all children, too
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir1"), NIE::url(), newDir1Url));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir1"), NFO::fileName(), LiteralValue(QLatin1String("dir1-new"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir11"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir11"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir12"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir12"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir13"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/dir13"))));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/file11"), NIE::url(), QUrl(newDir1Url.toString() + QLatin1String("/file11"))));

    delete dir;

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty_nieUrl4()
{
    KTempDir* dir = createNieUrlTestData();

    // move one of the dirs to a new parent
    const QUrl oldDir121Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1/dir12/dir121"));
    const QUrl newDir121Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1/dir12/dir121-new"));

    // we first need to move the file, otherwise the file check in the dms kicks in
    QVERIFY(QFile::rename(oldDir121Url.toLocalFile(), newDir121Url.toLocalFile()));

    // now update the database
    m_dmModel->setProperty(QList<QUrl>() << QUrl("res:/dir121"), NIE::url(), QVariantList() << newDir121Url, QLatin1String("testapp"));

    // the url
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir121"), NIE::url(), newDir121Url));

    // the child file
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/file1211"), NIE::url(), QUrl(newDir121Url.toString() + QLatin1String("/file1211"))));

    // the nie:isPartOf relationship should have been updated, too
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir121"), NIE::isPartOf(), QUrl("res:/dir12")));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// the same test as above only using the file URL
void DataManagementModelTest::testSetProperty_nieUrl5()
{
    KTempDir* dir = createNieUrlTestData();

    // move one of the dirs to a new parent
    const QUrl oldDir121Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir1/dir12/dir121"));
    const QUrl newDir121Url = QUrl(QLatin1String("file://") + dir->name() + QLatin1String("dir2/dir121"));

    // we first need to move the file, otherwise the file check in the dms kicks in
    QVERIFY(QFile::rename(oldDir121Url.toLocalFile(), newDir121Url.toLocalFile()));

    // now update the database
    m_dmModel->setProperty(QList<QUrl>() << oldDir121Url, NIE::url(), QVariantList() << newDir121Url, QLatin1String("testapp"));

    // the url
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir121"), NIE::url(), newDir121Url));

    // the child file
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/file1211"), NIE::url(), QUrl(newDir121Url.toString() + QLatin1String("/file1211"))));

    // the nie:isPartOf relationship should have been updated, too
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/dir121"), NIE::isPartOf(), QUrl("res:/dir2")));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test support for any other URL scheme which already exists as nie:url (This is what libnepomuk does support)
void DataManagementModelTest::testSetProperty_nieUrl6()
{
    // create a resource that has a URL
    const QUrl url("http://nepomuk.kde.org/");

    m_model->addStatement(QUrl("res:/A"), NIE::url(), url, m_dmModel->nepomukGraph());


    // use the url to set a property
    m_dmModel->setProperty(QList<QUrl>() << url, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));

    // check that the property has been added to the resource
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42)));

    // check that no new resource has been created
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/int"), LiteralValue(42)).allElements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


// make sure we reuse legacy resource URIs
void DataManagementModelTest::testSetProperty_legacyData()
{
    // create some legacy data
    QTemporaryFile file;
    file.open();
    const KUrl url(file.fileName());

    const QUrl g = m_nrlModel->createGraph(NRL::InstanceBase());

    m_model->addStatement(url, QUrl("prop:/int"), LiteralValue(42), g);

    // set some data with the url
    m_dmModel->setProperty(QList<QUrl>() << url, QUrl("prop:/int"), QVariantList() << 2, QLatin1String("A"));

    // make sure the resource has changed
    QCOMPARE(m_model->listStatements(url, QUrl("prop:/int"), Node()).allElements().count(), 1);
    QCOMPARE(m_model->listStatements(url, QUrl("prop:/int"), Node()).allElements().first().object().literal(), LiteralValue(2));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testSetProperty_file()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();
    QTemporaryFile fileC;
    fileC.open();

    QUrl fileAUrl = QUrl::fromLocalFile(fileA.fileName());
    QUrl fileBUrl = QUrl::fromLocalFile(fileB.fileName());
    QUrl fileCUrl = QUrl::fromLocalFile(fileC.fileName());

    m_dmModel->setProperty(QList<QUrl>() << fileAUrl, QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("foobar")), QLatin1String("Testapp"));

    // make sure the nie:url relation has been created
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())));
    QVERIFY(!m_model->containsAnyStatement(fileAUrl, Node(), Node()));

    // get the resource uri
    const QUrl fileAResUri = m_model->listStatements(Node(), NIE::url(), fileAUrl).allStatements().first().subject().uri();

    // make sure the resource is a file
    QVERIFY(m_model->containsAnyStatement(fileAResUri, RDF::type(), NFO::FileDataObject()));

    // make sure the actual value is there
    QVERIFY(m_model->containsAnyStatement(fileAResUri, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    // add relation from file to file
    m_dmModel->setProperty(QList<QUrl>() << fileAUrl, QUrl("prop:/res"), QVariantList() << QVariant(fileBUrl), QLatin1String("Testapp"));

    // make sure the nie:url relation has been created
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), fileBUrl));
    QVERIFY(!m_model->containsAnyStatement(fileBUrl, Node(), Node()));

    // get the resource uri
    const QUrl fileBResUri = m_model->listStatements(Node(), NIE::url(), fileBUrl).allStatements().first().subject().uri();

    // make sure the resource is a file
    QVERIFY(m_model->containsAnyStatement(fileBResUri, RDF::type(), NFO::FileDataObject()));

    // make sure the actual value is there
    QVERIFY(m_model->containsAnyStatement(fileAResUri, QUrl("prop:/res"), fileBResUri));

    // test adding a property to both the file and the resource URI. The result should be the exact same as doing it with only one of them
    m_dmModel->setProperty(QList<QUrl>() << fileAResUri << fileAUrl, QUrl("prop:/string"), QVariantList() << QVariant(QLatin1String("Whatever")), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(fileAResUri, QUrl("prop:/string"), LiteralValue(QLatin1String("Whatever"))).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(Node(), NIE::url(), fileAUrl).allStatements().count(), 1);

    // test the same with the file as object
    m_dmModel->setProperty(QList<QUrl>() << QUrl("nepomuk:/res/A"), QUrl("prop:/res"), QVariantList() << QVariant(fileAUrl) << QVariant(fileAResUri), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(QUrl("nepomuk:/res/A"), QUrl("prop:/res"), fileAResUri).allStatements().count(), 1);
    QVERIFY(!m_model->containsAnyStatement(QUrl("nepomuk:/res/A"), QUrl("prop:/res"), fileAUrl));
    QCOMPARE(m_model->listStatements(Node(), NIE::url(), fileAUrl).allStatements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testRemoveProperty()
{
    const int cleanCount = m_model->statementCount();

    QUrl resA("nepomuk:/res/A");
    QList<QUrl> resAList;
    resAList << resA;
    m_dmModel->addProperty( resAList, QUrl("prop:/string"), QVariantList() << QString("foobar"), "Testapp" );
    m_dmModel->addProperty( resAList, QUrl("prop:/string"), QVariantList() << QString("hello world"), "Testapp" );


    QList<Node> lastModNodes = m_model->listStatements(resA, NAO::lastModified(), QUrl()).iterateObjects().allNodes();
    QCOMPARE( lastModNodes.size(), 1 );
    QDateTime lastMod = lastModNodes.first().literal().toDateTime();

    m_dmModel->removeProperty( resAList, QUrl("prop:/string"), QVariantList() << QLatin1String("hello world"), QLatin1String("Testapp"));

    QVERIFY(!m_dmModel->lastError());

    // test that the data has been removed
    QVERIFY(!m_model->containsAnyStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("hello world"))));

    // test that the mtime has been updated
    QList<Node> newLastModNodes = m_model->listStatements(resA, NAO::lastModified(), QUrl()).iterateObjects().allNodes();
    QCOMPARE( newLastModNodes.size(), 1 );
    QDateTime newLastMod = newLastModNodes.first().literal().toDateTime();
    QVERIFY( newLastMod > lastMod );

    // test that the other property value is still valid
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());

    // step 2: remove the second value
    m_dmModel->removeProperty(resAList, QUrl("prop:/string"), QVariantList() << QLatin1String("foobar"), QLatin1String("Testapp"));

    QVERIFY(!m_dmModel->lastError());

    // the property should be gone entirely
    QVERIFY(!m_model->containsAnyStatement(resA, QUrl("prop:/string"), Soprano::Node()));

    // even the resource should be gone since the NAO mtime does not count as a "real" property
    QVERIFY(!m_model->containsAnyStatement(resA, Soprano::Node(), Soprano::Node()));

    // nothing except the ontology and the Testapp Agent should be left
    // The +7 is for - rdf:type Agent, rdf:type GraphMetata, rdf:type InstanceBase
    //               - identifier "TestApp", created, coreGraphMetadataFor, maintainedBy
    QCOMPARE(m_model->statementCount(), cleanCount+7);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveProperty_file()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();

    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    const QUrl ng = m_dmModel->nepomukGraph();

    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), ng);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    m_model->addStatement(QUrl("res:/B"), NIE::url(), QUrl::fromLocalFile(fileB.fileName()), ng);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);



    // now we remove one value via the file URL
    m_dmModel->removeProperty(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/string"), QVariantList() << QLatin1String("hello world"), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/string"), Node()).allStatements().count(), 2);
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), NIE::url(), Node()).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName())).allStatements().count(), 1);


    // test the same with a file URL value
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/res"), QVariantList() << QUrl::fromLocalFile(fileB.fileName()), QLatin1String("Testapp"));

    QCOMPARE(m_model->listStatements(QUrl("res:/A"), QUrl("prop:/res"), Node()).allStatements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveProperty_invalid_args()
{
    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), g1);

    // Using add property so that the graph and app are created
    m_dmModel->addProperty(QList<QUrl>() << QUrl("nepomuk:/blankres"), QUrl("prop:/string"), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resource list
    m_dmModel->removeProperty(QList<QUrl>(), QUrl("prop:/string"), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // resource list with empty URL
    m_dmModel->removeProperty(QList<QUrl>() << QUrl() << QUrl("res:/A"), QUrl("prop:/string"), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty property
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl(), QVariantList() << QLatin1String("foobar"), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty values
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/string"), QVariantList(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/string"), QVariantList() << QLatin1String("foobar"), QString());

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid value type
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/int"), QVariantList() << QLatin1String("foobar"), QString("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected property 1
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), NAO::created(), QVariantList() << QDateTime::currentDateTime(), QString("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected property 2
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), NAO::lastModified(), QVariantList() << QDateTime::currentDateTime(), QString("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // make sure we cannot add anything to non-existing files
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    m_dmModel->removeProperty(QList<QUrl>() << nonExistingFileUrl, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("testapp"));

    // the call should silently fail
    QVERIFY(!m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // non-existing file as object
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), QUrl("prop:/res"), QVariantList() << nonExistingFileUrl, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}


// make sure that sub-resources are removed as soon as nothing related to them anymore, ie. as soon as the nao:hasSubResource relation is removed
void DataManagementModelTest::testRemoveProperty_subResource()
{
    QEXPECT_FAIL("", "No sub-resource handling in removeProperty yet.", Abort);

    // we create two resources, one being marked as sub-resource of the other
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    // now we remove the sub-resource property
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), NAO::hasSubResource(), QVariantList() << QUrl("res:/B"), QLatin1String("A"));

    // the sub-resource should have been removed, too. The reason is simple: a sub-resource does not make sense without its super-resource
    // and with the relation being gone there is no super-resource anymore.
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that sub-resources are not removed as long as they are still sub-resource to something else
void DataManagementModelTest::testRemoveProperty_subResource2()
{
    QEXPECT_FAIL("", "No sub-resource handling in removeProperty yet.", Abort);

    // we create two resources, one being marked as sub-resource of the other
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/B"), g1);

    // now we remove the sub-resource property
    m_dmModel->removeProperty(QList<QUrl>() << QUrl("res:/A"), NAO::hasSubResource(), QVariantList() << QUrl("res:/B"), QLatin1String("A"));

    // The sub-resource should still be there
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/C")));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveProperties()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();

    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    const QUrl ng = m_dmModel->nepomukGraph();

    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), ng);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(6), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/B"), NIE::url(), QUrl::fromLocalFile(fileB.fileName()), ng);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);


    // test removing one property from one resource
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << QUrl("prop:/string"), QLatin1String("testapp"));

    // check that all values are gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/string"), Node()));

    // check that all other values from that prop are still there
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/string"), Node()).allStatements().count(), 3);
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), QUrl("prop:/string"), Node()).allStatements().count(), 3);


    // test removing a property from more than one resource
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/B"), QList<QUrl>() << QUrl("prop:/int"), QLatin1String("testapp"));

    // check that all values are gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/int"), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/int"), Node()));

    // check that other properties from res:/B are still there
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), QUrl("prop:/string"), Node()).allStatements().count(), 3);


    // test file URLs in the resources
    m_dmModel->removeProperties(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), QList<QUrl>() << QUrl("prop:/int2"), QLatin1String("testapp"));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/int2"), Node()));

    // TODO: verify graphs

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testRemoveProperties_invalid_args()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();

    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);

    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int2"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(6), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(12), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(2), g1);

    m_model->addStatement(QUrl("res:/B"), NIE::url(), QUrl::fromLocalFile(fileB.fileName()), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);


    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resource list
    m_dmModel->removeProperties(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/string"), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // resource list with empty url
    m_dmModel->removeProperties(QList<QUrl>() << QUrl() << QUrl("res:/A"), QList<QUrl>() << QUrl("prop:/string"), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty property list
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // property list with empty url
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << QUrl("prop:/string") << QUrl(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << QUrl("prop:/string"), QString());

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected property 1
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << NAO::created(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // protected property 2
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << NAO::lastModified(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // make sure we cannot add anything to non-existing files
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    m_dmModel->removeProperties(QList<QUrl>() << nonExistingFileUrl, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("testapp"));

    // the call should have silently failed
    QVERIFY(!m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}


// make sure that sub-resources are removed as soon as nothing related to them anymore, ie. as soon as the nao:hasSubResource relation is removed
void DataManagementModelTest::testRemoveProperties_subResource()
{
    QEXPECT_FAIL("", "No sub-resource handling in removeProperties yet.", Abort);

    // we create two resources, one being marked as sub-resource of the other
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    // now we remove the sub-resource property
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << NAO::hasSubResource(), QLatin1String("A"));

    // the sub-resource should have been removed, too. The reason is simple: a sub-resource does not make sense without its super-resource
    // and with the relation being gone there is no super-resource anymore.
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that sub-resources are not removed as long as they are still sub-resource to something else
void DataManagementModelTest::testRemoveProperties_subResource2()
{
    QEXPECT_FAIL("", "No sub-resource handling in removeProperties yet.", Abort);

    // we create two resources, one being marked as sub-resource of the other
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/B"), g1);

    // now we remove the sub-resource property
    m_dmModel->removeProperties(QList<QUrl>() << QUrl("res:/A"), QList<QUrl>() << NAO::hasSubResource(), QLatin1String("A"));

    // The sub-resource should still be there
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/C")));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveResources()
{
    QTemporaryFile fileA;
    fileA.open();
    QTemporaryFile fileB;
    fileB.open();

    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);

    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);

    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(6), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(12), g2);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/int"), LiteralValue(2), g2);
    m_model->addStatement(QUrl("res:/B"), NIE::url(), QUrl::fromLocalFile(fileB.fileName()), g2);


    m_dmModel->removeResources(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("testapp"));

    // verify that the resource is gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));

    // verify that other resources were not touched
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), Node(), Node()).allStatements().count(), 4);
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), Node(), Node()).allStatements().count(), 2);

    // verify that removing resources by file URL works
    m_dmModel->removeResources(QList<QUrl>() << QUrl::fromLocalFile(fileB.fileName()), Nepomuk2::NoRemovalFlags, QLatin1String("testapp"));

    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));

    // verify that other resources were not touched
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), Node(), Node()).allStatements().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveResources_subresources()
{
    // create our apps (we use more than one since we also test that it is ignored)
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the graphs
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);
    QUrl mg3;
    const QUrl g3 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg3);
    m_model->addStatement(g3, NAO::maintainedBy(), QUrl("app:/A"), mg3);

    // create the resource to delete
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    // sub-resource 1: can be deleted
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 2: can be deleted (is defined in another graph by the same app)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/AA"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/AA"), g1);
    m_model->addStatement(QUrl("res:/AA"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // sub-resource 3: can be deleted although another res refs it (we also delete the other res)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/C"), g1);

    // sub-resource 4: cannot be deleted since another res refs it
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/E"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/E"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 5: can be deleted although another app added properties
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/F"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/F"), g1);
    m_model->addStatement(QUrl("res:/F"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/F"), QUrl("prop:/int"), LiteralValue(42), g2);

    // delete the resource
    m_dmModel->removeResources(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));

    // this should have removed A, B and C
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/C"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/F"), Node(), Node()));

    // E and F need to be preserved
    QCOMPARE(m_model->listStatements(QUrl("res:/D"), Node(), Node()).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/E"), Node(), Node()).allStatements().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testRemoveResources_invalid_args()
{
    QTemporaryFile fileA;
    fileA.open();

    // prepare some test data
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);

    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("whatever")), g1);


    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resource list
    m_dmModel->removeResources(QList<QUrl>(), Nepomuk2::NoRemovalFlags, QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // resource list with empty URL
    m_dmModel->removeResources(QList<QUrl>() << QUrl("res:/A") << QUrl(), Nepomuk2::NoRemovalFlags, QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->removeResources(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QString());

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}


// make sure the mtime of related resources is updated properly
void DataManagementModelTest::testRemoveResources_mtimeRelated()
{
    // first we create our apps and graphs (just to have some pseudo real data)
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    const QDateTime date = QDateTime::currentDateTime();
    const QUrl ng = m_dmModel->nepomukGraph();

    // now we create different resources
    // A is the resource to be deleted
    // B is related to A and its mtime needs update
    // C is unrelated and no mtime change should occur
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(date), ng);
    m_model->addStatement(QUrl("res:/A"), NAO::lastModified(), LiteralValue(date), ng);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(date), ng);
    m_model->addStatement(QUrl("res:/B"), NAO::lastModified(), LiteralValue(date), ng);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A"), g2);

    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/C"), NAO::created(), LiteralValue(date), ng);
    m_model->addStatement(QUrl("res:/C"), NAO::lastModified(), LiteralValue(date), ng);


    // now we remove res:/A
    m_dmModel->removeResources(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));


    // now only the mtime of B should have changed
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().count(), 1);
    QVERIFY(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime() > date);

    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime(), date);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure we can remove data from non-existing files
void DataManagementModelTest::testRemoveResources_deletedFile()
{
    QTemporaryFile fileA;
    fileA.open();

    const KUrl fileUrl(fileA.fileName());

    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the data graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    // create the resource
    m_model->addStatement(QUrl("res:/A"), NIE::url(), fileUrl, g1);
    m_model->addStatement(QUrl("res:/A"), RDF::type(), NFO::FileDataObject(), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // now remove the file
    fileA.close();
    QFile::remove(fileUrl.toLocalFile());

    // now try removing the data
    m_dmModel->removeResources(QList<QUrl>() << fileUrl, NoRemovalFlags, QLatin1String("A"));

    // the call should succeed
    QVERIFY(!m_dmModel->lastError());

    // the resource should be gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(Node(), Node(), fileUrl));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testCreateResource()
{
    // the simple test: we just create a resource using all params
    const QUrl resUri = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA") << NCO::Contact(), QLatin1String("the label"), QLatin1String("the desc"), QLatin1String("A"));

    // this call should succeed
    QVERIFY(!m_dmModel->lastError());

    // check if the returned uri is valid
    QVERIFY(!resUri.isEmpty());
    QCOMPARE(resUri.scheme(), QString(QLatin1String("nepomuk")));

    // check if the resource was created properly
    QVERIFY(m_model->containsAnyStatement(resUri, RDF::type(), QUrl("class:/typeA")));
    QVERIFY(m_model->containsAnyStatement(resUri, RDF::type(), NCO::Contact()));
    QVERIFY(m_model->containsAnyStatement(resUri, NAO::prefLabel(), LiteralValue::createPlainLiteral(QLatin1String("the label"))));
    QVERIFY(m_model->containsAnyStatement(resUri, NAO::description(), LiteralValue::createPlainLiteral(QLatin1String("the desc"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testCreateResource_types()
{
    QList<QUrl> types;
    types << NMM::MusicPiece() << NFO::FileDataObject() << RDFS::Resource();

    QUrl uri = m_dmModel->createResource( types, QString(), QString(), QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());

    QList<Node> typeNodes = m_model->listStatements( uri, RDF::type(), QUrl() ).iterateObjects().allNodes();
    QCOMPARE( typeNodes.size(), 1 );
    QCOMPARE( typeNodes.first().uri(), NMM::MusicPiece() );

    types << NFO::Folder();
    QUrl uri2 = m_dmModel->createResource( types, QString(), QString(), QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());

    typeNodes = m_model->listStatements( uri2, RDF::type(), QUrl() ).iterateObjects().allNodes();
    QCOMPARE( typeNodes.size(), 2 );
    QVERIFY( typeNodes.contains( NMM::MusicPiece() ) );
    QVERIFY( typeNodes.contains( NFO::Folder() ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testCreateResource_invalid_args()
{
    // try to create a resource without any types
    const QUrl uri = m_dmModel->createResource(QList<QUrl>(), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // should have been created with RDFS::Resource
    QList<Node> nodes = m_model->listStatements( uri, RDF::type(), QUrl() ).iterateObjects().allNodes();
    QCOMPARE( nodes.size(), 1 );
    QCOMPARE( nodes.front().uri(), RDFS::Resource() );

    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();

    // use an invalid type
    m_dmModel->createResource(QList<QUrl>() << QUrl("class:/non-existing-type"), QString(), QString(), QLatin1String("A"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // use a property as type
    m_dmModel->createResource(QList<QUrl>() << NAO::prefLabel(), QString(), QString(), QLatin1String("A"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}

// the isolated test: create one graph with one resource, delete that resource
void DataManagementModelTest::testRemoveDataByApplication1()
{
    // create our app
    const QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), g1);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // verify that the resource has been deleted
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// scatter resource over two graphs, only one of which is supposed to be removed
void DataManagementModelTest::testRemoveDataByApplication2()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // only two statements left: the one in the second graph and the last modification date
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements().count(), 2);
    QVERIFY(m_model->containsStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NAO::lastModified(), Node()));

    // four graphs: g2, the 2 app graphs, and the mtime graph
    // and 1 for the nepomuk graph
    QCOMPARE(m_model->listStatements(Node(), RDF::type(), NRL::InstanceBase()).allStatements().count(), 5);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// two apps that maintain a graph should keep the data when one removes it
void DataManagementModelTest::testRemoveDataByApplication3()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // the resource should still be there, without any changes, not even a changed mtime
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements().count(), 3);
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements().count(), 3);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test file URLs + not removing nie:url
void DataManagementModelTest::testRemoveDataByApplication4()
{
    QTemporaryFile fileA;
    fileA.open();

    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();
    m_model->addStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName()), nepomukG);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl::fromLocalFile(fileA.fileName()), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // now the nie:url should still be there even though A created it
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NIE::url(), QUrl::fromLocalFile(fileA.fileName())));

    // creation time should have been created
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NAO::lastModified(), Node()));

    // the foobar value should be gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    // the "hello world" should still be there
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world"))));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test sub-resource handling the easy kind
void DataManagementModelTest::testRemoveDataByApplication5()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));

    // this should have removed both A and B
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test sub-resource handling
void DataManagementModelTest::testRemoveDataByApplication6()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the graphs
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);
    QUrl mg3;
    const QUrl g3 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg3);
    m_model->addStatement(g3, NAO::maintainedBy(), QUrl("app:/A"), mg3);

    // create the resource to delete
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    // sub-resource 1: can be deleted
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 2: can be deleted (is defined in another graph by the same app)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/AA"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/AA"), g1);
    m_model->addStatement(QUrl("res:/AA"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // sub-resource 3: can be deleted although another res refs it (we also delete the other res)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/C"), g1);

    // sub-resource 4: cannot be deleted since another res refs it
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/E"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/E"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 5: cannot be deleted since another app added properties
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/F"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/F"), g1);
    m_model->addStatement(QUrl("res:/F"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/F"), QUrl("prop:/int"), LiteralValue(42), g2);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));

    // this should have removed A, B and C
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/C"), Node(), Node()));

    // E and F need to be preserved
    QCOMPARE(m_model->listStatements(QUrl("res:/D"), Node(), Node()).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/E"), Node(), Node()).allStatements().count(), 2);
    QCOMPARE(m_model->listStatements(QUrl("res:/F"), Node(), Node()).allStatements().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that we do not remove metadata from resources that were also touched by other apps
void DataManagementModelTest::testRemoveDataByApplication7()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), nepomukG);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // verify that the creation date is still there
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NAO::created(), Soprano::Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure everything is removed even if splitted in more than one graph
void DataManagementModelTest::testRemoveDataByApplication8()
{
    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the resource to delete
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/A"), mg2);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), g1);

    // delete the resource
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // verify that all has gone
    // verify that nothing is left, not even the graph
    kDebug() << m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements();
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that we still maintain other resources in the same graph after deleting one resource
void DataManagementModelTest::testRemoveDataByApplication9()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();

    // create the resources
    const QDateTime dt = QDateTime::currentDateTime();
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(dt), nepomukG);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(dt), nepomukG);

    // now remove res:/A by app A
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // now there should be 2 graphs - once for res:/A which is only maintained by B, and one for res:/B which is still
    // maintained by A and B
    // 1. check that B still maintains res:/A (all of it in one graph)
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { graph ?g { <res:/A> <prop:/string> %1 . } . ?g nao:maintainedBy <app:/B> . }")
                                  .arg(Soprano::Node::literalToN3(LiteralValue(QLatin1String("foobar")))),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // 2. check that A does not maintain res:/A anymore
    QVERIFY(!m_model->executeQuery(QString::fromLatin1("ask where { graph ?g { <res:/A> ?p ?o } . ?g %1 <app:/A> . }")
                                  .arg(Soprano::Node::resourceToN3(NAO::maintainedBy())),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    // 3. check that both A and B do still maintain res:/B
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { graph ?g { <res:/B> <prop:/string> %1 . } . ?g nao:maintainedBy <app:/A> . }")
                                  .arg(Soprano::Node::literalToN3(LiteralValue(QLatin1String("hello world")))),
                                  Soprano::Query::QueryLanguageSparql).boolValue());
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { graph ?g { <res:/B> <prop:/string> %1 . } . ?g nao:maintainedBy <app:/B> . }")
                                  .arg(Soprano::Node::literalToN3(LiteralValue(QLatin1String("hello world")))),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// This test simply creates a lot of resources using storeResources and then
// removes all of them using removeDataByApplication.
// This is exactly what the strigi service does.
void DataManagementModelTest::testRemoveDataByApplication10()
{
    QLatin1String app("AppA");
    QList<QUrl> uris;

    for( int i=0; i<10; i++ ) {
        QTemporaryFile fileA;
        fileA.open();
        QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

        SimpleResource res;
        res.addProperty( RDF::type(), NFO::FileDataObject() );
        res.addProperty( NIE::url(), QUrl(fileA.fileName()) );

        m_dmModel->storeResources( SimpleResourceGraph() << res, app );
        QVERIFY( !m_dmModel->lastError() );

        QString query = QString::fromLatin1("select ?r where { ?r %1 %2 . }")
                        .arg( Node::resourceToN3( NIE::url() ),
                            Node::resourceToN3(fileUrl) );

        QList<Node> list = m_dmModel->executeQuery( query, Soprano::Query::QueryLanguageSparql ).iterateBindings(0).allNodes();
        QCOMPARE( list.size(), 1 );

        uris << list.first().uri();
    }

    //
    // Remove the data
    //

    m_dmModel->removeDataByApplication( uris, Nepomuk2::RemoveSubResoures,
                                        QLatin1String("AppA") );
    QVERIFY( !m_dmModel->lastError() );

    QString query = QString::fromLatin1("ask where { graph ?g { ?r ?p ?o . } ?g %1 ?app . ?app %2 %3 . }")
                    .arg( Node::resourceToN3( NAO::maintainedBy() ),
                          Node::resourceToN3( NAO::identifier() ),
                          Node::literalToN3( app ) );

    QVERIFY( !m_dmModel->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    foreach( const QUrl resUri, uris ) {

        // The Resource should no longer have any statements
        QList<Soprano::Statement> l = m_dmModel->listStatements( resUri, Node(), Node() ).allStatements();
        QVERIFY( l.isEmpty() );
    }

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


// make sure that sub-resources to sub-resources can have backlinks which do not interfer
void DataManagementModelTest::testRemoveDataByApplication12()
{
    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);


    // create the resources: one main resource, one sub, and one sub-sub with a backlink to the sub
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), NAO::hasSubResource(), QUrl("res:/C"), g1);

    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello worldy")), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/res"), QUrl("res:/B"), g1);


    // delete both A and B with sub-resources
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));


    // now all 3 resources should be gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/C"), Node(), Node()));
}

// make sure that weird cross sub-resource'ing is handled properly. This is very unlikely to ever happen, but still...
void DataManagementModelTest::testRemoveDataByApplication_subResourcesOfSubResources()
{
    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    // create the resource to delete
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    // sub-resource 1
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 2
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 3 (also sub-resource to res:/C)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);


    // delete the resource
    QBENCHMARK_ONCE
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));


    // all resources should have been removed
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/C"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/D"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// the same test as above except that another app created information about the resource, too
void DataManagementModelTest::testRemoveDataByApplication_subResourcesOfSubResources2()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // create the graphs
    QUrl mg1, mg2, mg3;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    const QUrl g3 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg3);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);
    m_model->addStatement(g3, NAO::maintainedBy(), QUrl("app:/A"), mg3);

    // create the resource to delete
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    // add some data by app B
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world 2")), g2);

    // sub-resource 1
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 2
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/C"), g3);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/C"), g3);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // sub-resource 3 (also sub-resource to res:/C)
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);


    // delete the resource
    QBENCHMARK_ONCE
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::RemoveSubResoures, QLatin1String("A"));


    // all sub-resources should have been removed
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/C"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/D"), Node(), Node()));

    // the only statements left for resA should be the one from app B and the metadata (which is only lastModified)
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allElements().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testRemoveDataByApplication_nieUrl()
{
    KTemporaryFile file;
    file.open();
    const QUrl fileUrl( file.fileName() );

    const QUrl res1("nepomuk:/res/1");

    // The file is tagged via Dolphin
    const QUrl g1 = m_nrlModel->createGraph( NRL::InstanceBase() );
    const QUrl ng = m_dmModel->nepomukGraph();

    m_model->addStatement( res1, RDF::type(), NFO::FileDataObject(), g1 );
    m_model->addStatement( res1, NIE::url(), fileUrl, ng );
    QDateTime now = QDateTime::currentDateTime();
    m_model->addStatement( res1, NAO::created(), LiteralValue(QVariant(now)), ng );
    m_model->addStatement( res1, NAO::lastModified(), LiteralValue(QVariant(now)), ng );

    const QUrl tag("nepomuk:/res/tag");
    m_model->addStatement( tag, RDF::type(), NAO::Tag(), g1 );
    m_model->addStatement( tag, NAO::identifier(), LiteralValue("tag"), g1 );
    m_model->addStatement( res1, NAO::hasTag(), tag, g1 );

    // Indexed via strigi
    SimpleResource simpleRes( res1 );
    simpleRes.addProperty( RDF::type(), NFO::FileDataObject() );
    simpleRes.addProperty( RDF::type(), NMM::MusicPiece() );
    simpleRes.addProperty( NIE::url(), fileUrl );

    m_dmModel->storeResources( SimpleResourceGraph() << simpleRes, QLatin1String("nepomukindexer"));
    QVERIFY( !m_dmModel->lastError() );

    // Remove strigi indexed content
    m_dmModel->removeDataByApplication( QList<QUrl>() << res1, Nepomuk2::RemoveSubResoures,
                                        QLatin1String("nepomukindexer") );

    // The tag should still be there
    QVERIFY( m_model->containsStatement( tag, RDF::type(), NAO::Tag(), g1 ) );
    QVERIFY( m_model->containsStatement( tag, NAO::identifier(), LiteralValue("tag"), g1 ) );
    QVERIFY( m_model->containsStatement( res1, NAO::hasTag(), tag, g1 ) );

    // The resource should not have any data maintained by "nepomukindexer"
    QString query = QString::fromLatin1("select * where { graph ?g { %1 ?p ?o. } ?g %2 ?a . ?a %3 %4. }")
                    .arg( Node::resourceToN3( res1 ),
                          Node::resourceToN3( NAO::maintainedBy() ),
                          Node::resourceToN3( NAO::identifier() ),
                          Node::literalToN3( LiteralValue("nepomukindexer") ) );

    QList< BindingSet > bs = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).allBindings();
    kDebug() << bs;
    QVERIFY( bs.isEmpty() );

    // The nie:url should still exist
    QVERIFY( m_model->containsAnyStatement( res1, NIE::url(), fileUrl, ng ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure the mtime is updated properly in different situations
void DataManagementModelTest::testRemoveDataByApplication_mtime()
{
    // first we create our apps and graphs
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();
    const QDateTime date = QDateTime::currentDateTime();


    // now we create different resources
    // A has actual data maintained by app:/A
    // B has only metadata maintained by app:/A
    // C has nothing maintained by app:/A
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/A"), NAO::lastModified(), LiteralValue(date), nepomukG);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/B"), NAO::lastModified(), LiteralValue(date), nepomukG);

    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/C"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/C"), NAO::lastModified(), LiteralValue(date), nepomukG);


    // we delete all three
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/B") << QUrl("res:/C"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));


    // now only the mtime of A should have changed
    QCOMPARE(m_model->listStatements(QUrl("res:/A"), NAO::lastModified(), Node()).allElements().count(), 1);
    QVERIFY(m_model->listStatements(QUrl("res:/A"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime() > date);

    QCOMPARE(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime(), date);

    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime(), date);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure the mtime of resources that are related to deleted ones is updated
void DataManagementModelTest::testRemoveDataByApplication_mtimeRelated()
{
    // first we create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();
    const QDateTime date = QDateTime::currentDateTime();

    // we three two resources - one to delete, and one which is related to the one to be deleted,
    // one which is also related but will not be changed
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello")), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A"), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/res"), QUrl("res:/A"), g2);

    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), nepomukG);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/B"), NAO::lastModified(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/C"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/C"), NAO::lastModified(), LiteralValue(date), nepomukG);

    // now we remove res:/A
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // now the mtime of res:/B should have been changed
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().count(), 1);
    QVERIFY(m_model->listStatements(QUrl("res:/B"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime() > date);

    // the mtime of res:/C should NOT have changed
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().count(), 1);
    QCOMPARE(m_model->listStatements(QUrl("res:/C"), NAO::lastModified(), Node()).allElements().first().object().literal().toDateTime(), date);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure relations to removed resources are handled properly
void DataManagementModelTest::testRemoveDataByApplication_related()
{
    // first we create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    // we create 3 graphs, maintained by different apps
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);
    QUrl mg3;
    const QUrl g3 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg3);
    m_model->addStatement(g3, NAO::maintainedBy(), QUrl("app:/A"), mg3);

    const QDateTime date = QDateTime::currentDateTime();
    QUrl nepomukG = m_dmModel->nepomukGraph();

    // create two resources:
    // A is split across both graphs
    // B is only in one graph
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("Hello World")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/A"), NAO::lastModified(), LiteralValue(date), nepomukG);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/B"), NAO::lastModified(), LiteralValue(date), nepomukG);

    // three relations B -> A, one in each graph
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A"), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res2"), QUrl("res:/A"), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res3"), QUrl("res:/A"), g3);

    // a third resource which is deleted entirely but has one relation in another graph
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::created(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/C"), NAO::lastModified(), LiteralValue(date), nepomukG);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/C"), g2);


    // now remove A
    m_dmModel->removeDataByApplication(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/C"), Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // now only the relation in the first graph should have been removed
    kDebug() << m_model->listStatements(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A"), g1).allStatements();
    QVERIFY(!m_model->containsStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A"), g1));
    QVERIFY(!m_model->containsStatement(QUrl("res:/B"), QUrl("prop:/res3"), QUrl("res:/A"), g3));
    QVERIFY(m_model->containsStatement(QUrl("res:/B"), QUrl("prop:/res2"), QUrl("res:/A"), g2));
    QVERIFY(!m_model->containsStatement(QUrl("res:/B"), QUrl("prop:/res3"), QUrl("res:/C"), g2));

    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/A")));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/res3"), QUrl("res:/A")));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/res2"), QUrl("res:/A")));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), QUrl("prop:/res3"), QUrl("res:/C")));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure we can remove data from non-existing files
void DataManagementModelTest::testRemoveDataByApplication_deletedFile()
{
    QTemporaryFile* fileA = new QTemporaryFile();
    fileA->open();
    const KUrl fileUrl(fileA->fileName());
    delete fileA;
    QVERIFY(!QFile::exists(fileUrl.toLocalFile()));

    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create the data graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    // create the resource
    m_model->addStatement(QUrl("res:/A"), NIE::url(), fileUrl, g1);
    m_model->addStatement(QUrl("res:/A"), RDF::type(), NFO::FileDataObject(), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);


    // now try removing the data
    m_dmModel->removeDataByApplication(QList<QUrl>() << fileUrl, NoRemovalFlags, QLatin1String("A"));

    // the call should succeed
    QVERIFY(!m_dmModel->lastError());

    // the resource should be gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(Node(), Node(), fileUrl));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test that all is removed, ie. storage is clear afterwards
void DataManagementModelTest::testRemoveAllDataByApplication1()
{
    // create our app
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/A"), mg2);

    // create two resources to remove
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar 2")), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world 2")), g2);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), g1);

    m_dmModel->removeDataByApplication(Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    // make sure nothing is there anymore
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/B"), Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test that other resources are not removed - the easy way
void DataManagementModelTest::testRemoveAllDataByApplication2()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    const QUrl ng = m_dmModel->nepomukGraph();

    // create two resources to remove
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar 2")), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world 2")), g2);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);

    m_dmModel->removeDataByApplication(Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), Node(), Node()).allStatements().count(), 3);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test that an app is simply removed as maintainer of a graph
void DataManagementModelTest::testRemoveAllDataByApplication3()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    QUrl nepomukG = m_dmModel->nepomukGraph();

    // create two resources to remove
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar 2")), g1);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world 2")), g1);

    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar 2")), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world 2")), g2);

    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), nepomukG);
    m_model->addStatement(QUrl("res:/B"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), nepomukG);

    m_dmModel->removeDataByApplication(Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements().count(), 4);
    QCOMPARE(m_model->listStatements(QUrl("res:/B"), Node(), Node()).allStatements().count(), 4);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test that metadata is not removed if the resource still exists even if its in a deleted graph
void DataManagementModelTest::testRemoveAllDataByApplication4()
{
    // create our apps
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/B"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/B"), NAO::identifier(), LiteralValue(QLatin1String("B")), appG);

    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);
    QUrl mg2;
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg2);
    m_model->addStatement(g2, NAO::maintainedBy(), QUrl("app:/B"), mg2);

    // create two resources to remove
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g2);
    m_model->addStatement(QUrl("res:/A"), NAO::created(), LiteralValue(QDateTime::currentDateTime()), m_dmModel->nepomukGraph());

    m_dmModel->removeDataByApplication(Nepomuk2::NoRemovalFlags, QLatin1String("A"));

    QCOMPARE(m_model->listStatements(QUrl("res:/A"), Node(), Node()).allStatements().count(), 3);
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NAO::created(), Soprano::Node()));
    QVERIFY(m_model->containsAnyStatement(QUrl("res:/A"), NAO::lastModified(), Soprano::Node()));
    QVERIFY(m_model->containsAnyStatement(Node(), NAO::maintainedBy(), QUrl("app:/A")));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


namespace {
    int push( Soprano::Model * model, Nepomuk2::SimpleResource res, QUrl graph ) {
        QHashIterator<QUrl, QVariant> it( res.properties() );
        if( !res.uri().isValid() )
            return 0;

        int numPushed = 0;
        Soprano::Statement st( res.uri(), Soprano::Node(), Soprano::Node(), graph );
        while( it.hasNext() ) {
            it.next();
            st.setPredicate( it.key() );
            if(it.value().type() == QVariant::Url)
                st.setObject(it.value().toUrl());
            else
                st.setObject( Soprano::LiteralValue(it.value()) );
            if( model->addStatement( st ) == Soprano::Error::ErrorNone )
                numPushed++;
        }
        return numPushed;
    }
}


void DataManagementModelTest::testStoreResources_strigiCase()
{
    //
    // This is for testing exactly how Strigi will use storeResources ie
    // have some blank nodes ( some of which may already exists ) and a
    // main resources which does not exist
    //

    kDebug() << "Starting Strigi merge test";

    QUrl graphUri = m_nrlModel->createGraph(NRL::InstanceBase());

    Nepomuk2::SimpleResource coldplay;
    coldplay.addProperty( RDF::type(), NCO::Contact() );
    coldplay.addProperty( NCO::fullname(), "Coldplay" );

    // Push it into the model with a proper uri
    coldplay.setUri( QUrl("nepomuk:/res/coldplay") );
    QVERIFY( push( m_model, coldplay, graphUri ) == coldplay.properties().size() );

    // Now keep it as a blank node
    coldplay.setUri( QUrl("_:coldplay") );

    Nepomuk2::SimpleResource album;
    album.setUri( QUrl("_:XandY") );
    album.addProperty( RDF::type(), NMM::MusicAlbum() );
    album.addProperty( NIE::title(), "X&Y" );

    Nepomuk2::SimpleResource res1;
    res1.addProperty( RDF::type(), NFO::FileDataObject() );
    res1.addProperty( RDF::type(), NMM::MusicPiece() );
    res1.addProperty( NFO::fileName(), "Yellow.mp3" );
    res1.addProperty( NMM::performer(), QUrl("_:coldplay") );
    res1.addProperty( NMM::musicAlbum(), QUrl("_:XandY") );
    Nepomuk2::SimpleResourceGraph resGraph;
    resGraph << res1 << coldplay << album;

    //
    // Do the actual merging
    //
    kDebug() << "Perform the merge";
    m_dmModel->storeResources( resGraph, "TestApp" );
    QVERIFY( !m_dmModel->lastError() );

    QList<Soprano::Statement> stList = m_model->listStatements( Node(), RDF::type(), NMM::MusicPiece() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl res1Uri = stList.first().subject().uri();

    QVERIFY( m_model->containsAnyStatement( res1Uri, Soprano::Node(),
                                            Soprano::Node() ) );
    QVERIFY( m_model->containsAnyStatement( res1Uri, NFO::fileName(),
                                            Soprano::LiteralValue("Yellow.mp3") ) );
    // Make sure we have the nao:created and nao:lastModified
    QVERIFY( m_model->containsAnyStatement( res1Uri, NAO::lastModified(),
                                            Soprano::Node() ) );
    QVERIFY( m_model->containsAnyStatement( res1Uri, NAO::created(),
                                            Soprano::Node() ) );
    kDebug() << m_model->listStatements( res1Uri, Soprano::Node(), Soprano::Node() ).allStatements();
    // The +2 is because nao:created and nao:lastModified would have also been added
    QCOMPARE( m_model->listStatements( res1Uri, Soprano::Node(), Soprano::Node() ).allStatements().size(),
                res1.properties().size() + 2 );

    QList< Node > objects = m_model->listStatements( res1Uri, NMM::performer(), Soprano::Node() ).iterateObjects().allNodes();

    QVERIFY( objects.size() == 1 );
    QVERIFY( objects.first().isResource() );

    QUrl coldplayUri = objects.first().uri();
    QCOMPARE( coldplayUri, QUrl("nepomuk:/res/coldplay") );
    stList = coldplay.toStatementList();
    foreach( Soprano::Statement st, stList ) {
        st.setSubject( coldplayUri );
        QVERIFY( m_model->containsAnyStatement( st ) );
    }

    objects = m_model->listStatements( res1Uri, NMM::musicAlbum(), Soprano::Node() ).iterateObjects().allNodes();

    QVERIFY( objects.size() == 1 );
    QVERIFY( objects.first().isResource() );

    QUrl albumUri = objects.first().uri();
    stList = album.toStatementList();
    foreach( Soprano::Statement st, stList ) {
        st.setSubject( albumUri );
        QVERIFY( m_model->containsAnyStatement( st ) );
    }

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_createResource()
{
    //
    // Simple case: create a resource by merging it
    //
    SimpleResource res;
    res.setUri(QUrl("_:A"));
    res.addProperty(RDF::type(), NAO::Tag());
    res.addProperty(NAO::prefLabel(), QLatin1String("Foobar"));

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));
    QVERIFY( !m_dmModel->lastError() );

    // check if the resource exists
    QVERIFY(m_model->containsAnyStatement(Soprano::Node(), RDF::type(), NAO::Tag()));
    QVERIFY(m_model->containsAnyStatement(Soprano::Node(), NAO::prefLabel(), Soprano::LiteralValue::createPlainLiteral(QLatin1String("Foobar"))));

    // make sure only one tag resource was created
    QCOMPARE(m_model->listStatements(Node(), RDF::type(), NAO::Tag()).allElements().count(), 1);

    // get the new resources URI
    const QUrl resUri = m_model->listStatements(Node(), RDF::type(), NAO::Tag()).iterateSubjects().allNodes().first().uri();

    // check that it has the default metadata
    QVERIFY(m_model->containsAnyStatement(resUri, NAO::created(), Node()));
    QVERIFY(m_model->containsAnyStatement(resUri, NAO::lastModified(), Node()));

    // and both created and last modification date should be similar
    QCOMPARE(m_model->listStatements(resUri, NAO::created(), Node()).iterateObjects().allNodes().first(),
             m_model->listStatements(resUri, NAO::lastModified(), Node()).iterateObjects().allNodes().first());

    // check if all the correct metadata graphs exist
    // ask where {
    //  graph ?g { ?r a nao:Tag . ?r nao:prefLabel "Foobar" . } .
    //  graph ?mg { ?g a nrl:InstanceBase . ?mg a nrl:GraphMetadata . ?mg nrl:coreGraphMetadataFor ?g . } .
    //  ?g nao:maintainedBy ?a . ?a nao:identifier "testapp"
    // }
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { "
                                                      "graph ?g { ?r a %1 . ?r %2 %3 . } . "
                                                      "graph ?mg { ?g a %4 . ?mg a %5 . ?mg %6 ?g . } . "
                                                      "?g %7 ?a . ?a %8 %9 . "
                                                      "}")
                                  .arg(Soprano::Node::resourceToN3(NAO::Tag()),
                                       Soprano::Node::resourceToN3(NAO::prefLabel()),
                                       Soprano::Node::literalToN3(LiteralValue::createPlainLiteral(QLatin1String("Foobar"))),
                                       Soprano::Node::resourceToN3(NRL::InstanceBase()),
                                       Soprano::Node::resourceToN3(NRL::GraphMetadata()),
                                       Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor()),
                                       Soprano::Node::resourceToN3(NAO::maintainedBy()),
                                       Soprano::Node::resourceToN3(NAO::identifier()),
                                       Soprano::Node::literalToN3(QLatin1String("testapp"))),
                                  Soprano::Query::QueryLanguageSparql).boolValue());

    //
    // Now create the same resource again
    //
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();
    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));
    QVERIFY( !m_dmModel->lastError() );

    // nothing should have happened, apart from the nao:lastModified
    QSet<Soprano::Statement> newStList = m_model->listStatements().allStatements().toSet();
    foreach(const Soprano::Statement& st, newStList) {
        if( st.predicate() != NAO::lastModified() )
            QVERIFY(existingStatements.containsAnyStatement(st));
    }

    //
    // Now create the same resource with a different app
    //
    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp2"));
    QVERIFY( !m_dmModel->lastError() );

    // All the statements should have been duplicated in another graph which will
    // have a different maintainer

    QList<Node> gNodes1 = m_model->listStatements( QUrl(), RDF::type(), NAO::Tag() ).iterateContexts().allNodes();
    QList<Node> gNodes2 = m_model->listStatements( QUrl(), NAO::prefLabel(), LiteralValue::createPlainLiteral("Foobar") ).iterateContexts().allNodes();

    QCOMPARE( gNodes1.toSet().size(), 2 );
    QCOMPARE( gNodes2.toSet().size(), 2 );
    QCOMPARE( gNodes1.toSet(), gNodes2.toSet() );

    // Check if the new app exists, and make sure both the graphs have their own nao:maintainedBy
    // things

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_invalid_args()
{
    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resources -> no error but no change either
    m_dmModel->storeResources(SimpleResourceGraph(), QLatin1String("testapp"));

    // no error
    QVERIFY(!m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->storeResources(SimpleResourceGraph(), QString());

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid resource in graph
    m_dmModel->storeResources(SimpleResourceGraph() << SimpleResource(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid range used in one resource
    SimpleResource res;
    res.setUri(QUrl("nepomuk:/res/A"));
    res.addProperty(QUrl("prop:/int"), QVariant(QLatin1String("foobar")));
    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // non-existing file
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    SimpleResource nonExistingFileRes;
    nonExistingFileRes.setUri(nonExistingFileUrl);
    nonExistingFileRes.addProperty(QUrl("prop:/int"), QVariant(42));
    m_dmModel->storeResources(SimpleResourceGraph() << nonExistingFileRes, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // non-existing file as object
    nonExistingFileRes.setUri(QUrl("nepomuk:/res/A"));
    nonExistingFileRes.addProperty(QUrl("prop:/res"), nonExistingFileUrl);
    m_dmModel->storeResources(SimpleResourceGraph() << nonExistingFileRes, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid graph metadata 1
    SimpleResource res2;
    res.setUri(QUrl("nepomuk:/res/A"));
    res.addProperty(QUrl("prop:/int"), QVariant(42));
    QHash<QUrl, QVariant> invalidMetadata;
    invalidMetadata.insert(QUrl("prop:/int"), QLatin1String("foobar"));
    m_dmModel->storeResources(SimpleResourceGraph() << res2, QLatin1String("testapp"), Nepomuk2::IdentifyNew, Nepomuk2::NoStoreResourcesFlags, invalidMetadata);

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid graph metadata 2
    invalidMetadata.clear();
    invalidMetadata.insert(NAO::maintainedBy(), QLatin1String("foobar"));
    m_dmModel->storeResources(SimpleResourceGraph() << res2, QLatin1String("testapp"), Nepomuk2::IdentifyNew, Nepomuk2::NoStoreResourcesFlags, invalidMetadata);

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}

void DataManagementModelTest::testStoreResources_invalid_args_with_existing()
{
    // create a test resource
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    // create the resource to delete
    QUrl resA("nepomuk:/res/A");
    QUrl resB("nepomuk:/res/B");
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(resA, QUrl("prop:/res"), resB, g1);
    const QDateTime now = QDateTime::currentDateTime();
    m_model->addStatement(resA, NAO::created(), LiteralValue(now), g1);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(now), g1);


    // create a resource to merge
    SimpleResource a(resA);
    a.addProperty(QUrl("prop:/int"), 42);


    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // empty resources -> no error but no change either
    m_dmModel->storeResources(SimpleResourceGraph(), QLatin1String("testapp"));

    // no error
    QVERIFY(!m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // empty app
    m_dmModel->storeResources(SimpleResourceGraph() << a, QString());

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid resource in graph
    m_dmModel->storeResources(SimpleResourceGraph() << a << SimpleResource(), QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid range used in one resource
    SimpleResource res(a);
    res.addProperty(QUrl("prop:/int"), QVariant(QLatin1String("foobar")));
    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));

    // this call should fail
    QVERIFY(m_dmModel->lastError());

    // no data should have been changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // non-existing file as object
    const QUrl nonExistingFileUrl("file:///a/file/that/is/very/unlikely/to/exist");
    SimpleResource nonExistingFileRes(a);
    nonExistingFileRes.addProperty(QUrl("prop:/res"), nonExistingFileUrl);
    m_dmModel->storeResources(SimpleResourceGraph() << nonExistingFileRes, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);


    // invalid cardinality in resource
    SimpleResource invalidCRes(a);
    invalidCRes.addProperty(QUrl("prop:/int_c1"), 42);
    invalidCRes.addProperty(QUrl("prop:/int_c1"), 2);
    m_dmModel->storeResources(SimpleResourceGraph() << invalidCRes, QLatin1String("testapp"));

    // the call should have failed
    QVERIFY(m_dmModel->lastError());

    // nothing should have changed
    QCOMPARE(Graph(m_model->listStatements().allStatements()), existingStatements);
}

void DataManagementModelTest::testStoreResources_file1()
{
    QTemporaryFile fileA;
    fileA.open();

    // merge a file URL
    SimpleResource r1;
    r1.setUri(QUrl::fromLocalFile(fileA.fileName()));
    r1.addProperty(RDF::type(), NAO::Tag());
    r1.addProperty(QUrl("prop:/string"), QLatin1String("Foobar"));

    m_dmModel->storeResources(SimpleResourceGraph() << r1, QLatin1String("testapp"));
    QVERIFY( !m_dmModel->lastError() );

    // a nie:url relation should have been created
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())));

    // the file URL should never be used as subject
    QVERIFY(!m_model->containsAnyStatement(QUrl::fromLocalFile(fileA.fileName()), Node(), Node()));

    // make sure file URL and res URI are properly related including the properties
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { ?r %1 %4 . "
                                                      "?r a %2 . "
                                                      "?r <prop:/string> %3 . }")
                                  .arg(Node::resourceToN3(NIE::url()),
                                       Node::resourceToN3(NAO::Tag()),
                                       Node::literalToN3(LiteralValue(QLatin1String("Foobar"))),
                                       Node::resourceToN3(QUrl::fromLocalFile(fileA.fileName()))),
                                  Query::QueryLanguageSparql).boolValue());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_file2()
{
    // merge a property with non-existing file value
    QTemporaryFile fileA;
    fileA.open();
    const QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

    SimpleResource r1;
    r1.addProperty(QUrl("prop:/res"), fileUrl);

    m_dmModel->storeResources(SimpleResourceGraph() << r1, QLatin1String("testapp"));
    QVERIFY( !m_dmModel->lastError() );

    QList<Soprano::Statement> stList = m_model->listStatements( Node(), QUrl("prop:/res"), Node() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl r1Uri = stList.first().subject().uri();

    // the property should have been created
    QVERIFY(m_model->containsAnyStatement(r1Uri, QUrl("prop:/res"), Node()));

    // but it should not be related to the file URL
    QVERIFY(!m_model->containsAnyStatement(r1Uri, QUrl("prop:/res"), fileUrl));

    // there should be a nie:url for the file URL
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), fileUrl));

    // make sure file URL and res URI are properly related including the properties
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { %3 <prop:/res> ?r . "
                                                      "?r %1 %2 . }")
                                  .arg(Node::resourceToN3(NIE::url()),
                                       Node::resourceToN3(fileUrl),
                                       Node::resourceToN3(r1Uri)),
                                  Query::QueryLanguageSparql).boolValue());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_file3()
{
    QTemporaryFile fileA;
    fileA.open();
    const QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

    SimpleResource r1;
    r1.setUri( fileUrl );
    r1.addProperty( RDF::type(), QUrl("class:/typeA") );

    m_dmModel->storeResources( SimpleResourceGraph() << r1, QLatin1String("origApp") );
    QVERIFY( !m_dmModel->lastError() );

    // Make sure all the data has been added and it belongs to origApp
    QString query = QString::fromLatin1("ask { graph ?g { ?r a %1 . "
                                        " ?r a nfo:FileDataObject . } "
                                        " ?g nao:maintainedBy ?app . ?app nao:identifier %2 . }")
                    .arg( Soprano::Node::resourceToN3( QUrl("class:/typeA") ),
                          Soprano::Node(Soprano::LiteralValue("origApp")).toN3() );

    QVERIFY( m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    query = QString::fromLatin1("ask { graph ?g { ?r nie:url %1 . "
                                "  } ?g nao:maintainedBy ?app . ?app nao:identifier %2 . }")
                    .arg( Soprano::Node::resourceToN3( fileUrl ),
                          Soprano::Node(Soprano::LiteralValue("nepomuk")).toN3() );

    QVERIFY( m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    QList< Statement > stList = m_model->listStatements( Soprano::Node(), NIE::url(), fileUrl ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QUrl resUri = stList.first().subject().uri();
    QVERIFY( resUri.scheme() == QLatin1String("nepomuk") );

    SimpleResource r2;
    r2.setUri( fileUrl );
    r2.addProperty( QUrl("prop:/res"), NFO::FileDataObject() );

    m_dmModel->storeResources( SimpleResourceGraph() << r2, QLatin1String("newApp") );
    QVERIFY( !m_dmModel->lastError() );

    // Make sure it was identified properly
    QSet<Node> resNodes = m_model->listStatements( QUrl(), NIE::url(), fileUrl ).iterateSubjects().allNodes().toSet();
    QCOMPARE( resNodes.size(), 1 );

    QUrl resUri2 = resNodes.begin()->uri();
    QVERIFY( resUri2.scheme() == QLatin1String("nepomuk") );
    QCOMPARE( resUri, resUri2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_file4()
{
    QTemporaryFile fileA;
    fileA.open();
    const QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

    SimpleResource res;
    res.addProperty( RDF::type(), fileUrl );
    res.addProperty( QUrl("prop:/res"), fileUrl );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app1") );
    QVERIFY( !m_dmModel->lastError() );

    // Make sure the fileUrl got added
    QList< Statement > stList = m_model->listStatements( Node(), NIE::url(), fileUrl ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QUrl fileResUri = stList.first().subject().uri();

    // Make sure the SimpleResource was stored
    stList = m_model->listStatements( Node(), RDF::type(), fileResUri ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QUrl resUri = stList.first().subject().uri();

    // Check for the other statement
    stList = m_model->listStatements( resUri, QUrl("prop:/res"), Node() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QUrl fileResUri2 = stList.first().object().uri();
    QCOMPARE( fileResUri, fileResUri2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure resources are identified properly via their nie:url value
void DataManagementModelTest::testStoreResources_file5()
{
    // create a file resource
    QTemporaryFile fileA;
    fileA.open();
    const QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

    SimpleResource res(fileUrl);
    res.addProperty( RDF::type(), QUrl("class:/typeA") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("A") );
    QVERIFY( !m_dmModel->lastError() );


    // add information by identifying the file via its nie:url as a property
    SimpleResource res2;
    res2.addProperty(NIE::url(), fileUrl);
    res2.addProperty(QUrl("prop:/int"), 42);

    m_dmModel->storeResources( SimpleResourceGraph() << res2, QLatin1String("A") );
    QVERIFY( !m_dmModel->lastError() );


    // make sure the information was added to the correct resource
    QCOMPARE(m_model->listStatements(Node(), RDF::type(), QUrl("class:/typeA")).allElements().count(), 1);
    const Node fileUri = m_model->listStatements(Node(), RDF::type(), QUrl("class:/typeA")).allElements().first().subject();
    QVERIFY(m_model->containsAnyStatement(fileUri, QUrl("prop:/int"), LiteralValue(42)));
}


void DataManagementModelTest::testStoreResources_folder()
{
    QTemporaryFile file;
    QVERIFY( file.open() );
    KUrl fileUrl = KUrl::fromLocalFile( file.fileName() );
    QUrl folderUrl = QUrl::fromLocalFile( fileUrl.directory() );

    SimpleResource res( fileUrl );
    res.addProperty( RDF::type(), NMM::MusicPiece() );
    res.addProperty( NAO::prefLabel(), QLatin1String("Label") );
    res.addProperty( NIE::isPartOf(), fileUrl.directory() );

    SimpleResourceGraph graph;
    graph << res;

    m_dmModel->storeResources( graph, QLatin1String("testApp") );
    QVERIFY( !m_dmModel->lastError() );

    QList<Statement> stList = m_model->listStatements( Node(), NIE::isPartOf(), Node() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl folderResUri = stList.first().object().uri();
    QVERIFY( m_dmModel->containsAnyStatement( folderResUri, RDF::type(), NFO::FileDataObject() ) );
    QVERIFY( m_dmModel->containsAnyStatement( folderResUri, RDF::type(), NFO::Folder() ) );
    QVERIFY( m_dmModel->containsAnyStatement( folderResUri, NIE::url(), folderUrl ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_fileExists()
{
    QUrl fileUrl("file:///a/b/v/c/c");
    SimpleResource res( fileUrl );
    res.addType( NMM::MusicPiece() );
    res.addProperty( NAO::numericRating(), 10 );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app") );

    // Should give an error - The file does not exist ( probably )
    QVERIFY( m_dmModel->lastError() );

    SimpleResource file;
    file.addType( NFO::FileDataObject() );
    file.setProperty( NIE::url(), fileUrl );

    m_dmModel->storeResources( SimpleResourceGraph() << file, QLatin1String("app") );
    QVERIFY( m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_sameNieUrl()
{
    QTemporaryFile fileA;
    fileA.open();
    const QUrl fileUrl = QUrl::fromLocalFile(fileA.fileName());

    SimpleResource res;
    res.setUri( fileUrl );
    res.addProperty( RDF::type(), NFO::FileDataObject() );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app1") );
    QVERIFY( !m_dmModel->lastError() );

    // Make sure the fileUrl got added
    QList< Statement > stList = m_model->listStatements( Node(), NIE::url(), fileUrl ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QUrl fileResUri = stList.first().subject().uri();

    // Make sure there is only one rdf:type nfo:FileDataObject
    stList = m_model->listStatements( Node(), RDF::type(), NFO::FileDataObject() ).allStatements();
    QCOMPARE( stList.size(), 1 );
    QCOMPARE( stList.first().subject().uri(), fileResUri );

    SimpleResource res2;
    res2.addProperty( NIE::url(), fileUrl );
    res2.addProperty( NAO::numericRating(), QVariant(10) );

    m_dmModel->storeResources( SimpleResourceGraph() << res2, QLatin1String("app1") );
    QVERIFY( !m_dmModel->lastError() );

    // Make sure it got mapped
    stList = m_model->listStatements( Node(), NAO::numericRating(), LiteralValue(10) ).allStatements();
    QCOMPARE( stList.size(), 1 );
    QCOMPARE( stList.first().subject().uri(), fileResUri );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// metadata should be ignored when merging one resource into another
void DataManagementModelTest::testStoreResources_metadata()
{
    // create our app
    const QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create a resource
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl nepomukG = m_dmModel->nepomukGraph();

    const QDateTime now = QDateTime::currentDateTime();
    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("Foobar")), g1);
    m_model->addStatement(resA, QUrl("prop:/string2"), LiteralValue(QLatin1String("Foobar2")), g1);
    m_model->addStatement(resA, NAO::created(), LiteralValue(now), nepomukG);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(now), nepomukG);


    // now we merge the same resource (with differing metadata)
    SimpleResource a;
    a.addProperty(RDF::type(), NAO::Tag());
    a.addProperty(QUrl("prop:/int"), QVariant(42));
    a.addProperty(QUrl("prop:/string"), QVariant(QLatin1String("Foobar")));
    QDateTime creationDateTime(QDate(2010, 12, 24), QTime::currentTime());
    a.addProperty(NAO::created(), QVariant(creationDateTime));

    // merge the resource
    m_dmModel->storeResources(SimpleResourceGraph() << a, QLatin1String("B"));
    QVERIFY(!m_dmModel->lastError());

    // make sure no new resource has been created
    QCOMPARE(m_model->listStatements(Node(), RDF::type(), NAO::Tag()).iterateSubjects().allNodes().toSet().count(), 1);
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/int"), Node()).iterateSubjects().allNodes().toSet().count(), 1);
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/string"), Node()).iterateSubjects().allNodes().toSet().count(), 1);
    QCOMPARE(m_model->listStatements(resA, NAO::created(), Node()).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(resA, NAO::lastModified(), Node()).allStatements().count(), 1);

    // make sure the new app has been created
    QueryResultIterator it = m_model->executeQuery(QString::fromLatin1("select ?a where { ?a a %1 . ?a %2 %3 . }")
                                                   .arg(Node::resourceToN3(NAO::Agent()),
                                                        Node::resourceToN3(NAO::identifier()),
                                                        Node::literalToN3(QLatin1String("B"))),
                                                   Soprano::Query::QueryLanguageSparql);
    QVERIFY(it.next());
    const QUrl appBRes = it[0].uri();

    // make sure the data is now maintained by both apps
    QList<QString> apps;
    apps << QLatin1String("A") << QLatin1String("B");

    checkDataMaintainedBy( Statement(resA, QUrl("prop:/int"), LiteralValue(42)), apps );
    checkDataMaintainedBy( Statement(resA, QUrl("prop:/string"), LiteralValue(QString("Foobar"))), apps );
    checkDataMaintainedBy( Statement(resA, RDF::type(), NAO::Tag()), apps );

    QDateTime mod = m_model->listStatements( resA, NAO::lastModified(), Soprano::Node() ).iterateObjects().allNodes().first().literal().toDateTime();
    QVERIFY( mod >= now );

    // Make sure that nao:created have not changed
    QDateTime creation = m_model->listStatements( resA, NAO::created(), Soprano::Node() ).iterateObjects().allNodes().first().literal().toDateTime();

    // The creation date for now will always stay the same.
    // FIXME: Should we allow changes?
    QVERIFY( creation != creationDateTime );


    // now merge the same resource with some new data - just to make sure the metadata is updated properly
    SimpleResource sResA(resA);
    sResA.addProperty(RDF::type(), NAO::Tag());
    sResA.addProperty(QUrl("prop:/int2"), 42);

    // merge the resource
    m_dmModel->storeResources(SimpleResourceGraph() << sResA, QLatin1String("B"));
    QVERIFY(!m_dmModel->lastError());

    // make sure the new data is there
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/int2"), LiteralValue(42)));

    // make sure creation date did not change
    QCOMPARE(m_model->listStatements(resA, NAO::created(), Node()).allStatements().count(), 1);
    QVERIFY(m_model->containsAnyStatement(resA, NAO::created(), LiteralValue(now)));

    // make sure mtime has changed - the resource has changed
    QCOMPARE(m_model->listStatements(resA, NAO::lastModified(), Node()).allStatements().count(), 1);
    QVERIFY(m_model->listStatements(resA, NAO::lastModified(), Node()).iterateObjects().allNodes().first().literal().toDateTime() != now);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


// make sure storeResources ignores supertypes
void DataManagementModelTest::testStoreResources_superTypes()
{
    // 1. create a resource to merge
    QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(resA, RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(resA, RDF::type(), QUrl("class:/typeB"), g1);
    const QDateTime now = QDateTime::currentDateTime();
    const QUrl ng = m_dmModel->nepomukGraph();
    m_model->addStatement(resA, NAO::created(), LiteralValue(now), ng);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(now), ng);


    // now merge the same resource (excluding the super-type A)
    SimpleResource a;
    a.addProperty(RDF::type(), QUrl("class:/typeB"));
    a.addProperty(QUrl("prop:/string"), QLatin1String("hello world"));

    m_dmModel->storeResources(SimpleResourceGraph() << a, QLatin1String("A"));
    QVERIFY( !m_dmModel->lastError() );


    // make sure the existing resource was reused
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world"))).allElements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure merging even works with missing metadata in store
void DataManagementModelTest::testStoreResources_missingMetadata()
{
    // create our app
    const QUrl appG = m_nrlModel->createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("app:/A"), RDF::type(), NAO::Agent(), appG);
    m_model->addStatement(QUrl("app:/A"), NAO::identifier(), LiteralValue(QLatin1String("A")), appG);

    // create a resource (without creation date)
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);
    m_model->addStatement(g1, NAO::maintainedBy(), QUrl("app:/A"), mg1);

    const QUrl nepomukG = m_dmModel->nepomukGraph();

    const QDateTime now = QDateTime::currentDateTime();
    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("Foobar")), g1);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(now), nepomukG);


    // now we merge the same resource
    SimpleResource a;
    a.addProperty(RDF::type(), NAO::Tag());
    a.addProperty(QUrl("prop:/int"), QVariant(42));
    a.addProperty(QUrl("prop:/string"), QVariant(QLatin1String("Foobar")));

    // merge the resource
    m_dmModel->storeResources(SimpleResourceGraph() << a, QLatin1String("B"));
    QVERIFY( !m_dmModel->lastError() );

    // make sure no new resource has been created
    QStringList apps;
    apps << "A" << "B";

    checkDataMaintainedBy( Statement(QUrl(), RDF::type(), NAO::Tag()), apps );
    checkDataMaintainedBy( Statement(QUrl(), QUrl("prop:/int"), LiteralValue(42)), apps );
    checkDataMaintainedBy( Statement(QUrl(), QUrl("prop:/string"), LiteralValue("Foobar")), apps );

    // now merge the same resource with some new data - just to make sure the metadata is updated properly
    SimpleResource simpleResA(resA);
    simpleResA.addProperty(RDF::type(), NAO::Tag());
    simpleResA.addProperty(QUrl("prop:/int2"), 42);

    // merge the resource
    m_dmModel->storeResources(SimpleResourceGraph() << simpleResA, QLatin1String("B"));
    QVERIFY( !m_dmModel->lastError() );

    // make sure the new data is there
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/int2"), LiteralValue(42)));

    // make sure creation date did not change, ie. it was not created as that would be wrong
    QVERIFY(!m_model->containsAnyStatement(resA, NAO::created(), Node()));

    // make sure the last mtime has been updated
    QCOMPARE(m_model->listStatements(resA, NAO::lastModified(), Node()).allStatements().count(), 1);
    QDateTime newDt = m_model->listStatements(resA, NAO::lastModified(), Node()).iterateObjects().allNodes().first().literal().toDateTime();
    QVERIFY( newDt > now);



    //
    // Merge the resource again, but this time make sure it is identified as well
    //
    SimpleResource resB;
    resB.addProperty(RDF::type(), NAO::Tag());
    resB.addProperty(QUrl("prop:/int"), QVariant(42));
    resB.addProperty(QUrl("prop:/string"), QVariant(QLatin1String("Foobar")));
    resB.addProperty(QUrl("prop:/int2"), 42);
    resB.addProperty(QUrl("prop:/int3"), 50);

    // merge the resource
    m_dmModel->storeResources(SimpleResourceGraph() << resB, QLatin1String("B"));
    QVERIFY( !m_dmModel->lastError() );

    // make sure the new data is there
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/int3"), LiteralValue(50)));

    // make sure creation date did not change, ie. it was not created as that would be wrong
    QVERIFY(!m_model->containsAnyStatement(resA, NAO::created(), Node()));

    // make sure the last mtime has been updated
    QCOMPARE(m_model->listStatements(resA, NAO::lastModified(), Node()).allStatements().count(), 1);
    QVERIFY(m_model->listStatements(resA, NAO::lastModified(), Node()).iterateObjects().allNodes().first().literal().toDateTime() > newDt);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// test merging when there is more than one candidate resource to merge with
void DataManagementModelTest::testStoreResources_multiMerge()
{
    // create two resource which could be matches for the one we will store
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    // the resource in which we want to merge
    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resA, NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), ng);

    QUrl resB("nepomuk:/res/B");
    m_model->addStatement(resB, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resB, RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(resB, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resB, NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(resB, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), ng);


    // now store the exact same resource
    SimpleResource res;
    res.addProperty(RDF::type(), NAO::Tag());
    res.addProperty(QUrl("prop:/int"), 42);
    res.addProperty(QUrl("prop:/string"), QLatin1String("foobar"));

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // make sure no new resource was created
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/int"), LiteralValue(42)).iterateSubjects().allNodes().toSet().count(), 2);
    QCOMPARE(m_model->listStatements(Node(), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))).iterateSubjects().allNodes().toSet().count(), 2);

    // make sure both resources still exist
    QVERIFY(m_model->containsAnyStatement(resA, Node(), Node()));
    QVERIFY(m_model->containsAnyStatement(resB, Node(), Node()));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// an example from real-life which made an early version of DMS fail
void DataManagementModelTest::testStoreResources_realLife()
{
    // we deal with one file
    QTemporaryFile theFile;
    theFile.open();

    // the full data - slightly cleanup up (excluding additional video track resources)
    // this data is a combination from the file indexing service and DMS storeResources calls

    // the resource URIs used
    const QUrl fileResUri("nepomuk:/res/3ff603a5-4023-4c2f-bd89-372002a0ffd2");
    const QUrl tvSeriesUri("nepomuk:/res/e6fbe22d-bb5c-416c-a935-407a34b58c76");
    const QUrl appRes("nepomuk:/res/275907b0-c120-4581-83d5-ea9ec034dbcd");

    // the graph URIs
    // two strigi graphs due to the nie:url preservation
    const QUrl strigiG1("nepomuk:/ctx/5ca62cd0-ccff-4484-99e3-1fd9f782a3a4");
    const QUrl strigiG2("nepomuk:/ctx/9f0bac21-f8e4-4e82-b51d-e0a7585f1c8d");
    const QUrl strigiMG1("nepomuk:/ctx/0117104c-d501-48ce-badd-6f363bfde3e2");
    const QUrl strigiMG2("nepomuk:/ctx/e52c8b8a-2e32-4a27-8633-03f27fec441b");

    const QUrl dmsG1("nepomuk:/ctx/8bea556f-cacf-4f31-be73-7f7c0f14024b");
    const QUrl dmsG2("nepomuk:/ctx/374d3968-0d20-4807-8a87-d2d6b87e7de3");
    const QUrl dmsMG1("nepomuk:/ctx/9902847f-dfe8-489a-881b-4abf1707fee7");
    const QUrl dmsMG2("nepomuk:/ctx/72ef2cdf-26e7-42b6-9093-0b7b0a7c25fc");

    const QUrl appG1("nepomuk:/ctx/7dc9f013-4e45-42bf-8595-a12e78adde81");
    const QUrl appMG1("nepomuk:/ctx/1ffcb2bb-525d-4173-b211-ebdf28c0897b");

    const QUrl ng = m_dmModel->nepomukGraph();

    // strings we reuse
    const QString seriesTitle("Nepomuk The Series");
    const QString episodeTitle("Who are you?");
    const QString seriesOverview("Nepomuk is a series about information and the people needing this information for informational purposes.");
    const QString episodeOverview("The series pilot focusses on this and that, nothing in particular and it not really that interesting at all.");

    // the file resource
    m_model->addStatement(fileResUri, NIE::isPartOf(), QUrl("nepomuk:/res/e9f85f29-150d-49b6-9ffb-264ae7ec3864"), strigiG1);
    m_model->addStatement(fileResUri, NIE::contentSize(), Soprano::LiteralValue::fromString("369532928", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NIE::mimeType(), Soprano::LiteralValue::fromString("audio/x-riff", XMLSchema::string()), strigiG1);
    m_model->addStatement(fileResUri, NIE::mimeType(), Soprano::LiteralValue::fromString("video/x-msvideo", XMLSchema::string()), strigiG1);
    m_model->addStatement(fileResUri, NFO::fileName(), Soprano::LiteralValue(KUrl(theFile.fileName()).fileName()), strigiG1);
    m_model->addStatement(fileResUri, NIE::lastModified(), Soprano::LiteralValue::fromString("2010-06-29T15:44:44Z", XMLSchema::dateTime()), strigiG1);
    m_model->addStatement(fileResUri, NFO::codec(), Soprano::LiteralValue::fromString("MP3", XMLSchema::string()), strigiG1);
    m_model->addStatement(fileResUri, NFO::codec(), Soprano::LiteralValue::fromString("xvid", XMLSchema::string()), strigiG1);
    m_model->addStatement(fileResUri, NFO::averageBitrate(), Soprano::LiteralValue::fromString("1132074", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NFO::duration(), Soprano::LiteralValue::fromString("2567", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NFO::duration(), Soprano::LiteralValue::fromString("2611", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NFO::frameRate(), Soprano::LiteralValue::fromString("23", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NIE::hasPart(), QUrl("nepomuk:/res/b805e3bb-db13-4561-b457-8da8d13ce34d"), strigiG1);
    m_model->addStatement(fileResUri, NIE::hasPart(), QUrl("nepomuk:/res/c438df3c-1446-4931-9d9e-3665567025b9"), strigiG1);
    m_model->addStatement(fileResUri, NFO::horizontalResolution(), Soprano::LiteralValue::fromString("624", XMLSchema::xsdInt()), strigiG1);
    m_model->addStatement(fileResUri, NFO::verticalResolution(), Soprano::LiteralValue::fromString("352", XMLSchema::xsdInt()), strigiG1);

    m_model->addStatement(fileResUri, RDF::type(), NFO::FileDataObject(), strigiG2);
    m_model->addStatement(fileResUri, NIE::url(), QUrl::fromLocalFile(theFile.fileName()), ng);

    m_model->addStatement(fileResUri, RDF::type(), NMM::TVShow(), dmsG1);
    m_model->addStatement(fileResUri, NAO::created(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.317Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(fileResUri, NIE::title(), Soprano::LiteralValue(episodeTitle), dmsG1);
    m_model->addStatement(fileResUri, NAO::lastModified(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.317Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(fileResUri, NMM::synopsis(), Soprano::LiteralValue(episodeOverview), dmsG1);
    m_model->addStatement(fileResUri, NMM::series(), tvSeriesUri, dmsG1);
    m_model->addStatement(fileResUri, NMM::season(), Soprano::LiteralValue::fromString("1", XMLSchema::xsdInt()), dmsG1);
    m_model->addStatement(fileResUri, NMM::episodeNumber(), Soprano::LiteralValue::fromString("1", XMLSchema::xsdInt()), dmsG1);
    m_model->addStatement(fileResUri, RDF::type(), NIE::InformationElement(), dmsG2);
    m_model->addStatement(fileResUri, RDF::type(), NFO::Video(), dmsG2);

    // the TV Series resource
    m_model->addStatement(tvSeriesUri, RDF::type(), NMM::TVSeries(), dmsG1);
    m_model->addStatement(tvSeriesUri, NAO::created(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.317Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(tvSeriesUri, NIE::title(), Soprano::LiteralValue(seriesTitle), dmsG1);
    m_model->addStatement(tvSeriesUri, NAO::lastModified(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.317Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(tvSeriesUri, NIE::description(), Soprano::LiteralValue(seriesOverview), dmsG1);
    m_model->addStatement(tvSeriesUri, NMM::hasEpisode(), fileResUri, dmsG1);
    m_model->addStatement(tvSeriesUri, RDF::type(), NIE::InformationElement(), dmsG2);

    // the app that called storeResources
    m_model->addStatement(appRes, RDF::type(), NAO::Agent(), appG1);
    m_model->addStatement(appRes, NAO::prefLabel(), Soprano::LiteralValue::fromString("Nepomuk TVNamer", XMLSchema::string()), appG1);
    m_model->addStatement(appRes, NAO::identifier(), Soprano::LiteralValue::fromString("nepomuktvnamer", XMLSchema::string()), appG1);

    // all the graph metadata
    m_model->addStatement(strigiG1, RDF::type(), NRL::DiscardableInstanceBase(), strigiMG1);
    m_model->addStatement(strigiG1, NAO::created(), Soprano::LiteralValue::fromString("2010-10-22T14:13:42.204Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(strigiG1, QUrl("http://www.strigi.org/fields#indexGraphFor"), fileResUri, strigiMG1);
    m_model->addStatement(strigiMG1, RDF::type(), NRL::GraphMetadata(), strigiMG1);
    m_model->addStatement(strigiMG1, NRL::coreGraphMetadataFor(), strigiG1, strigiMG1);

    m_model->addStatement(strigiG2, RDF::type(), NRL::InstanceBase(), strigiMG2);
    m_model->addStatement(strigiG2, NAO::created(), Soprano::LiteralValue::fromString("2010-10-22T14:13:42.204Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(strigiG2, QUrl("http://www.strigi.org/fields#indexGraphFor"), fileResUri, strigiMG2);
    m_model->addStatement(strigiG2, NAO::maintainedBy(), appRes, strigiMG2);
    m_model->addStatement(strigiMG2, RDF::type(), NRL::GraphMetadata(), strigiMG2);
    m_model->addStatement(strigiMG2, NRL::coreGraphMetadataFor(), strigiG2, strigiMG2);

    m_model->addStatement(dmsG1, RDF::type(), NRL::InstanceBase(), dmsMG1);
    m_model->addStatement(dmsG1, NAO::created(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.343Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(dmsG1, NAO::maintainedBy(), appRes, dmsMG1);
    m_model->addStatement(dmsMG1, RDF::type(), NRL::GraphMetadata(), dmsMG1);
    m_model->addStatement(dmsMG1, NRL::coreGraphMetadataFor(), dmsG1, dmsMG1);

    m_model->addStatement(dmsG2, RDF::type(), NRL::InstanceBase(), dmsMG2);
    m_model->addStatement(dmsG2, NAO::created(), Soprano::LiteralValue::fromString("2011-03-14T10:06:38.621Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(dmsG2, NAO::maintainedBy(), appRes, dmsMG2);
    m_model->addStatement(dmsMG2, RDF::type(), NRL::GraphMetadata(), dmsMG2);
    m_model->addStatement(dmsMG2, NRL::coreGraphMetadataFor(), dmsG2, dmsMG2);

    m_model->addStatement(appG1, RDF::type(), NRL::InstanceBase(), appMG1);
    m_model->addStatement(appG1, NAO::created(), Soprano::LiteralValue::fromString("2011-03-12T17:48:44.307Z", XMLSchema::dateTime()), ng);
    m_model->addStatement(appMG1, RDF::type(), NRL::GraphMetadata(), appMG1);
    m_model->addStatement(appMG1, NRL::coreGraphMetadataFor(), appG1, appMG1);


    // remember current state to compare later on
    Soprano::Graph existingStatements = m_model->listStatements().allStatements();


    // now the TV show information is stored again
    SimpleResourceGraph graph;
    SimpleResource tvShowRes(QUrl::fromLocalFile(theFile.fileName()));
    tvShowRes.addProperty(RDF::type(), NMM::TVShow());
    tvShowRes.addProperty(NMM::episodeNumber(), 1);
    tvShowRes.addProperty(NMM::season(), 1);
    tvShowRes.addProperty(NIE::title(), episodeTitle);
    tvShowRes.addProperty(NMM::synopsis(), episodeOverview);

    SimpleResource tvSeriesRes;
    tvSeriesRes.addProperty(RDF::type(), NMM::TVSeries());
    tvSeriesRes.addProperty(NIE::title(), seriesTitle);
    tvSeriesRes.addProperty(NIE::description(), seriesOverview);
    tvSeriesRes.addProperty(NMM::hasEpisode(), tvShowRes.uri());

    tvShowRes.addProperty(NMM::series(), tvSeriesRes.uri());

    graph << tvShowRes << tvSeriesRes;

    m_dmModel->storeResources(graph, QLatin1String("nepomuktvnamer"));
    QVERIFY(!m_dmModel->lastError());

    // now test the data - nothing should have changed at all
    // no data should have been changed
    QSet<Statement> newStatements = m_model->listStatements().allStatements().toSet();
    foreach(const Soprano::Statement& st, newStatements) {
        if( st.predicate() != NAO::lastModified() )
            // WARNING: The graphs might have changed!
            QVERIFY( existingStatements.containsAnyStatement(st.subject(), st.predicate(), st.object()) );
    }

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_trivialMerge()
{
    // we create a resource with some properties
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resA, NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), ng);


    // now we store a trivial resource
    SimpleResource res;
    res.addProperty(RDF::type(), QUrl("class:/typeA"));

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("A"));
    QVERIFY( !m_dmModel->lastError() );

    // the two resources should NOT have been merged
    QCOMPARE(m_model->listStatements(Node(), RDF::type(), QUrl("class:/typeA")).allElements().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that two resources are not merged if they have no matching type even if the rest of the idenfifying props match.
// the merged resource does not have any type
void DataManagementModelTest::testStoreResources_noTypeMatch1()
{
    // we create a resource with some properties
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resA, NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), ng);

    // now we store the resource without a type
    SimpleResource res;
    res.addProperty(QUrl("prop:/int"), 42);
    res.addProperty(QUrl("prop:/string"), QLatin1String("foobar"));

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("A"));
    QVERIFY( !m_dmModel->lastError() );

    // the two resources should NOT have been merged - we should have a new resource
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/int"), Soprano::LiteralValue(42)).allStatements().count(), 2);
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/string"), Soprano::LiteralValue(QLatin1String("foobar"))).allStatements().count(), 2);

    // two different subjects
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/int"), Soprano::LiteralValue(42)).iterateSubjects().allNodes().toSet().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that two resources are not merged if they have no matching type even if the rest of the idenfifying props match.
// the merged resource has a different type than the one in store
void DataManagementModelTest::testStoreResources_noTypeMatch2()
{
    // we create a resource with some properties
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resA, NAO::created(), LiteralValue(QDateTime::currentDateTime()), ng);
    m_model->addStatement(resA, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), ng);

    // now we store the resource with a different type
    SimpleResource res;
    res.addType(QUrl("class:/typeB"));
    res.addProperty(QUrl("prop:/int"), 42);
    res.addProperty(QUrl("prop:/string"), QLatin1String("foobar"));

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("A"));
    QVERIFY( !m_dmModel->lastError() );

    // the two resources should NOT have been merged - we should have a new resource
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/int"), Soprano::LiteralValue(42)).allStatements().count(), 2);
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/string"), Soprano::LiteralValue(QLatin1String("foobar"))).allStatements().count(), 2);

    // two different subjects
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/int"), Soprano::LiteralValue(42)).iterateSubjects().allNodes().toSet().count(), 2);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_faultyMetadata()
{
    KTemporaryFile file;
    file.open();
    const QUrl fileUrl( file.fileName() );

    SimpleResource res;
    res.addProperty( RDF::type(), NFO::FileDataObject() );
    res.addProperty( NIE::url(), fileUrl );
    res.addProperty( NAO::lastModified(), QVariant( 5 ) );
    res.addProperty( NAO::created(), QVariant(QLatin1String("oh no") ) );

    QList<Soprano::Statement> list = m_model->listStatements().allStatements();
    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp") );

    // The should be an error
    QVERIFY(m_dmModel->lastError());

    // And the statements should not exist
    QVERIFY(!m_model->containsAnyStatement( Node(), RDF::type(), NFO::FileDataObject() ));
    QVERIFY(!m_model->containsAnyStatement( Node(), NIE::url(), fileUrl ));
    QVERIFY(!m_model->containsAnyStatement( Node(), NAO::lastModified(), Node() ));

    QList<Soprano::Statement> list2 = m_model->listStatements().allStatements();
    QCOMPARE( list, list2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_additionalMetadataApp()
{
    KTemporaryFile file;
    file.open();
    const QUrl fileUrl( file.fileName() );

    SimpleResource res;
    res.addProperty( RDF::type(), NFO::FileDataObject() );
    res.addProperty( NIE::url(), fileUrl );

    SimpleResource app;
    app.addProperty( RDF::type(), NAO::Agent() );
    app.addProperty( NAO::identifier(), "appB" );

    SimpleResourceGraph g;
    g << res << app;

    QHash<QUrl, QVariant> additionalMetadata;
    additionalMetadata.insert( NAO::maintainedBy(), app.uri() );

    m_dmModel->storeResources( g, QLatin1String("appA"), Nepomuk2::IdentifyNew, Nepomuk2::NoStoreResourcesFlags, additionalMetadata );

    // We no longer support anything but type discardable
    // Maybe this test should be thrown away?
    QEXPECT_FAIL("", "We no longer support additional Metadata", Abort);

    //FIXME: for now this should fail as nao:maintainedBy is protected,
    //       but what if we want to add some additionalMetadata which references
    //       a node in the SimpleResourceGraph. There needs to be a test for that.
    QVERIFY(m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_itemUris()
{
    SimpleResourceGraph g;

    for (int i = 0; i < 10; i++) {
        QUrl uri( "testuri:?item="+QString::number(i) );
        SimpleResource r(uri);
        r.addType( NIE::DataObject() );
        r.addType( NIE::InformationElement() );

        QString label = QLatin1String("label") + QString::number(i);
        r.setProperty( NAO::prefLabel(), label );
        g.insert(r);
    }

    m_dmModel->storeResources( g, "app" );

    // Should give an error 'testuri' is an unknown protocol
    QVERIFY(m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_kioProtocols()
{
    QStringList protocolList = KProtocolInfo::protocols();
    protocolList.removeAll( QLatin1String("nepomuk") );
    protocolList.removeAll( QLatin1String("file") );

    kDebug() << "List: " << protocolList;
    foreach( const QString& protocol, protocolList ) {
        SimpleResource res( QUrl(protocol + ":/item") );
        res.addType( NFO::FileDataObject() );
        res.addType( NMM::MusicPiece() );

        m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app") );
        QVERIFY(!m_dmModel->lastError());

        QVERIFY( m_model->containsAnyStatement( Node(), NIE::url(), res.uri() ) );

        const QUrl resUri = m_model->listStatements( Node(), NIE::url(), res.uri() ).allStatements().first().subject().uri();

        QVERIFY( m_model->containsAnyStatement( resUri, RDF::type(), NFO::FileDataObject() ) );
        QVERIFY( m_model->containsAnyStatement( resUri, RDF::type(), NMM::MusicPiece() ) );
    }

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_duplicates()
{
    KTemporaryFile file;
    file.open();
    const QUrl fileUrl( file.fileName() );

    SimpleResource res;
    res.addType( NFO::FileDataObject() );
    res.addProperty( NIE::url(), fileUrl );

    SimpleResource hash1;
    hash1.addType( NFO::FileHash() );
    hash1.addProperty( NFO::hashAlgorithm(), QLatin1String("SHA1") );
    hash1.addProperty( NFO::hashValue(), QLatin1String("ddaa6b339428b75ee1545f80f1f35fb89c166bf9") );

    SimpleResource hash2;
    hash2.addType( NFO::FileHash() );
    hash2.addProperty( NFO::hashAlgorithm(), QLatin1String("SHA1") );
    hash2.addProperty( NFO::hashValue(), QLatin1String("ddaa6b339428b75ee1545f80f1f35fb89c166bf9") );

    res.addProperty( NFO::hasHash(), hash1.uri() );
    res.addProperty( NFO::hasHash(), hash2.uri() );

    SimpleResourceGraph graph;
    graph << res << hash1 << hash2;

    m_dmModel->storeResources( graph, "appA", Nepomuk2::IdentifyNew, Nepomuk2::MergeDuplicateResources );
    QVERIFY(!m_dmModel->lastError());

    // hash1 and hash2 are the same, they should have been merged together
    int hashCount = m_model->listStatements( Node(), RDF::type(), NFO::FileHash() ).allStatements().size();
    QCOMPARE( hashCount, 1 );

    // res should have only have one hash1
    QCOMPARE( m_model->listStatements( Node(), NFO::hasHash(), Node() ).allStatements().size(), 1 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_duplicateHierarchy()
{
    SimpleResource contact1;
    contact1.addType( NCO::Contact() );
    contact1.addProperty( NCO::fullname(), QLatin1String("Spiderman") );
    contact1.addProperty( NAO::prefLabel(), QLatin1String("test") );

    SimpleResource email1;
    email1.addType(NCO::EmailAddress());
    email1.addProperty(NCO::emailAddress(), QLatin1String("email@foo.com"));
    contact1.addProperty(NCO::hasEmailAddress(), email1.uri());

    SimpleResource contact2;
    contact2.addType( NCO::Contact() );
    contact2.addProperty( NCO::fullname(), QLatin1String("Spiderman") );
    contact2.addProperty( NAO::prefLabel(), QLatin1String("test") );

    SimpleResource email2;
    email2.addType(NCO::EmailAddress());
    email2.addProperty(NCO::emailAddress(), QLatin1String("email@foo.com"));
    contact2.addProperty(NCO::hasEmailAddress(), email2.uri());

    SimpleResourceGraph graph;
    graph << email1 << contact1 << email2 << contact2;

    m_dmModel->storeResources( graph, "appA", Nepomuk2::IdentifyNew, Nepomuk2::MergeDuplicateResources );
    QVERIFY(!m_dmModel->lastError());

    int contactCount = m_model->listStatements( Node(), RDF::type(),
                                                NCO::Contact() ).allStatements().size();
    QCOMPARE( contactCount, 1 );

    int emailCount = m_model->listStatements( Node(), RDF::type(),
                                              NCO::EmailAddress() ).allStatements().size();
    QCOMPARE( emailCount, 1 );

    QCOMPARE( m_model->listStatements( Node(), NCO::fullname(), Node()
                                       ).allStatements().size(), 1 );
    QCOMPARE( m_model->listStatements( Node(), NAO::prefLabel(), Node()
                                       ).allStatements().size(), 1 );

}

void DataManagementModelTest::testStoreResources_duplicates2()
{
    SimpleResourceGraph graph;

    for( int i=0; i<2; i++ ) {
        SimpleResource contact;
        contact.addType( NCO::Contact() );
        contact.setProperty( NCO::fullname(), QLatin1String("Peter") );

        SimpleResource email;
        email.addType( NCO::EmailAddress() );
        email.addProperty( NCO::emailAddress(), QUrl("peter@parker.com") );

        contact.addProperty( NCO::hasEmailAddress(), email );

        graph << contact << email;
    }

    m_dmModel->storeResources( graph, "appA", Nepomuk2::IdentifyNew, Nepomuk2::MergeDuplicateResources );
    QVERIFY(!m_dmModel->lastError());

    // There should only be one email and one contact
    int contactCount = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).allStatements().size();
    QCOMPARE( contactCount, 1 );

    int emailCount = m_model->listStatements( Node(), RDF::type(), NCO::EmailAddress() ).allStatements().size();
    QCOMPARE( emailCount, 1 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_duplicates3()
{
    SimpleResource contact;
    contact.addType( NCO::Contact() );
    contact.setProperty( NCO::fullname(), QLatin1String("Peter") );

    QHash<QUrl, QUrl> map = m_dmModel->storeResources( SimpleResourceGraph() << contact, QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());
    const QUrl contactUri  = map.value( contact.uri() );

    SimpleResourceGraph graph;
    SimpleResource con1( contactUri );
    con1.addType( NCO::Contact() );
    con1.addProperty( NCO::gender(), NCO::male() );

    SimpleResource con2;
    con2.addType( NCO::Contact() );
    con2.addProperty( NCO::gender(), NCO::male() );

    // These 2 contacts are the same but they shouldn't get merged cause one is not a blank uri
    graph << con1 << con2;
    map = m_dmModel->storeResources( graph, QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());

    QList< Node > nodeList = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).iterateSubjects().allNodes();
    QCOMPARE( nodeList.size(), 2 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_duplicatesInMerger()
{
    SimpleResource contact1;
    contact1.addType( NCO::PersonContact() );
    contact1.setProperty( NCO::fullname(), QLatin1String("Rachel McAdams") );

    SimpleResourceGraph graph;
    graph << contact1;

    m_dmModel->storeResources( graph, QLatin1String("appA") );
    QVERIFY(!m_dmModel->lastError());

    SimpleResource contact2;
    contact2.addType( NCO::PersonContact() );
    contact2.setProperty( NCO::fullname(), QLatin1String("Rachel McAdams") );
    contact2.setProperty( NAO::prefLabel(), QLatin1String("Rachel McAdams") );

    graph << contact2;

    m_dmModel->storeResources( graph, QLatin1String("appA") );
    QVERIFY(!m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_overwriteProperties()
{
    SimpleResource contact;
    contact.addType( NCO::Contact() );
    contact.addProperty( NCO::fullname(), QLatin1String("Spiderman") );

    m_dmModel->storeResources( SimpleResourceGraph() << contact, QLatin1String("app") );
    QVERIFY( !m_dmModel->lastError() );

    QList< Statement > stList = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl resUri = stList.first().subject().uri();

    SimpleResource contact2( resUri );
    contact2.addType( NCO::Contact() );
    contact2.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    //m_dmModel->storeResources( SimpleResourceGraph() << contact2, QLatin1String("app") );
    //QVERIFY( m_dmModel->lastError() ); // should fail without the merge flags

    // Now everyone will know who Spiderman really is
    m_dmModel->storeResources( SimpleResourceGraph() << contact2, QLatin1String("app"), IdentifyNew, OverwriteProperties );
    QVERIFY( !m_dmModel->lastError() );

    stList = m_model->listStatements( resUri, NCO::fullname(), Node() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QString newName = stList.first().object().literal().toString();
    QCOMPARE( newName, QLatin1String("Peter Parker") );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_overwriteProperties_cardinality()
{
    SimpleResource contact;
    contact.addType( NCO::Contact() );
    contact.addProperty( NCO::fullname(), QLatin1String("Spiderman") );

    m_dmModel->storeResources( SimpleResourceGraph() << contact, QLatin1String("app") );
    QVERIFY( !m_dmModel->lastError() );

    QList< Statement > stList = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl resUri = stList.first().subject().uri();

    SimpleResource contact2( resUri );
    contact2.addType( NCO::Contact() );
    contact2.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    contact2.addProperty( NCO::fullname(), QLatin1String("The Amazing Spiderman") );

    m_dmModel->storeResources( SimpleResourceGraph() << contact2, QLatin1String("app"), IdentifyNew, OverwriteProperties );

    // There should be an error since contact2 has two fullnames
    QVERIFY( m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_overwriteAllProperties()
{
    SimpleResource tag1;
    tag1.addType( NAO::Tag() );
    tag1.setProperty( NAO::prefLabel(), "Tag1" );

    SimpleResource tag2;
    tag2.addType( NAO::Tag() );
    tag2.setProperty( NAO::prefLabel(), "Tag2" );

    SimpleResource tag3;
    tag3.addType( NAO::Tag() );
    tag3.setProperty( NAO::prefLabel(), "Tag3" );

    SimpleResource res;
    res.addType( RDFS::Resource() );
    res.addProperty( NAO::hasTag(), tag1 );

    QHash< QUrl, QUrl > map = m_dmModel->storeResources( SimpleResourceGraph() << res << tag1, QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());

    SimpleResource res2( map.value(res.uri()) );
    res2.addType( RDFS::Resource() );
    res2.addProperty( NAO::hasTag(), tag2 );
    res2.addProperty( NAO::hasTag(), tag3 );

    SimpleResourceGraph graph;
    graph << tag2 << tag3 << res2;

    map = m_dmModel->storeResources( graph, QLatin1String("app2"), Nepomuk2::IdentifyNew, Nepomuk2::OverwriteAllProperties );
    QVERIFY(!m_dmModel->lastError());

    kDebug() << "TAG2: " << m_model->containsAnyStatement( res2.uri(), NAO::hasTag(), map.value(tag2.uri()) );
    kDebug() << "TAG3: " << m_model->containsAnyStatement( res2.uri(), NAO::hasTag(), map.value(tag3.uri()) );

    QList<Node> objects = m_model->listStatements( res2.uri(), NAO::hasTag(), QUrl() ).iterateObjects().allNodes();
    kDebug() << objects;
    QCOMPARE( objects.size(), 2 );

    QVERIFY( objects.contains( map.value(tag2.uri()) ) );
    QVERIFY( objects.contains( map.value(tag3.uri()) ) );
}


// make sure that already existing resource types are taken into account for domain checks
void DataManagementModelTest::testStoreResources_correctDomainInStore()
{
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    // create the resource
    const QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), NMM::MusicPiece(), g1);
    m_model->addStatement(resA, NAO::lastModified(), Soprano::LiteralValue(QDateTime::currentDateTime()), ng);

    // now store a music piece with a performer.
    // the performer does not have a type in the simple res but only in store
    SimpleResource piece(resA);
    piece.addProperty(NIE::title(), QLatin1String("Hello World"));
    SimpleResource artist;
    artist.addType(NCO::Contact());
    artist.addProperty(NCO::fullname(), QLatin1String("foobar"));
    piece.addProperty(NMM::performer(), artist);

    m_dmModel->storeResources(SimpleResourceGraph() << piece << artist, QLatin1String("testapp"));

    QVERIFY(!m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_correctDomainInStore2()
{
    SimpleResource res;
    res.addType( NMM::MusicPiece() );
    res.addType( QUrl("class:/typeA") );
    res.addProperty( NIE::title(), QLatin1String("Music") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp") );
    QVERIFY( !m_dmModel->lastError() );

    QList<Soprano::Statement> stList = m_model->listStatements( Node(), RDF::type(), QUrl("class:/typeA") ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl resUri = stList.first().subject().uri();

    SimpleResource musicPiece;
    musicPiece.addType( QUrl("class:/typeA") );
    // We're not giving it a nmm:MusicPiece type
    musicPiece.addProperty( NIE::title(), QLatin1String("Music") );

    SimpleResource artist;
    artist.addType( NCO::Contact() );
    artist.addProperty( NCO::fullname(), QLatin1String("Snow Patrol") );

    // nmm:performer has a domain of nmm:MusicPiece which is already present in the store
    musicPiece.addProperty( NMM::performer(), artist );

    m_dmModel->storeResources( SimpleResourceGraph() << musicPiece << artist,
                               QLatin1String("testApp") );
    QVERIFY( !m_dmModel->lastError() );

    // musicPiece should have gotten identified as res
    stList = m_model->listStatements( Node(), RDF::type(), QUrl("class:/typeA") ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl musicPieceUri = stList.first().subject().uri();
    QCOMPARE( musicPieceUri, resUri );

    // It should have the artist
    QVERIFY( m_model->containsAnyStatement( musicPieceUri, NMM::performer(), Node() ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that already existing resource types are taken into account for range checks
void DataManagementModelTest::testStoreResources_correctRangeInStore()
{
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl ng = m_dmModel->nepomukGraph();

    // create the resource
    const QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, RDF::type(), NCO::Contact(), g1);
    m_model->addStatement(resA, NAO::lastModified(), Soprano::LiteralValue(QDateTime::currentDateTime()), ng);

    // now store a music piece with a performer.
    // the performer does not have a type in the simple res but only in store
    SimpleResource piece;
    piece.addType(NMM::MusicPiece());
    piece.addProperty(NIE::title(), QLatin1String("Hello World"));
    SimpleResource artist(resA);
    artist.addProperty(NCO::fullname(), QLatin1String("foobar"));
    piece.addProperty(NMM::performer(), artist);

    m_dmModel->storeResources(SimpleResourceGraph() << piece << artist, QLatin1String("testapp"));

    QVERIFY(!m_dmModel->lastError());

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_correctRangeInStore2()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addType( QUrl("class:/typeA") );
    res.addProperty( NCO::fullname(), QLatin1String("Jack Black") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp") );
    QVERIFY( !m_dmModel->lastError() );

    QList<Soprano::Statement> stList = m_model->listStatements( Node(), RDF::type(), QUrl("class:/typeA") ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl resUri = stList.first().subject().uri();

    SimpleResource musicPiece;
    musicPiece.addType( NFO::FileDataObject() );
    musicPiece.addType( NMM::MusicPiece() );
    musicPiece.addProperty( NIE::title(), QLatin1String("Music") );

    SimpleResource artist;
    artist.addType( QUrl("class:/typeA") );
    // We're not giving it the type NCO::Contact - should be inferred from the store
    artist.addProperty( NCO::fullname(), QLatin1String("Jack Black") );

    // nmm:performer has a range of nco:Contact which is already present in the store
    musicPiece.addProperty( NMM::performer(), artist );

    m_dmModel->storeResources( SimpleResourceGraph() << musicPiece << artist,
                               QLatin1String("testApp") );
    QVERIFY( !m_dmModel->lastError() );

    // artist should have gotten identified as res
    stList = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl artistUri = stList.first().subject().uri();
    QCOMPARE( artistUri, resUri );

    // It should have the artist
    QVERIFY( m_model->containsAnyStatement( Node(), NMM::performer(), artistUri ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that the same values are simply merged even if encoded differently
void DataManagementModelTest::testStoreResources_duplicateValuesAsString()
{
    SimpleResource res;

    // add the same type twice
    res.addType(QUrl("class:/typeA"));
    res.addProperty(RDF::type(), QLatin1String("class:/typeA"));

    // add the same value twice
    res.addProperty(QUrl("prop:/int"), 42);
    res.addProperty(QUrl("prop:/int"), QLatin1String("42"));

    // now add the resource
    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));

    // this should succeed
    QVERIFY(!m_dmModel->lastError());

    // make sure all is well
    QCOMPARE(m_model->listStatements(Soprano::Node(), RDF::type(), QUrl("class:/typeA")).allStatements().count(), 1);
    QCOMPARE(m_model->listStatements(Soprano::Node(), QUrl("prop:/int"), LiteralValue(42)).allStatements().count(), 1);

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_ontology()
{
    SimpleResource res( NFO::FileDataObject() );
    res.addType( NCO::Contact() );

    m_dmModel->storeResources(SimpleResourceGraph() << res, QLatin1String("testapp"));

    // There should be some error, we're trying to set an ontology
    QVERIFY( m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testStoreResources_legacyUris()
{
    const QUrl uri("res:/A");

    const QUrl graphUri = m_nrlModel->createGraph( NRL::InstanceBase() );
    m_model->addStatement( uri, RDF::type(), NFO::FileDataObject(), graphUri );
    m_model->addStatement( uri, RDF::type(), NFO::Folder(), graphUri );
    m_model->addStatement( uri, NAO::numericRating(), LiteralValue(5), graphUri );

    SimpleResource res( uri );
    res.addType( NFO::Folder() );
    res.addType( NFO::FileDataObject() );
    res.addProperty( NAO::numericRating(), QLatin1String("5") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app"), IdentifyNew, OverwriteProperties );
    QVERIFY( !m_dmModel->lastError() );

    QVERIFY( m_model->containsAnyStatement( uri, NAO::numericRating(), LiteralValue(5) ) );

    SimpleResource res2;
    res2.addType( NFO::FileDataObject() );
    res2.addProperty( NIE::isPartOf(), uri );

    m_dmModel->storeResources( SimpleResourceGraph() << res2, QLatin1String("app") );
    QVERIFY( !m_dmModel->lastError() );

    QVERIFY( m_model->containsAnyStatement( Node(), NIE::isPartOf(), uri ) );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_lazyCardinalities()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Superman") );
    res.addProperty( NCO::fullname(), QLatin1String("Clark Kent") ); // Don't tell Lex!

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp"),
                               Nepomuk2::IdentifyNew, Nepomuk2::LazyCardinalities );

    // There shouldn't be any error, even though nco:fullname has maxCardinality = 1
    QVERIFY( !m_dmModel->lastError() );

    QList< Statement > stList = m_model->listStatements( Node(), NCO::fullname(), Node() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    QString name = stList.first().object().literal().toString();
    bool isClark = ( name == QLatin1String("Clark Kent") );
    bool isSuperMan = ( name == QLatin1String("Superman") );

    QVERIFY( isClark || isSuperMan );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_graphMetadataFail()
{
    QList<Soprano::Statement> stList = m_model->listStatements().allStatements();

    QHash<QUrl, QVariant> additionalMetadata;
    additionalMetadata.insert( NCO::fullname(), QLatin1String("graphs can't have names") );

    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Harry Potter") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp"),
                               IdentifyNew, NoStoreResourcesFlags, additionalMetadata );

    // There should be an error as graphs cannot have NFO::FileDataObject
    QEXPECT_FAIL("", "StoreResources no longer supports complex additionalMetadata", Abort);
    QVERIFY( m_dmModel->lastError() );

    // Nothing should have changed
    QList<Soprano::Statement> newStList = m_model->listStatements().allStatements();
    QCOMPARE( stList, newStList );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_randomNepomukUri()
{
    SimpleResource res(QUrl("nepomuk:/res/random-uri"));
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Mickey Mouse") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp") );

    // There should be an error - We do not allow creation of arbitrary uris
    // All uris must be created by the DataManagementModel
    QVERIFY( m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_legacyData()
{
    // create some legacy data
    QTemporaryFile file;
    file.open();
    const KUrl url(file.fileName());

    const QUrl g = m_nrlModel->createGraph(NRL::InstanceBase());

    m_model->addStatement(url, QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(url, RDF::type(), NFO::FileDataObject(), g);

    // set some data with the url
    SimpleResource res( url );
    res.addType( NFO::FileDataObject() );
    res.addProperty( QUrl("prop:/int"), 42 );
    res.addProperty( QUrl("prop:/int2"), 50 );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app") );

    // make sure the resource has changed
    QSet<Node> nodeSet = m_model->listStatements( Node(), RDF::type(), NFO::FileDataObject() ).iterateSubjects().allNodes().toSet();
    QCOMPARE( nodeSet.size(), 1 );

    nodeSet = m_model->listStatements( Node(), QUrl("prop:/int2"), LiteralValue(50) ).iterateSubjects().allNodes().toSet();
    QCOMPARE( nodeSet.size(), 1 );

    nodeSet = m_model->listStatements( Node(), QUrl("prop:/int"), LiteralValue(42) ).iterateSubjects().allNodes().toSet();
    QCOMPARE( nodeSet.size(), 1 );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_missingBlankNode()
{
    SimpleResource album;
    album.addType( NMM::MusicAlbum() );
    album.setProperty( NIE::title(), QLatin1String("Some album") );

    SimpleResource artist;
    artist.addType( NCO::Contact() );
    artist.addProperty( NCO::fullname(), QLatin1String("Coldplay") );

    QTemporaryFile file;
    file.open();

    SimpleResource res(KUrl(file.fileName()));
    res.addProperty( NMM::musicAlbum(), album );
    res.addProperty( NMM::performer(), artist );

    SimpleResourceGraph graph;
    // Do not add the album
    graph << res << artist;

    m_dmModel->storeResources( graph, QLatin1String("testApp") );

    // It should have screamed that album hasn't been added
    QVERIFY( m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

void DataManagementModelTest::testStoreResources_graphChecks()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("John Coner") );

    const QUrl graph = m_nrlModel->createGraph( NRL::InstanceBase() );
    m_model->addStatement( QUrl("nepomuk:/repo"), RDF::type(), RDFS::Resource(), graph );

    QHash<QUrl, QVariant> additionalMetadata;
    additionalMetadata.insert( QUrl("prop:/graph"), QUrl("nepomuk:/repo") );

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app"), IdentifyNew,
                               NoStoreResourcesFlags, additionalMetadata );

    // The should be no error as the additionalMetadata should implicitly have the nrl:Graph type
    QVERIFY( !m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}

// make sure that two resources are treated as different if their nie:url differs, even if they already exist
void DataManagementModelTest::testStoreResources_nieUrlDefinesResources()
{
    // we create two resources which only differ in their nie:url - but that does make all the difference!
    QUrl urlA( "http://www.k3b.org" );
    QUrl urlB( "http://nepomuk.kde.org" );

    const QUrl g1 = m_nrlModel->createGraph( NRL::InstanceBase() );
    m_model->addStatement( QUrl("nepomuk:/res/A"), RDF::type(), QUrl("class:/typeB"), g1 );
    m_model->addStatement( QUrl("nepomuk:/res/A"), NIE::url(), urlA, g1 );
    m_model->addStatement( QUrl("nepomuk:/res/B"), RDF::type(), QUrl("class:/typeB"), g1 );
    m_model->addStatement( QUrl("nepomuk:/res/B"), NIE::url(),urlB, g1 );


    // create a new resource and add links to both existing ones
    SimpleResourceGraph graph;
    SimpleResource mainRes;
    mainRes.addType(QUrl("class:/typeA"));

    SimpleResource resA(urlA);
    resA.addType(QUrl("class:/typeB"));
    mainRes.addProperty(QUrl("prop:/res"), urlA);

    SimpleResource resB(urlB);
    resB.addType(QUrl("class:/typeB"));
    mainRes.addProperty(QUrl("prop:/res"), urlB);

    graph << resA << resB << mainRes;


    // store the data
    m_dmModel->storeResources(graph, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // verify that all is there
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { ?r a <class:/typeA> . ?r <prop:/res> <nepomuk:/res/A> . }"),
                                  Soprano::Query::QueryLanguageSparql).boolValue());
    QVERIFY(m_model->executeQuery(QString::fromLatin1("ask where { ?r a <class:/typeA> . ?r <prop:/res> <nepomuk:/res/B> . }"),
                                  Soprano::Query::QueryLanguageSparql).boolValue());
}

void DataManagementModelTest::testStoreResources_objectExistsIdentification()
{
    SimpleResource res;
    res.addType( NAO::Tag() );
    res.addProperty( NAO::identifier(), QLatin1String("Christmas") );

    QHash< QUrl, QUrl > m = m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app") );
    QVERIFY( !m_dmModel->lastError() );

    SimpleResource person;
    person.addType( NCO::Contact() );
    person.addProperty( NCO::fullname(), QLatin1String("Santa Claus") );
    person.addProperty( NAO::hasTag(), m.value(res.uri()) );

    m_dmModel->storeResources( SimpleResourceGraph() << person, QLatin1String("app") );
    QVERIFY( !m_dmModel->lastError() );

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testMergeResources()
{
    // first we need to create the two resources we want to merge as well as one that should not be touched
    // for this simple test we put everything into one graph
    QUrl mg1;
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase(), &mg1);

    // the resource in which we want to merge
    QUrl resA("nepomuk:/res/A");
    m_model->addStatement(resA, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/int_c1"), LiteralValue(42), g1);
    m_model->addStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    // the resource that is going to be merged
    // one duplicate property and one that differs, one backlink to ignore,
    // one property with cardinality 1 to ignore
    QUrl resB("nepomuk:/res/B");
    m_model->addStatement(resB, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resB, QUrl("prop:/int_c1"), LiteralValue(12), g1);
    m_model->addStatement(resB, QUrl("prop:/string"), LiteralValue(QLatin1String("hello")), g1);
    m_model->addStatement(resA, QUrl("prop:/res"), resB, g1);

    // resource C to ignore (except the backlink which needs to be updated)
    QUrl resC("nepomuk:/res/C");
    m_model->addStatement(resC, QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(resC, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(resC, QUrl("prop:/res"), resB, g1);


    // now merge the resources
    m_dmModel->mergeResources(QList<QUrl>() << resA << resB, QLatin1String("A"));
    kDebug() << m_dmModel->lastError();
    QVERIFY(m_dmModel->lastError() == Soprano::Error::ErrorNone);

    // make sure B is gone
    QVERIFY(!m_model->containsAnyStatement(resB, Node(), Node()));
    QVERIFY(!m_model->containsAnyStatement(Node(), Node(), resB));

    // make sure A has all the required properties
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/int"), LiteralValue(42)));
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/int_c1"), LiteralValue(42)));
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));
    QVERIFY(m_model->containsAnyStatement(resA, QUrl("prop:/string"), LiteralValue(QLatin1String("hello"))));

    // make sure A has no superfluous properties
    QVERIFY(!m_model->containsAnyStatement(resA, QUrl("prop:/int_c1"), LiteralValue(12)));
    QCOMPARE(m_model->listStatements(resA, QUrl("prop:/int"), Node()).allElements().count(), 1);

    // make sure the backlink was updated
    QVERIFY(m_model->containsAnyStatement(resC, QUrl("prop:/res"), resA));

    // make sure C was not touched apart from the backlink
    QVERIFY(m_model->containsStatement(resC, QUrl("prop:/int"), LiteralValue(42), g1));
    QVERIFY(m_model->containsStatement(resC, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1));

    QVERIFY(!haveDataInDefaultGraph());
    QVERIFY(!haveMetadataInOtherGraphs());
}


void DataManagementModelTest::testDescribeResources()
{
    QTemporaryFile fileC;
    fileC.open();

    // create some resources
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());

    m_model->addStatement(QUrl("res:/A"), RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);

    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);

    m_model->addStatement(QUrl("res:/C"), NIE::url(), QUrl::fromLocalFile(fileC.fileName()), g1);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/int"), LiteralValue(42), g1);
    m_model->addStatement(QUrl("res:/C"), NAO::hasSubResource(), QUrl("res:/D"), g1);

    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("Hello")), g1);


    // get one resource without related
    SimpleResourceGraph g = m_dmModel->describeResources(QList<QUrl>() << QUrl("res:/A"), Nepomuk2::ExcludeRelatedResources);

    // no error
    QVERIFY(!m_dmModel->lastError());

    // A and its sub-res B
    QVERIFY(g.contains(QUrl("res:/A")));
    QVERIFY(g.contains(QUrl("res:/B")));
    QCOMPARE(g.count(), 2);

    // res:/A has 3 properties
    QEXPECT_FAIL("", "Not clear yet if prop:/res should be returned in addition to nao:hasSubResource", Continue);
    QCOMPARE(g[QUrl("res:/A")].properties().count(), 3);


    // get one resource by file-url without related
    g = m_dmModel->describeResources(QList<QUrl>() << QUrl::fromLocalFile(fileC.fileName()), Nepomuk2::ExcludeRelatedResources);

    // no error
    QVERIFY(!m_dmModel->lastError());

    // C and its sub-res
    QVERIFY(g.contains(QUrl("res:/C")));
    QVERIFY(g.contains(QUrl("res:/D")));
    QCOMPARE(g.count(), 2);

    // res:/C has 3 properties
    QVERIFY(g[QUrl("res:/C")].contains(NIE::url(), QUrl::fromLocalFile(fileC.fileName())));
    QVERIFY(g[QUrl("res:/C")].contains(QUrl("prop:/int"), 42));
    QVERIFY(g[QUrl("res:/C")].contains(NAO::hasSubResource(), QUrl("res:/D")));
    QCOMPARE(g[QUrl("res:/C")].properties().count(), 3);


    // the result with related res should be the same as there is no related non-sub-res
    QCOMPARE(g, m_dmModel->describeResources(QList<QUrl>() << QUrl::fromLocalFile(fileC.fileName())));


    // get two resources with sub-res and mixed URL/URI
    g = m_dmModel->describeResources(QList<QUrl>() << QUrl("res:/A") << QUrl::fromLocalFile(fileC.fileName()));

    // no error
    QVERIFY(!m_dmModel->lastError());

    // only one resource in the result
    QVERIFY(g.contains(QUrl("res:/A")));
    QVERIFY(g.contains(QUrl("res:/B")));
    QVERIFY(g.contains(QUrl("res:/C")));
    QVERIFY(g.contains(QUrl("res:/D")));
    QCOMPARE(g.count(), 4);
}

// test that related resources are properly returned, ie. only containing their identifying properties
void DataManagementModelTest::testDescribeResources_relatedResources()
{
    // create two main resources, one sub-resource, two related resources,
    // one related resource to the sub-resource, one sub-resource to a related resource,
    // and one related resource to one related resource. The latter once as identifying
    // and once non-identifying
    const QUrl g = m_nrlModel->createGraph(NRL::InstanceBase());

    // main res 1: A
    m_model->addStatement(QUrl("res:/A"), RDF::type(), QUrl("class:/typeA"), g);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g);

    // main res 2: B
    m_model->addStatement(QUrl("res:/B"), RDF::type(), QUrl("class:/typeB"), g);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/int"), LiteralValue(2), g);

    // sub-resource to A
    m_model->addStatement(QUrl("res:/AA"), RDF::type(), QUrl("class:/typeA"), g);
    m_model->addStatement(QUrl("res:/AA"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/AA"), g);

    // related res to AA
    m_model->addStatement(QUrl("res:/AAA"), RDF::type(), QUrl("class:/typeB"), g);
    m_model->addStatement(QUrl("res:/AAA"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/AA"), QUrl("prop:/res"), QUrl("res:/AAA"), g);

    // related res to B
    m_model->addStatement(QUrl("res:/BB"), RDF::type(), QUrl("class:/typeC"), g);
    m_model->addStatement(QUrl("res:/BB"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/res"), QUrl("res:/BB"), g);

    // related res to BB (identifying)
    m_model->addStatement(QUrl("res:/BBB_ident"), RDF::type(), QUrl("class:/typeC"), g);
    m_model->addStatement(QUrl("res:/BBB_ident"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/BB"), QUrl("prop:/res_ident"), QUrl("res:/BBB_ident"), g);

    // related res to BB (non-identifying)
    m_model->addStatement(QUrl("res:/BBB"), RDF::type(), QUrl("class:/typeC"), g);
    m_model->addStatement(QUrl("res:/BBB"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/BB"), QUrl("prop:/res"), QUrl("res:/BBB"), g);

    // sub-resource to AA
    m_model->addStatement(QUrl("res:/AAA_sub"), RDF::type(), QUrl("class:/typeB"), g);
    m_model->addStatement(QUrl("res:/AAA_sub"), QUrl("prop:/int"), LiteralValue(42), g);
    m_model->addStatement(QUrl("res:/AA"), NAO::hasSubResource(), QUrl("res:/AAA_sub"), g);


    {
        // describe A and B
        const SimpleResourceGraph graph = m_dmModel->describeResources(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/B"));

        // the graph should contain all resources except BBB which is a non-identifying relation to a related resource
        QVERIFY(graph.contains(QUrl("res:/A")));
        QVERIFY(graph.contains(QUrl("res:/B")));
        QVERIFY(graph.contains(QUrl("res:/AA")));
        QVERIFY(graph.contains(QUrl("res:/AAA")));
        QVERIFY(graph.contains(QUrl("res:/BB")));
        QVERIFY(graph.contains(QUrl("res:/BBB_ident")));
        QVERIFY(graph.contains(QUrl("res:/AAA_sub")));
        QCOMPARE(graph.count(), 7);

        const SimpleResource resA = graph[QUrl("res:/A")];
        const SimpleResource resB = graph[QUrl("res:/B")];
        const SimpleResource resAA = graph[QUrl("res:/AA")];
        const SimpleResource resAAA = graph[QUrl("res:/AAA")];
        const SimpleResource resBB = graph[QUrl("res:/BB")];
        const SimpleResource resBBB_ident = graph[QUrl("res:/BBB_ident")];
        const SimpleResource resAAA_sub = graph[QUrl("res:/AAA_sub")];

        // A
        QVERIFY(resA.contains(RDF::type(), QUrl("class:/typeA")));
        QVERIFY(resA.contains(QUrl("prop:/string"), QLatin1String("foobar")));
        QVERIFY(resA.contains(QUrl("prop:/int"), 42));
        QVERIFY(resA.contains(NAO::hasSubResource(), QUrl("res:/AA")));
        QCOMPARE(resA.properties().count(), 4);

        // B
        QVERIFY(resB.contains(RDF::type(), QUrl("class:/typeB")));
        QVERIFY(resB.contains(QUrl("prop:/string"), QLatin1String("hello world")));
        QVERIFY(resB.contains(QUrl("prop:/int"), 2));
        QVERIFY(resB.contains(QUrl("prop:/res"), QUrl("res:/BB")));
        QCOMPARE(resB.properties().count(), 4);

        // AA
        QVERIFY(resAA.contains(RDF::type(), QUrl("class:/typeA")));
        QVERIFY(resAA.contains(QUrl("prop:/int"), 42));
        QVERIFY(resAA.contains(QUrl("prop:/res"), QUrl("res:/AAA")));
        QVERIFY(resAA.contains(NAO::hasSubResource(), QUrl("res:/AAA_sub")));
        QCOMPARE(resAA.properties().count(), 4);

        // AAA
        QVERIFY(resAAA.contains(RDF::type(), QUrl("class:/typeB")));
        QVERIFY(resAAA.contains(QUrl("prop:/int"), 42));
        QCOMPARE(resAAA.properties().count(), 2);

        // BB
        QVERIFY(resBB.contains(RDF::type(), QUrl("class:/typeC")));
        QVERIFY(resBB.contains(QUrl("prop:/int"), 42));
        QVERIFY(resBB.contains(QUrl("prop:/res_ident"), QUrl("res:/BBB_ident")));
        QCOMPARE(resBB.properties().count(), 3);

        // BBB_ident
        QVERIFY(resBBB_ident.contains(RDF::type(), QUrl("class:/typeC")));
        QVERIFY(resBBB_ident.contains(QUrl("prop:/int"), 42));
        QCOMPARE(resBBB_ident.properties().count(), 2);

        // AAA_sub
        QVERIFY(resAAA_sub.contains(RDF::type(), QUrl("class:/typeB")));
        QVERIFY(resAAA_sub.contains(QUrl("prop:/int"), 42));
        QCOMPARE(resAAA_sub.properties().count(), 2);
    }

    {
        // describe A and B excluding related resources
        const SimpleResourceGraph graph = m_dmModel->describeResources(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/B"), ExcludeRelatedResources);

        // the graph should only contains A and B and their sub-resources
        QVERIFY(graph.contains(QUrl("res:/A")));
        QVERIFY(graph.contains(QUrl("res:/B")));
        QVERIFY(graph.contains(QUrl("res:/AA")));
        QVERIFY(graph.contains(QUrl("res:/AAA_sub")));
        QCOMPARE(graph.count(), 4);

        const SimpleResource resA = graph[QUrl("res:/A")];
        const SimpleResource resB = graph[QUrl("res:/B")];
        const SimpleResource resAA = graph[QUrl("res:/AA")];
        const SimpleResource resAAA_sub = graph[QUrl("res:/AAA_sub")];

        // A
        QVERIFY(resA.contains(RDF::type(), QUrl("class:/typeA")));
        QVERIFY(resA.contains(QUrl("prop:/string"), QLatin1String("foobar")));
        QVERIFY(resA.contains(QUrl("prop:/int"), 42));
        QVERIFY(resA.contains(NAO::hasSubResource(), QUrl("res:/AA")));
        QCOMPARE(resA.properties().count(), 4);

        // B
        QVERIFY(resB.contains(RDF::type(), QUrl("class:/typeB")));
        QVERIFY(resB.contains(QUrl("prop:/string"), QLatin1String("hello world")));
        QVERIFY(resB.contains(QUrl("prop:/int"), 2));
        QCOMPARE(resB.properties().count(), 3);

        // AA
        QVERIFY(resAA.contains(RDF::type(), QUrl("class:/typeA")));
        QVERIFY(resAA.contains(QUrl("prop:/int"), 42));
        QVERIFY(resAA.contains(NAO::hasSubResource(), QUrl("res:/AAA_sub")));
        QCOMPARE(resAA.properties().count(), 3);

        // AAA_sub
        QVERIFY(resAAA_sub.contains(RDF::type(), QUrl("class:/typeB")));
        QVERIFY(resAAA_sub.contains(QUrl("prop:/int"), 42));
        QCOMPARE(resAAA_sub.properties().count(), 2);
    }
}

// test that discardable data is excluded properly
void DataManagementModelTest::testDescribeResources_excludeDiscardableData()
{
    QEXPECT_FAIL("", "This test fails and I cannot figure out why. Plus, no-one uses this function", Abort);

    QTemporaryFile file;
    file.open();

    // create three graphs: 2 discardable and one non-discardable
    const QUrl g1 = m_nrlModel->createGraph(NRL::DiscardableInstanceBase());
    const QUrl g2 = m_nrlModel->createGraph(NRL::DiscardableInstanceBase());
    const QUrl g3 = m_nrlModel->createGraph(NRL::InstanceBase());


    // create the main resource - a bit of discardable and non-discardable data
    m_model->addStatement(QUrl("res:/A"), RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(QUrl("res:/A"), NIE::url(), KUrl(file.fileName()), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello")), g2);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("world")), g3);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/int"), LiteralValue(42), g3);

    // a discardable sub-resource relation to a discardable resource
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/B"), g1);
    m_model->addStatement(QUrl("res:/B"), RDF::type(), QUrl("class:/typeB"), g2);
    m_model->addStatement(QUrl("res:/B"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);

    // a discardable sub-resource relation to a non-discardable resource
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/C"), g1);
    m_model->addStatement(QUrl("res:/C"), RDF::type(), QUrl("class:/typeB"), g3);
    m_model->addStatement(QUrl("res:/C"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // a discardable relation to a discardable resource
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/D"), g1);
    m_model->addStatement(QUrl("res:/D"), RDF::type(), QUrl("class:/typeB"), g2);
    m_model->addStatement(QUrl("res:/D"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);

    // a discardable relation to a non-discardable resource
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/E"), g1);
    m_model->addStatement(QUrl("res:/E"), RDF::type(), QUrl("class:/typeB"), g3);
    m_model->addStatement(QUrl("res:/E"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // a non-discardable sub-resource relation to a discardable resource
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/F"), g3);
    m_model->addStatement(QUrl("res:/F"), RDF::type(), QUrl("class:/typeB"), g2);
    m_model->addStatement(QUrl("res:/F"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);

    // a non-discardable sub-resource relation to a non-discardable resource
    m_model->addStatement(QUrl("res:/A"), NAO::hasSubResource(), QUrl("res:/G"), g3);
    m_model->addStatement(QUrl("res:/G"), RDF::type(), QUrl("class:/typeB"), g3);
    m_model->addStatement(QUrl("res:/G"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // a non-discardable relation to a discardable resource
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/H"), g3);
    m_model->addStatement(QUrl("res:/H"), RDF::type(), QUrl("class:/typeB"), g2);
    m_model->addStatement(QUrl("res:/H"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g2);

    // a non-discardable relation to a non-discardable resource
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/res"), QUrl("res:/I"), g3);
    m_model->addStatement(QUrl("res:/I"), RDF::type(), QUrl("class:/typeB"), g3);
    m_model->addStatement(QUrl("res:/I"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g3);

    // a second main resource, completely discardable except for some metadata -> this should be omitted completely
    m_model->addStatement(QUrl("res:/J"), RDF::type(), QUrl("class:/typeA"), g1);
    m_model->addStatement(QUrl("res:/J"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/J"), NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), g3);
    m_model->addStatement(QUrl("res:/J"), NAO::userVisible(), LiteralValue(true), g3);


    // describe res:/A and res:/J
    const SimpleResourceGraph graph = m_dmModel->describeResources(QList<QUrl>() << QUrl("res:/A") << QUrl("res:/J"), ExcludeDiscardableData);

    // now the graph should contain res:/A, res:/G, res:/I
    QCOMPARE(graph.count(), 3);
    QVERIFY(graph.contains(QUrl("res:/A")));
    QVERIFY(graph.contains(QUrl("res:/G")));
    QVERIFY(graph.contains(QUrl("res:/I")));

    const SimpleResource resA = graph[QUrl("res:/A")];
    const SimpleResource resG = graph[QUrl("res:/G")];
    const SimpleResource resI = graph[QUrl("res:/I")];

    // resA
    QVERIFY(resA.contains(QUrl("prop:/string"), QLatin1String("world")));
    QVERIFY(resA.contains(QUrl("prop:/int"), 42));
    QVERIFY(resA.contains(NIE::url(), QUrl(KUrl(file.fileName()))));
    QVERIFY(resA.contains(NAO::hasSubResource(), QUrl("res:/G")));
    QVERIFY(resA.contains(QUrl("prop:/res"), QUrl("res:/I")));
    QCOMPARE(resA.properties().count(), 5);

    // resG
    QVERIFY(resG.contains(QUrl("prop:/string"), QLatin1String("foobar")));
    QVERIFY(resG.contains(RDF::type(), QUrl("class:/typeB")));
    QCOMPARE(resG.properties().count(), 2);

    // resI
    QVERIFY(resI.contains(QUrl("prop:/string"), QLatin1String("foobar")));
    QVERIFY(resI.contains(RDF::type(), QUrl("class:/typeB")));
    QCOMPARE(resI.properties().count(), 2);
}

KTempDir * DataManagementModelTest::createNieUrlTestData()
{
    // now we create a real example with some real files:
    // mainDir
    // |- dir1
    //    |- dir11
    //       |- file111
    //    |- dir12
    //       |- dir121
    //          |- file1211
    //    |- file11
    //    |- dir13
    // |- dir2
    KTempDir* mainDir = new KTempDir();
    QDir dir(mainDir->name());
    dir.mkdir(QLatin1String("dir1"));
    dir.mkdir(QLatin1String("dir2"));
    dir.cd(QLatin1String("dir1"));
    dir.mkdir(QLatin1String("dir11"));
    dir.mkdir(QLatin1String("dir12"));
    dir.mkdir(QLatin1String("dir13"));
    QFile file(dir.filePath(QLatin1String("file11")));
    file.open(QIODevice::WriteOnly);
    file.close();
    dir.cd(QLatin1String("dir12"));
    dir.mkdir(QLatin1String("dir121"));
    dir.cd(QLatin1String("dir121"));
    file.setFileName(dir.filePath(QLatin1String("file1211")));
    file.open(QIODevice::WriteOnly);
    file.close();
    dir.cdUp();
    dir.cdUp();
    dir.cd(QLatin1String("dir11"));
    file.setFileName(dir.filePath(QLatin1String("file111")));
    file.open(QIODevice::WriteOnly);
    file.close();

    // We now create the situation in the model
    // for that we use 2 graphs
    const QUrl g1 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QUrl g2 = m_nrlModel->createGraph(NRL::InstanceBase());
    const QString basePath = mainDir->name();

    const QUrl ng = m_dmModel->nepomukGraph();

    // nie:url properties for all of them (spread over both graphs)
    m_model->addStatement(QUrl("res:/dir1"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1")), ng);
    m_model->addStatement(QUrl("res:/dir2"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir2")), ng);
    m_model->addStatement(QUrl("res:/dir11"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1/dir11")), ng);
    m_model->addStatement(QUrl("res:/dir12"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1/dir12")), ng);
    m_model->addStatement(QUrl("res:/dir13"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1/dir13")), ng);
    m_model->addStatement(QUrl("res:/file11"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1/file11")), ng);
    m_model->addStatement(QUrl("res:/file111"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir1/dir11/file111")), ng);
    m_model->addStatement(QUrl("res:/dir121"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir2/dir121")), ng);
    m_model->addStatement(QUrl("res:/file1211"), NIE::url(), QUrl(QLatin1String("file://") + basePath + QLatin1String("dir2/dir121/file1211")), ng);

    // we define filename and parent folder only for some to test if the optional clause in the used query works properly
    m_model->addStatement(QUrl("res:/dir1"), NFO::fileName(), LiteralValue(QLatin1String("dir1")), g1);
    m_model->addStatement(QUrl("res:/dir2"), NFO::fileName(), LiteralValue(QLatin1String("dir2")), g1);
    m_model->addStatement(QUrl("res:/dir11"), NFO::fileName(), LiteralValue(QLatin1String("dir11")), g2);
    m_model->addStatement(QUrl("res:/dir12"), NFO::fileName(), LiteralValue(QLatin1String("dir12")), g2);
    m_model->addStatement(QUrl("res:/file11"), NFO::fileName(), LiteralValue(QLatin1String("file11")), g1);
    m_model->addStatement(QUrl("res:/file111"), NFO::fileName(), LiteralValue(QLatin1String("file111")), g2);
    m_model->addStatement(QUrl("res:/dir121"), NFO::fileName(), LiteralValue(QLatin1String("dir121")), g2);

    m_model->addStatement(QUrl("res:/dir11"), NIE::isPartOf(), QUrl("res:/dir1"), g1);
    m_model->addStatement(QUrl("res:/dir12"), NIE::isPartOf(), QUrl(QLatin1String("res:/dir1")), g2);
    m_model->addStatement(QUrl("res:/dir13"), NIE::isPartOf(), QUrl(QLatin1String("res:/dir1")), g1);
    m_model->addStatement(QUrl("res:/file111"), NIE::isPartOf(), QUrl(QLatin1String("res:/dir11")), g1);
    m_model->addStatement(QUrl("res:/dir121"), NIE::isPartOf(), QUrl(QLatin1String("res:/dir2")), g2);
    m_model->addStatement(QUrl("res:/file1211"), NIE::isPartOf(), QUrl(QLatin1String("res:/dir121")), g1);

    return mainDir;
}


bool DataManagementModelTest::haveDataInDefaultGraph() const
{
    return m_model->executeQuery(QString::fromLatin1("ask where { "
                                                     "graph <sopranofakes:/DEFAULTGRAPH> { ?s ?p ?o . } . "
                                                     "}"),
                                 Soprano::Query::QueryLanguageSparql).boolValue();
}

bool DataManagementModelTest::haveMetadataInOtherGraphs() const
{
    // Do not count graph metadata graphs for now
    QString query = QString::fromLatin1("select ?g ?r ?p ?o where { graph ?g { ?r ?p ?o . } "
                                        "FILTER NOT EXISTS { ?g nrl:coreGraphMetadataFor ?gg . }"
                                        "FILTER(?p in (nie:url, nao:lastModified, nao:created)) ."
                                        "FILTER(?g!=%1) . }")
                    .arg( Soprano::Node::resourceToN3(m_dmModel->nepomukGraph()));

    QList<BindingSet> bindings = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).allBindings();
    foreach(const Soprano::BindingSet& bs, bindings ) {
        kDebug() << bs;
    }
    return bindings.count();
}

void DataManagementModelTest::testImportResources()
{
    // create the test data
    QTemporaryFile fileA;
    fileA.open();

    Soprano::Graph graph;
    graph.addStatement(Node(QString::fromLatin1("res1")), QUrl("prop:/int"), LiteralValue(42));
    graph.addStatement(Node(QString::fromLatin1("res1")), RDF::type(), QUrl("class:/typeA"));
    graph.addStatement(Node(QString::fromLatin1("res1")), QUrl("prop:/res"), Node(QString::fromLatin1("res2")));
    graph.addStatement(Node(QString::fromLatin1("res2")), RDF::type(), QUrl("class:/typeB"));
    graph.addStatement(QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/int"), LiteralValue(12));
    graph.addStatement(QUrl::fromLocalFile(fileA.fileName()), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")));

    // write the test file
    QTemporaryFile tmp;
    tmp.open();
    QTextStream str(&tmp);
    Q_FOREACH(const Statement& s, graph.toList()) {
        str << s.subject().toN3() << " " << s.predicate().toN3() << " " << s.object().toN3() << " ." << endl;
    }
    tmp.close();


    // import the file
    m_dmModel->importResources(QUrl::fromLocalFile(tmp.fileName()), QLatin1String("A"), Soprano::SerializationNTriples);


    // make sure the data has been imported properly
    QVERIFY(m_model->containsAnyStatement(Node(), QUrl("prop:/int"), LiteralValue(42)));
    const QUrl res1Uri = m_model->listStatements(Node(), QUrl("prop:/int"), LiteralValue(42)).allStatements().first().subject().uri();
    QVERIFY(m_model->containsAnyStatement(res1Uri, RDF::type(), QUrl("class:/typeA")));
    QVERIFY(m_model->containsAnyStatement(res1Uri, QUrl("prop:/res"), Node()));
    const QUrl res2Uri = m_model->listStatements(res1Uri, QUrl("prop:/res"), Node()).allStatements().first().object().uri();
    QVERIFY(m_model->containsAnyStatement(res2Uri, RDF::type(), QUrl("class:/typeB")));
    QVERIFY(m_model->containsAnyStatement(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())));
    const QUrl res3Uri = m_model->listStatements(Node(), NIE::url(), QUrl::fromLocalFile(fileA.fileName())).allStatements().first().subject().uri();
    QVERIFY(m_model->containsAnyStatement(res3Uri, QUrl("prop:/int"), LiteralValue(12)));
    QVERIFY(m_model->containsAnyStatement(res3Uri, QUrl("prop:/string"), LiteralValue(QLatin1String("foobar"))));

    // make sure the metadata is there
    QVERIFY(m_model->containsAnyStatement(res1Uri, NAO::lastModified(), Node()));
    QVERIFY(m_model->containsAnyStatement(res1Uri, NAO::created(), Node()));
    QVERIFY(m_model->containsAnyStatement(res2Uri, NAO::lastModified(), Node()));
    QVERIFY(m_model->containsAnyStatement(res2Uri, NAO::created(), Node()));
    QVERIFY(m_model->containsAnyStatement(res3Uri, NAO::lastModified(), Node()));
    QVERIFY(m_model->containsAnyStatement(res3Uri, NAO::created(), Node()));
}

void DataManagementModelTest::checkDataMaintainedBy(const Soprano::Statement& st, const QList< QString >& apps)
{
    kDebug() << st;
    QSet<Soprano::Node> stList = m_model->listStatements( st ).iterateContexts().allNodes().toSet();
    QCOMPARE( stList.size(), apps.size() );

    QSet<QUrl> appUris;
    foreach(const Soprano::Node& node, stList) {
        QSet<Node> nodes = m_model->listStatements( node, NAO::maintainedBy(), Node() ).iterateObjects().allNodes().toSet();
        QCOMPARE( nodes.size(), 1 );

        appUris << nodes.begin()->uri();
    }

    QCOMPARE( appUris.size(), apps.size() );

    QSet<QUrl> otherAppUris;
    foreach(const QString& app, apps) {
        QString query = QString::fromLatin1("select ?r where { ?r a nao:Agent ; nao:identifier %1 . }")
                        .arg( Soprano::Node::literalToN3(app) );

        QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        QList<QUrl> apps;
        while( it.next() ) {
            apps << it[0].uri();
        }
        QCOMPARE( apps.size(), 1 );

        otherAppUris << apps.first();
    }

    QCOMPARE( appUris, otherAppUris );
}

QTEST_KDEMAIN_CORE(DataManagementModelTest)

#include "datamanagementmodeltest.moc"
