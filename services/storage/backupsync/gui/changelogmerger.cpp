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



#include "changelogmerger.h"
#include "nrio.h"

#include <algorithm>

#include <QtCore/QMultiHash>
#include <QtCore/QHashIterator>
#include <QtCore/QThread>

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>
#include "nie.h"
#include "nfo.h"

#include <Soprano/Node>
#include <Soprano/Statement>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>

#include "resource.h"
#include "variant.h"
#include "types/property.h"
#include "resourcemanager.h"

#include <KDebug>

int Nepomuk2::ChangeLogMerger::NextId = 0;

Nepomuk2::ChangeLogMerger::ChangeLogMerger(Nepomuk2::ChangeLog log)
    : ResourceMerger(),
      m_logFile( log )
{
    m_id = NextId++;
}

int Nepomuk2::ChangeLogMerger::id()
{
    return m_id;
}

void Nepomuk2::ChangeLogMerger::load()
{
    kDebug() << "Loading the ChangeLog..." << m_logFile.size();
    m_hash = ResourceLogMap::fromChangeLog( m_logFile );

    // The records are stored according to dateTime
    Q_ASSERT( m_logFile.toList().isEmpty() == false );
    m_minDateTime = m_logFile.toList().first().dateTime();
}

namespace {

    //
    // Cache the results. This could have very bad consequences if someone updates the ontology
    // when the service is running
    //
    bool isMergeable( const KUrl & prop, Soprano::Model * model ) {
        //
        // trueg: hardcoding the non-mergeable properties here since there are only 2 defined
        //
        return prop != Soprano::Vocabulary::RDF::type() && prop != Nepomuk2::Vocabulary::NIE::url();
    }

    QList<Nepomuk2::ChangeLogRecord> getRecords( const Nepomuk2::ResourceLogMap & hash, const KUrl resUri, const KUrl & propUri ) {

        Nepomuk2::ResourceLogMap::const_iterator it = hash.constFind( resUri );
        if( it == hash.constEnd() ) {
            return QList<Nepomuk2::ChangeLogRecord>();
        }

        return it->prop.values( propUri );
    }
}

//TODO: Add completed signal
void Nepomuk2::ChangeLogMerger::mergeChangeLog()
{
    m_theGraph = createGraph();

    kDebug();

    //
    // Get own changeLog
    //
    kDebug() << "minDateTime : " << m_minDateTime;
    ChangeLog ownLog;// = LogStorage::instance()->getChangeLog( m_minDateTime );
    kDebug() << "own Log : " << ownLog.size();

    // Get our and their hash
    // ownHash = current local hash from system's ChangeLog
    // theirHash = derived from external ChangeLog
    ResourceLogMap ownHash = ResourceLogMap::fromChangeLog( ownLog );
    ResourceLogMap & theirHash = m_hash;

    kDebug() << "own Hash : " << ownHash.size();
    kDebug() << "their hash : " << theirHash.size();

    QHashIterator<KUrl, ResourceLog> it( theirHash );
    while( it.hasNext() ) {
        it.next();

        // Check for resource deletions
        if( handleResourceDeletion( it.key() ) )
            continue;

        const KUrl & resUri = it.key();
        const ResourceLog & resLog = it.value();

        kDebug() << "Resolving " << resUri;

        const QList<KUrl> & properties = resLog.prop.uniqueKeys();
        foreach( const KUrl & propUri, properties ) {
            kDebug() << propUri;

            if( !isMergeable( propUri, model() ) ) {
                kDebug() << propUri << " is non Mergeable - IGNORING";
                continue;
            }

            Nepomuk2::Types::Property prop( propUri );
            int cardinality = prop.maxCardinality();

            QList<ChangeLogRecord> theirRecords = resLog.prop.values( propUri );
            QList<ChangeLogRecord> ownRecords = getRecords( ownHash, resUri, propUri );
            //kDebug() << "own Records : " << ownRecords.size();

            // This case shouldn't ever happen, but just to be sure
            if( theirRecords.empty() )
                continue;

            if( cardinality == 1 ) {
                resolveSingleCardinality( theirRecords, ownRecords );
            }
            else {
                resolveMultipleCardinality( theirRecords, ownRecords );
            }
        }

        //if( !rs.propHash.isEmpty() )
        //    m_jobs.append( rs );
    }
    //theirHash.clear();
    //kDebug() << "Done with merge resolution : " << m_jobs.size();

    //processJobs();
}


namespace {

    Nepomuk2::ChangeLogRecord maxRecord( const QList<Nepomuk2::ChangeLogRecord> & records ) {
        QList<Nepomuk2::ChangeLogRecord>::const_iterator it = std::max_element( records.begin(), records.end() );
        if( it != records.constEnd() )
            return *it;
        return Nepomuk2::ChangeLogRecord();
    }
}


