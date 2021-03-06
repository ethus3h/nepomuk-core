/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "basicindexingqueue.h"
#include "fileindexerconfig.h"
#include "util.h"
#include "indexer/simpleindexer.h"

#include "resourcemanager.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <KMimeType>
#include <QtCore/QDateTime>

namespace Nepomuk2 {

BasicIndexingQueue::BasicIndexingQueue(QObject* parent): IndexingQueue(parent)
{

}

void BasicIndexingQueue::clear()
{
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;
    m_paths.clear();
}

void BasicIndexingQueue::clear(const QString& path)
{
    QMutableVectorIterator< QPair<QString, UpdateDirFlags> > it( m_paths );
    while( it.hasNext() ) {
        it.next();
        if( it.value().first.startsWith( path ) )
            it.remove();
    }
}

QUrl BasicIndexingQueue::currentUrl() const
{
    return m_currentUrl;
}

UpdateDirFlags BasicIndexingQueue::currentFlags() const
{
    return m_currentFlags;
}


bool BasicIndexingQueue::isEmpty()
{
    return m_paths.isEmpty();
}

void BasicIndexingQueue::enqueue(const QString& path)
{
    UpdateDirFlags flags;
    flags |= UpdateRecursive;

    enqueue( path, flags );
}

void BasicIndexingQueue::enqueue(const QString& path, UpdateDirFlags flags)
{
    kDebug() << path;
    bool wasEmpty = m_paths.empty();
    m_paths.push( qMakePair( path, flags ) );
    callForNextIteration();

    if( wasEmpty )
        emit startedIndexing();
}

void BasicIndexingQueue::processNextIteration()
{
    bool processingFile = false;

    if( !m_paths.isEmpty() ) {
        QPair< QString, UpdateDirFlags > pair = m_paths.pop();
        processingFile = process( pair.first, pair.second );
    }

    if( !processingFile )
        finishIteration();
}


bool BasicIndexingQueue::process(const QString& path, UpdateDirFlags flags)
{
    bool startedIndexing = false;

    QUrl url = QUrl::fromLocalFile( path );
    QString mimetype = KMimeType::findByUrl( url )->name();

    bool forced = flags & ForceUpdate;
    bool recursive = flags & UpdateRecursive;
    bool indexingRequired = shouldIndex( path, mimetype );

    QFileInfo info( path );
    if( info.isDir() ) {
        if( forced || indexingRequired ) {
            m_currentUrl = url;
            m_currentFlags = flags;
            m_currentMimeType = mimetype;

            startedIndexing = true;
            index( path );
        }

        // We don't want to follow system links
        if( recursive && !info.isSymLink() && shouldIndexContents(path) ) {
            QDir::Filters dirFilter = QDir::NoDotAndDotDot|QDir::Readable|QDir::Files|QDir::Dirs;

            QDirIterator it( path, dirFilter );
            while( it.hasNext() ) {
                m_paths.push( qMakePair(it.next(), flags) );
            }
        }
    }
    else if( info.isFile() && (forced || indexingRequired) ) {
        m_currentUrl = url;
        m_currentFlags = flags;
        m_currentMimeType = mimetype;

        startedIndexing = true;
        index( path );
    }

    return startedIndexing;
}

bool BasicIndexingQueue::shouldIndex(const QString& path, const QString& mimetype)
{
    bool shouldIndexFile = FileIndexerConfig::self()->shouldFileBeIndexed( path );
    if( !shouldIndexFile )
        return false;

    bool shouldIndexType = FileIndexerConfig::self()->shouldMimeTypeBeIndexed( mimetype );
    if( !shouldIndexType )
        return false;

    QFileInfo fileInfo( path );
    if( !fileInfo.exists() )
        return false;

    bool needToIndex = false;

    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // Optimization: We don't care about the mtime of directories. If it has been indexed once
    // then it doesn't need to indexed again - ever
    if( fileInfo.isDir() ) {
        QString query = QString::fromLatin1("ask where { ?r nie:url %1 . }")
                        .arg( Soprano::Node::resourceToN3( QUrl::fromLocalFile(path) ) );

        needToIndex = !model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference ).boolValue();
    }
    else {
        // check if this file is new by checking its mtime
        QString query = QString::fromLatin1("ask where { ?r nie:url %1 ; nie:lastModified ?dt . FILTER(?dt=%2) .}")
                        .arg( Soprano::Node::resourceToN3( QUrl::fromLocalFile(path) ),
                            Soprano::Node::literalToN3( Soprano::LiteralValue(fileInfo.lastModified()) ) );

        needToIndex = !model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference ).boolValue();
    }

    if( needToIndex ) {
        kDebug() << path;
        return true;
    }

    return false;
}

bool BasicIndexingQueue::shouldIndexContents(const QString& dir)
{
    return FileIndexerConfig::self()->shouldFolderBeIndexed( dir );
}

void BasicIndexingQueue::index(const QString& path)
{
    kDebug() << path;
    const QUrl fileUrl = QUrl::fromLocalFile( path );
    emit beginIndexingFile( fileUrl );

    KJob* job = clearIndexedData( fileUrl );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotClearIndexedDataFinished(KJob*)) );
}

void BasicIndexingQueue::slotClearIndexedDataFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    SimpleIndexingJob* indexingJob = new SimpleIndexingJob( m_currentUrl, m_currentMimeType );
    indexingJob->start();

    connect( indexingJob, SIGNAL(finished(KJob*)), this, SLOT(slotIndexingFinished(KJob*)) );
}

void BasicIndexingQueue::slotIndexingFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    QUrl url = m_currentUrl;
    m_currentUrl.clear();
    m_currentMimeType.clear();
    m_currentFlags = NoUpdateFlags;

    emit endIndexingFile( url );

    // Continue the queue
    finishIteration();
}


}
