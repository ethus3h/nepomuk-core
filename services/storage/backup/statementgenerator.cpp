/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "statementgenerator.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>

#include <KDebug>

#include <Soprano/QueryResultIterator>
#include <Soprano/PluginManager>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>

using namespace Nepomuk2;
using namespace Nepomuk2::Backup;

StatementGenerator::StatementGenerator(Soprano::Model* model, const QString& inputFile,
                                       const QString& outputFile, QObject* parent)
    : KJob(parent)
    , m_model(model)
    , m_inputFile(inputFile)
    , m_outputFile(outputFile)
{
}

void StatementGenerator::start()
{
    QTimer::singleShot( 0, this, SLOT(doJob()) );
}

void StatementGenerator::doJob()
{
    Soprano::PluginManager* pg = Soprano::PluginManager::instance();
    const Soprano::Serializer* serializer = pg->discoverSerializerForSerialization( Soprano::SerializationNQuads );

    QFile input( m_inputFile );
    kDebug() << "INPUT: " << m_inputFile;
    if( !input.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_inputFile ) );
        return;
    }

    QFile output( m_outputFile );
    if( !output.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_outputFile ) );
        return;
    }

    QTextStream inputStream( &input );
    QTextStream outputStream( &output );

    while( !inputStream.atEnd() ) {
        const QUrl uri( inputStream.readLine() );
        kDebug() << "Processing" << uri;

        QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                            " FILTER(!(?p in (nao:lastModified, nao:created, rdf:type, nie:url))) ."
                                            " ?g a nrl:InstanceBase ."
                                            " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                        .arg( Soprano::Node::resourceToN3(uri) );

        Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        QList<Soprano::Statement> stList;
        while( it.next() ) {
            Soprano::Statement st( uri, it[0], it[1], it[2]);
            stList << st;
        }

        if( stList.count() ) {
            QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                                " FILTER(?p in (nao:lastModified, nao:created, rdf:type, nie:url)) . }")
                            .arg( Soprano::Node::resourceToN3(uri) );

            Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
            while( it.next() ) {
                Soprano::Statement st( uri, it[0], it[1], it[2] );
                stList << st;
            }

            Soprano::Util::SimpleStatementIterator iter( stList );
            serializer->serialize( iter, outputStream, Soprano::SerializationNQuads );
        }
    }

    emitResult();
}


