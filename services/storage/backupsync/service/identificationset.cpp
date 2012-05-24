/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

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


#include "identificationset.h"
#include "changelog.h"
#include "changelogrecord.h"
#include "nrio.h"

#include <QtCore/QList>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QHash>
#include <QtCore/QMultiHash>
#include <QtCore/QHashIterator>
#include <QtCore/QMutableHashIterator>
#include <QtCore/QSet>
#include <QtCore/QMutableSetIterator>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Serializer>
#include <Soprano/Parser>
#include <Soprano/Util/SimpleStatementIterator>
#include <Soprano/PluginManager>

#include "resourcemanager.h"

#include <KDebug>

using namespace Soprano::Vocabulary;

namespace {
    class IdentificationSetGenerator {
    public :
        IdentificationSetGenerator( const QSet<QUrl>& uniqueUris, Soprano::Model * m , const QSet<QUrl> & ignoreList = QSet<QUrl>());

        Soprano::Model * m_model;
        QSet<QUrl> m_done;
        QSet<QUrl> m_notDone;

        QList<Soprano::Statement> statements;

        Soprano::QueryResultIterator queryIdentifyingStatements( const QStringList& uris );
        void iterate();
        QList<Soprano::Statement> generate();

        static const int maxIterationSize = 500;

        bool done() const { return m_notDone.isEmpty(); }
    };

    IdentificationSetGenerator::IdentificationSetGenerator(const QSet<QUrl>& uniqueUris, Soprano::Model* m, const QSet<QUrl> & ignoreList)
    {
        m_notDone = uniqueUris - ignoreList;
        m_model = m;
        m_done = ignoreList;
    }

    Soprano::QueryResultIterator IdentificationSetGenerator::queryIdentifyingStatements(const QStringList& uris)
    {
        //
        // select distinct ?r ?p ?o where { ?r ?p ?o.
        // ?p rdfs:subPropertyOf nrio:identifyingProperty .
        // FILTER( ?r in ( <res1>, <res2>, ... ) ) . }
        //
        QString query = QString::fromLatin1("select distinct ?r ?p ?o where { ?r ?p ?o. "
                                            "?p %1 %2 . "
                                            " FILTER( ?r in ( %4 ) ) . } ")
                        .arg(Soprano::Node::resourceToN3( RDFS::subPropertyOf() ),
                             Soprano::Node::resourceToN3( NRL::DefiningProperty() ),
                             uris.join(", ") );

        return m_model->executeQuery(query, Soprano::Query::QueryLanguageSparql);
    }

    void IdentificationSetGenerator::iterate()
    {
        QStringList uris;

        QMutableSetIterator<QUrl> iter( m_notDone );
        while( iter.hasNext() ) {
            const QUrl & uri = iter.next();
            m_done.insert( uri );

            uris.append( Soprano::Node::resourceToN3( uri ) );

            iter.remove();
            if( uris.size() == maxIterationSize )
                break;
        }

        Soprano::QueryResultIterator it = queryIdentifyingStatements( uris );
        while( it.next() ) {
            Soprano::Node sub = it["r"];
            Soprano::Node pred = it["p"];
            Soprano::Node obj = it["o"];

            statements.push_back( Soprano::Statement( sub, pred, obj ) );

            // If the object is also a nepomuk uri, it too needs to be identified.
            const QUrl & objUri = obj.uri();
            if( objUri.toString().startsWith("nepomuk:/res/") ) {
                if( !m_done.contains( objUri ) ) {
                    m_notDone.insert( objUri );
                }
            }
        }
    }

    QList<Soprano::Statement> IdentificationSetGenerator::generate()
    {
        m_done.clear();

        while( !done() ) {
            iterate();
        }
        return statements;
    }
}


class Nepomuk2::IdentificationSet::Private : public QSharedData {
public:
    QList<Soprano::Statement> m_statements;
};


Nepomuk2::IdentificationSet::IdentificationSet()
    : d( new Nepomuk2::IdentificationSet::Private )
{
}


Nepomuk2::IdentificationSet::IdentificationSet(const Nepomuk2::IdentificationSet& rhs)
    : d( rhs.d )
{
}


Nepomuk2::IdentificationSet& Nepomuk2::IdentificationSet::operator=(const Nepomuk2::IdentificationSet& rhs)
{
    d = rhs.d;
    return *this;
}


Nepomuk2::IdentificationSet::~IdentificationSet()
{
}


Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromUrl(const QUrl& url)
{
    QFile file( url.toLocalFile() );
    if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        kWarning() << "The file " << url << " failed to load";
        return IdentificationSet();
    }

    QTextStream in( &file );
    return fromTextStream( in );
}


Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromTextStream(QTextStream& ts)
{
    //
    // Parse all the statements
    //
    const Soprano::Parser * parser = Soprano::PluginManager::instance()->discoverParserForSerialization( Soprano::SerializationNQuads );

    if( !parser ) {
        kDebug() << "The required parser could not be loaded.";
        return IdentificationSet();
    }

    Soprano::StatementIterator iter = parser->parseStream( ts, QUrl(), Soprano::SerializationNQuads );

    IdentificationSet identSet;
    identSet.d->m_statements = iter.allElements();
    return identSet;
}

