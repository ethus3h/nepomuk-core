/* This file is part of the KDE Project
   Copyright (c) 2007 Sebastian Trueg <trueg@kde.org>

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

#include "nepomukcore.h"
#include "repository.h"
#include "ontologyloader.h"

#include <KStandardDirs>
#include <KDebug>

#include <QtCore/QTimer>


Nepomuk::Core::Core( QObject* parent )
    : Soprano::Server::ServerCore( parent )
{
    m_ontologyLoader = new OntologyLoader( model( "main" ), this );
    QTimer::singleShot( 0, m_ontologyLoader, SLOT( update() ) );

    registerAsDBusObject();
}


Nepomuk::Core::~Core()
{
    kDebug(300002) << "Shutting down Nepomuk core services.";
    qDeleteAll( m_repositories );
}


Soprano::Model* Nepomuk::Core::model( const QString& name )
{
    return repository( name );
}


QStringList Nepomuk::Core::allModels() const
{
    QStringList models;
    foreach( Repository* repo, m_repositories ) {
        models.append( repo->name() );
    }
    return models;
}


Nepomuk::Repository* Nepomuk::Core::repository( const QString& name )
{
    if ( !m_repositories.contains( name ) ) {
        kDebug(300002) << "Creating new repository with name " << name;
        QString storagePath = createStoragePath( name );

        Repository* newRepo = Repository::open( storagePath, name );
        if( newRepo ) {
            m_repositories.insert( name, newRepo );
            return newRepo;
        }
        else {
            return 0;
        }
    }
    else {
        return m_repositories[name];
    }
}


QString Nepomuk::Core::createStoragePath( const QString& repositoryId ) const
{
    return KStandardDirs::locateLocal( "data", "nepomuk/repository/" + repositoryId + "/" );
}

#include "nepomukcore.moc"
