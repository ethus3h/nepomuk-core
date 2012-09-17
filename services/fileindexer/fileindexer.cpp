/* This file is part of the KDE Project
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2010-2011 Vishesh Handa <handa.vish@gmail.com>

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

#include "fileindexer.h"
#include "fileindexeradaptor.h"
#include "indexscheduler.h"
#include "eventmonitor.h"
#include "fileindexerconfig.h"
#include "filewatchserviceinterface.h"
#include "util.h"

#include <KDebug>
#include <KDirNotify>
#include <KLocale>

#include "resourcemanager.h"

#include <QtCore/QTimer>

Nepomuk2::FileIndexer::FileIndexer( QObject* parent, const QList<QVariant>& )
    : Service( parent )
{
    // Create the configuration instance singleton (for thread-safety)
    // ==============================================================
    (void)new FileIndexerConfig(this);

    // setup the actual index scheduler
    // ==============================================================
    m_indexScheduler = new IndexScheduler(this);

    // monitor all kinds of events
    ( void )new EventMonitor( m_indexScheduler, this );

    // update the watches if the config changes
    connect( FileIndexerConfig::self(), SIGNAL( configChanged() ),
             this, SLOT( updateWatches() ) );

    // export on dbus
    ( void )new FileIndexerAdaptor( this );

    // setup status connections
    connect( m_indexScheduler, SIGNAL( indexingStarted() ),
             this, SIGNAL( statusStringChanged() ) );
    connect( m_indexScheduler, SIGNAL( indexingStopped() ),
             this, SIGNAL( statusStringChanged() ) );
    connect( m_indexScheduler, SIGNAL( indexingDone() ),
             this, SLOT( slotIndexingDone() ) );
    connect( m_indexScheduler, SIGNAL( indexingFolder(QString) ),
             this, SIGNAL( statusStringChanged() ) );
    connect( m_indexScheduler, SIGNAL( indexingFile(QString) ),
             this, SIGNAL( statusStringChanged() ) );
    connect( m_indexScheduler, SIGNAL( indexingSuspended(bool) ),
             this, SIGNAL( statusStringChanged() ) );

    // start initial indexing honoring the hidden config option to disable it
    m_indexScheduler->suspend();
    if( FileIndexerConfig::self()->isInitialRun() || !FileIndexerConfig::self()->initialUpdateDisabled() ) {
        m_indexScheduler->updateAll();
    }

    // Creation of watches is a memory intensive process as a large number of
    // watch file descriptors need to be created ( one for each directory )
    // So we start it after 2 minutes in order to reduce startup time
    // FIXME: Add the watches in the file watcher
    QTimer::singleShot( 2 * 60 * 1000, this, SLOT( updateWatches() ) );

    // Connect some signals used in the DBus interface
    connect( this, SIGNAL( statusStringChanged() ),
             this, SIGNAL( statusChanged() ) );
    connect( m_indexScheduler, SIGNAL( indexingStarted() ),
             this, SIGNAL( indexingStarted() ) );
    connect( m_indexScheduler, SIGNAL( indexingStopped() ),
             this, SIGNAL( indexingStopped() ) );
    connect( m_indexScheduler, SIGNAL( indexingFolder(QString) ),
             this, SIGNAL( indexingFolder(QString) ) );
}


Nepomuk2::FileIndexer::~FileIndexer()
{
}

void Nepomuk2::FileIndexer::slotIndexingDone()
{
    FileIndexerConfig::self()->setInitialRun(false);
}


void Nepomuk2::FileIndexer::updateWatches()
{
    org::kde::nepomuk::FileWatch filewatch( "org.kde.nepomuk.services.nepomukfilewatch",
                                            "/nepomukfilewatch",
                                            QDBusConnection::sessionBus() );
    foreach( const QString& folder, FileIndexerConfig::self()->includeFolders() ) {
        filewatch.watchFolder( folder );
    }
}


QString Nepomuk2::FileIndexer::userStatusString() const
{
    return userStatusString( false );
}


QString Nepomuk2::FileIndexer::simpleUserStatusString() const
{
    return userStatusString( true );
}


QString Nepomuk2::FileIndexer::userStatusString( bool simple ) const
{
    bool indexing = m_indexScheduler->isIndexing();
    bool suspended = m_indexScheduler->isSuspended();

    if ( suspended ) {
        return i18nc( "@info:status", "File indexer is suspended." );
    }
    else if ( indexing ) {
        QString folder = m_indexScheduler->currentFolder();
        bool autoUpdate =  m_indexScheduler->currentFlags() & AutoUpdateFolder;

        if ( folder.isEmpty() || simple ) {
            if( autoUpdate ) {
                return i18nc( "@info:status", "Scanning for recent changes in files for desktop search");
            }
            else {
                return i18nc( "@info:status", "Indexing files for desktop search." );
            }
        }
        else {
            if( autoUpdate ) {
                return i18nc( "@info:status", "Scanning for recent changes in %1", folder );
            }
            else {
                if( m_indexScheduler->currentFile().isEmpty() )
                    return i18nc( "@info:status", "Indexing files in %1", folder );
                else
                    return i18nc( "@info:status", "Indexing %1", m_indexScheduler->currentFile() );
            }
        }
    }
    else {
        return i18nc( "@info:status", "File indexer is idle." );
    }
}


void Nepomuk2::FileIndexer::setSuspended( bool suspend )
{
    if ( suspend ) {
        m_indexScheduler->suspend();
    }
    else {
        m_indexScheduler->resume();
    }
}


bool Nepomuk2::FileIndexer::isSuspended() const
{
    return m_indexScheduler->isSuspended();
}


bool Nepomuk2::FileIndexer::isIndexing() const
{
    return m_indexScheduler->isIndexing();
}


void Nepomuk2::FileIndexer::suspend() const
{
    m_indexScheduler->suspend();
}


void Nepomuk2::FileIndexer::resume() const
{
    m_indexScheduler->resume();
}


QString Nepomuk2::FileIndexer::currentFile() const
{
   return m_indexScheduler->currentFile();
}


QString Nepomuk2::FileIndexer::currentFolder() const
{
    return m_indexScheduler->currentFolder();
}


void Nepomuk2::FileIndexer::updateFolder(const QString& path, bool recursive, bool forced)
{
    kDebug() << "Called with path: " << path;
    QFileInfo info( path );
    if ( info.exists() ) {
        QString dirPath;
        if ( info.isDir() )
            dirPath = info.absoluteFilePath();
        else
            dirPath = info.absolutePath();

        if ( FileIndexerConfig::self()->shouldFolderBeIndexed( dirPath ) ) {
            indexFolder(path, recursive, forced);
        }
    }
}


void Nepomuk2::FileIndexer::updateAllFolders(bool forced)
{
    m_indexScheduler->updateAll( forced );
}


void Nepomuk2::FileIndexer::indexFile(const QString& path)
{
    m_indexScheduler->analyzeFile( path );
}


void Nepomuk2::FileIndexer::indexFolder(const QString& path, bool recursive, bool forced)
{
    QFileInfo info( path );
    if ( info.exists() ) {
        QString dirPath;
        if ( info.isDir() )
            dirPath = info.absoluteFilePath();
        else
            dirPath = info.absolutePath();

        kDebug() << "Updating : " << dirPath;

        Nepomuk2::UpdateDirFlags flags;
        if(recursive)
            flags |= Nepomuk2::UpdateRecursive;
        if(forced)
            flags |= Nepomuk2::ForceUpdate;

        m_indexScheduler->updateDir( dirPath, flags );
    }
}



#include <kpluginfactory.h>
#include <kpluginloader.h>

NEPOMUK_EXPORT_SERVICE( Nepomuk2::FileIndexer, "nepomukfileindexer" )

#include "fileindexer.moc"