namespace {

    //
    // Separate all the unique URIs of scheme "nepomuk" from the subject and object in all the statements.
    //
    // vHanda: Maybe we should separate the graphs as well. Identification isn't meant for graphs.
    QSet<QUrl> getUniqueUris( const QList<Nepomuk2::ChangeLogRecord> records ) {
        QSet<QUrl> uniqueUris;
        foreach( const Nepomuk2::ChangeLogRecord & r, records ) {
            QUrl sub = r.st().subject().uri();
            uniqueUris.insert( sub );

            // If the Object is a resource, then it has to be identified as well.
            const Soprano::Node obj = r.st().object();
            if( obj.isResource() ) {
                QUrl uri = obj.uri();
                if( uri.scheme() == QLatin1String("nepomuk") )
                    uniqueUris.insert( uri );
            }
        }
        return uniqueUris;
    }
}

Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromChangeLog(const Nepomuk2::ChangeLog& log, Soprano::Model* model, const QSet<QUrl> & ignoreList)
{
    QSet<QUrl> uniqueUris = getUniqueUris( log.toList() );

    IdentificationSetGenerator ifg( uniqueUris, model, ignoreList );
    IdentificationSet is;
    is.d->m_statements = ifg.generate();
    return is;
}

Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromResource(const QUrl & resourceUrl, Soprano::Model* model, const QSet<QUrl> &  ignoreList)
{
    QSet<QUrl> uniqueUris;
    uniqueUris.insert(resourceUrl);

    IdentificationSetGenerator ifg(uniqueUris, model, ignoreList);
    IdentificationSet is;
    is.d->m_statements = ifg.generate();
    return is;
}


Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromResourceList(const QList<QUrl> resList, Soprano::Model* model)
{
    IdentificationSetGenerator ifg( resList.toSet(), model );
    IdentificationSet is;
    is.d->m_statements = ifg.generate();
    return is;
}


namespace {
    bool isIdentifyingProperty( QUrl prop, Soprano::Model * model ) {
        QString query = QString::fromLatin1( "ask { %1 %2 %3 }" )
        .arg( Soprano::Node::resourceToN3( prop ) )
        .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::RDFS::subPropertyOf()) )
        .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::NRL::DefiningProperty()) );
        return model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
    }
}


Nepomuk2::IdentificationSet Nepomuk2::IdentificationSet::fromOnlyChangeLog(const Nepomuk2::ChangeLog& log )
{
    Soprano::Model * model = ResourceManager::instance()->mainModel();
    IdentificationSet is;

    foreach( const ChangeLogRecord record, log.toList() ) {
        QUrl propUri = record.st().predicate().uri();
        if( isIdentifyingProperty( propUri, model ) )
            is.d->m_statements << record.st();
    }
    return is;
}


bool Nepomuk2::IdentificationSet::save( const QUrl& output ) const
{
    QFile file( output.path() );
    if( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
        kWarning() << "File could not be opened : " << output.path();
        return false;
    }

    QTextStream out( &file );
    return save( out );
}


//TODO: We could probably use some kind of error return codes
bool Nepomuk2::IdentificationSet::save( QTextStream& out ) const
{
    if( d->m_statements.isEmpty() )
        return false;

    //
    // Serialize the statements and output them
    //
    const Soprano::Serializer * serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization( Soprano::SerializationNQuads );

    if( !serializer ) {
        kWarning() << "Could not find the required serializer";
        return false;
    }

    if( d->m_statements.empty() ) {
        kWarning() << "No statements to Serialize";
        return false;
    }

    Soprano::Util::SimpleStatementIterator it( d->m_statements );
    if( !serializer->serialize( it, out, Soprano::SerializationNQuads ) ) {
        kWarning() << "Serialization Failed";
        return false;
    }

    return true;
}


QList< Soprano::Statement > Nepomuk2::IdentificationSet::toList() const
{
    return d->m_statements;
}


void Nepomuk2::IdentificationSet::clear()
{
    d->m_statements.clear();
}

Nepomuk2::IdentificationSet& Nepomuk2::IdentificationSet::operator<<(const Nepomuk2::IdentificationSet& rhs)
{
    d->m_statements << rhs.d->m_statements;
    return *this;
}

void Nepomuk2::IdentificationSet::mergeWith(const IdentificationSet & rhs)
{
    this->d->m_statements << rhs.d->m_statements;
    return;
}

void Nepomuk2::IdentificationSet::createIdentificationSet(Soprano::Model* model, const QSet< QUrl >& uniqueUris, const QUrl& outputUrl)
{
    QFile file( outputUrl.path() );
    if( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
        kWarning() << "File could not be opened : " << outputUrl.path();
        return;
    }

    QTextStream out( &file );

    IdentificationSet set;
    IdentificationSetGenerator generator( uniqueUris, model );

    while( !generator.done() ) {
        generator.statements.clear();
        kDebug() << "iterating";
        generator.iterate();
        kDebug() << "Done : " << generator.m_done.size();
        kDebug() << "Num statements: " << generator.statements.size();
        set.d->m_statements.clear();
        set.d->m_statements = generator.statements;
        set.save( out );
    }
    kDebug() << "Done creating Identification Set";
}

