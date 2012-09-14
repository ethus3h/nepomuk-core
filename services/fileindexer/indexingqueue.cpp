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


#include "indexingqueue.h"

#include <QtCore/QTimer>
#include <KDebug>

namespace Nepomuk2 {


IndexingQueue::IndexingQueue(QObject* parent): QObject(parent)
{
    m_sentEvent = false;
    m_suspended = false;
}

void IndexingQueue::processNext()
{
    if( m_suspended )
        return;

    // First process all the iterators and then the paths
    if( !m_iterators.isEmpty() ) {
        QDirIterator* it = m_iterators.first();

        if( it->hasNext() ) {
            process( it->next() );
        }
        else {
            delete m_iterators.dequeue();
        }
    }

    else if( !m_paths.isEmpty() ) {
        QString path = m_paths.dequeue();
        process( path );
    }

    m_sentEvent = false;
    callForNextIteration();
}

void IndexingQueue::process(const QString& path)
{
    QFileInfo info( path );
    if( info.isDir() ) {

        if( shouldIndex(path) ) {
            emit beginIndexing( path );
            indexDir( path );
            emit endIndexing( path );
        }

        if( shouldIndexContents(path) ) {
            QDir::Filters dirFilter = QDir::NoDotAndDotDot|QDir::Readable|QDir::Files|QDir::Dirs;
            m_iterators.enqueue( new QDirIterator( path, dirFilter ) );
        }
    }
    else if( info.isFile() && shouldIndex(path) ) {
        emit beginIndexing( path );
        indexFile( path );
        emit endIndexing( path );
    }
}

void IndexingQueue::enqueue(const QString& path)
{
    m_paths.enqueue( path );

    if( !m_suspended ) {
        callForNextIteration();
    }
}

void IndexingQueue::resume()
{
    m_suspended = false;
    callForNextIteration();
}

void IndexingQueue::suspend()
{
    m_suspended = true;
}

void IndexingQueue::callForNextIteration()
{
    bool queuesEmpty = m_iterators.isEmpty() && m_paths.isEmpty();

    if( !m_sentEvent && !queuesEmpty ) {
        QTimer::singleShot( 0, this, SLOT(processNext()) );
        m_sentEvent = true;
    }
}

}
