/*
  This file is part of the Nepomuk KDE project.
  Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License version 2 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "searchrunnable.h"
#include "folder.h"

#include "resourcemanager.h"
#include "resource.h"

#include <Soprano/Version>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Node>
#include <Soprano/Statement>
#include <Soprano/LiteralValue>
#include <Soprano/StatementIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/XMLSchema>
#include <Soprano/Vocabulary/OWL>
#include <Soprano/Vocabulary/Xesam>
#include "nfo.h"

#include <KDebug>
#include <KDateTime>
#include <KRandom>

#include <QtCore/QTime>
#include <QtCore/QRegExp>
#include <QtCore/QLatin1String>
#include <QtCore/QStringList>


Nepomuk2::Query::SearchRunnable::SearchRunnable( Soprano::Model* model, Nepomuk2::Query::Folder* folder )
    : QRunnable(),
      m_model( model ),
      m_folder( folder )
{
}


Nepomuk2::Query::SearchRunnable::~SearchRunnable()
{
}


void Nepomuk2::Query::SearchRunnable::cancel()
{
    // "detach" us from the folder which will most likely be deleted now
    QMutexLocker lock( &m_folderMutex );
    m_folder = 0;
}


void Nepomuk2::Query::SearchRunnable::run()
{
    QMutexLocker lock( &m_folderMutex );
    if( !m_folder )
        return;
    kDebug() << m_folder->query() << m_folder->sparqlQuery();
    const QString sparql = m_folder->sparqlQuery();
    lock.unlock();

#ifndef NDEBUG
    QTime time;
    time.start();
#endif

    Soprano::QueryResultIterator hits = m_model->executeQuery( sparql, Soprano::Query::QueryLanguageSparql );
    while ( m_folder &&
            hits.next() ) {
        Result result = extractResult( hits );

        kDebug() << "Found result:" << result.resource().uri() << result.score();

        lock.relock();
        if( m_folder ) {
            QList<Nepomuk2::Query::Result> results;
            results << result;
            QMetaObject::invokeMethod( m_folder, "addResults", Qt::QueuedConnection, Q_ARG( QList<Nepomuk2::Query::Result>, results ) );
        }
        lock.unlock();
    }

#ifndef NDEBUG
    kDebug() << time.elapsed();
#endif

    lock.relock();
    if( m_folder ) {
        QMetaObject::invokeMethod( m_folder, "listingFinished", Qt::QueuedConnection );
    }
}


Nepomuk2::Query::Result Nepomuk2::Query::SearchRunnable::extractResult( const Soprano::QueryResultIterator& it ) const
{
    Result result( Resource::fromResourceUri( it[0].uri() ) );

    // make sure we do not store values twice
    QStringList names = it.bindingNames();
    names.removeAll( QLatin1String( "r" ) );

    m_folderMutex.lock();
    if( m_folder ) {
        RequestPropertyMap requestProperties = m_folder->requestPropertyMap();
        for ( RequestPropertyMap::const_iterator rpIt = requestProperties.constBegin();
             rpIt != requestProperties.constEnd(); ++rpIt ) {
            result.addRequestProperty( rpIt.value(), it.binding( rpIt.key() ) );
            names.removeAll( rpIt.key() );
        }
    }
    m_folderMutex.unlock();

    static const char* s_scoreVarName = "_n_f_t_m_s_";
    static const char* s_excerptVarName = "_n_f_t_m_ex_";

    Soprano::BindingSet set;
    int score = 0;
    Q_FOREACH( const QString& var, names ) {
        if ( var == QLatin1String( s_scoreVarName ) )
            score = it[var].literal().toInt();
        else if ( var == QLatin1String( s_excerptVarName ) )
            result.setExcerpt( it[var].toString() );
        else
            set.insert( var, it[var] );
    }

    result.setAdditionalBindings( set );
    result.setScore( ( double )score );

    // score will be set above
    return result;
}