void Nepomuk2::ChangeLogMerger::resolveSingleCardinality(const QList< Nepomuk2::ChangeLogRecord >& theirRecords, const QList< Nepomuk2::ChangeLogRecord >& ownRecords)
{
    kDebug() << "O: " << ownRecords.size() << " " << "T:" << theirRecords.size();

    //Find max on the basis of time stamp
    ChangeLogRecord theirMax = maxRecord( theirRecords );
    ChangeLogRecord ownMax = maxRecord( ownRecords );
    kDebug() << "TheirMax : "<< theirMax.toString();
    kDebug() << "OwnMax " << ownMax.toString();

    if( theirMax > ownMax ) {
        Soprano::Statement statement( theirMax.st().subject(), theirMax.st().predicate(),
                                      Soprano::Node(), Soprano::Node() );

        if( theirMax.added() ) {
            Soprano::Node object = theirMax.st().object();
            kDebug() << "Resolved - Adding " << object;

            if( !model()->containsAnyStatement( statement ) ) {
                statement.setObject( object );
                statement.setContext( m_theGraph );
                model()->addStatement( statement );
            }
        }
        else {
            kDebug() << "Resolved - Removing";
            model()->removeAllStatements( statement );
        }
    }
}

namespace {

    struct MergeData {
        bool added;
        QDateTime dateTime;

        MergeData( bool add, const QDateTime & dt )
            : added( add ),
              dateTime( dt )
        {}
    };


}

void Nepomuk2::ChangeLogMerger::resolveMultipleCardinality( const QList<Nepomuk2::ChangeLogRecord>& theirRecords, const QList<Nepomuk2::ChangeLogRecord>& ownRecords)
{
    kDebug() << "MULTIPLE";
    kDebug() << "O: " << ownRecords.size() << " " << "T:" << theirRecords.size();

    const Soprano::Statement& reference = theirRecords.first().st();
    Soprano::Statement baseStatement( reference.subject(), reference.predicate(), Soprano::Node(), Soprano::Node() );

    //
    // Merge both record lists
    //
    //TODO: Optimize merging - use merge sort or something equivilant
    QList<ChangeLogRecord> records = ownRecords;
    records << theirRecords;
    qSort( records );

    QHash<Soprano::Node, MergeData> hash;
    foreach( const ChangeLogRecord rec, records ) {
        Soprano::Node object = rec.st().object();
        QHash<Soprano::Node, MergeData>::const_iterator it = hash.constFind( object );
        if( it == hash.constEnd() ) {
            hash.insert( object, MergeData( rec.added(), rec.dateTime() ) );
        }
        else {
            // +ve after -ve
            if( rec.added() == true && it.value().added == false ) {
                hash.remove( object );
                hash.insert( object, MergeData( rec.added(), rec.dateTime() ) );
            }
            // -ve after +ve
            else if( rec.added() == false && it.value().added == true ) {
                hash.remove( object );
            }
            // +ve after +ve
            // -ve after -ve
            //    Do nothing
        }
    }

    //
    // Do the actual merging
    //
    QHashIterator<Soprano::Node, MergeData> it( hash );
    while( it.hasNext() ) {
        it.next();

        Soprano::Statement st( baseStatement );
        st.setObject( it.key() );

        MergeData data = it.value();
        if( data.added == true ) {
            if( !model()->containsAnyStatement( st ) ) {
                st.setContext( m_theGraph );
                model()->addStatement( st );
                kDebug() << "adding - " << st;
            }
        }
        else {
            kDebug() << "removing " << st;
            model()->removeAllStatements( st );
        }
    }

    m_multipleMergers.append( Soprano::Statement( baseStatement.subject(),
                                                  baseStatement.predicate(),
                                                  Soprano::Node() ) );
}

QList< Soprano::Statement > Nepomuk2::ChangeLogMerger::multipleMergers() const
{
    return m_multipleMergers;
}

bool Nepomuk2::ChangeLogMerger::handleResourceDeletion(const KUrl& resUri)
{
    ResourceLog & log = m_hash[ resUri ];
    const KUrl& rdfTypeProp = Soprano::Vocabulary::RDF::type();

    QList<ChangeLogRecord> records = log.prop.values( rdfTypeProp );
    if( records.empty() )
        return false;

    //
    // Check if rdf:type is being removed
    //
    bool removed = false;
    foreach( const ChangeLogRecord & r, records ) {
        if( !r.added() ) {
            removed = true;
            break;
        }
    }
    if( !removed )
        return false;

    // If removed, remove all records and delete the resource
    m_hash.remove( resUri );
    Resource res( resUri );
    res.remove();
    return true;
}