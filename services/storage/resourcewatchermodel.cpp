/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "resourcewatchermodel.h"
#include "resourcewatcherconnection.h"

#include <Soprano/Statement>

#include <QtDBus/QDBusConnection>
#include <KDebug>
#include <QStringList>

Nepomuk::ResourceWatcherModel::ResourceWatcherModel(Soprano::Model* parent)
    : FilterModel(parent)
{
    // Register the service
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    sessionBus.registerService("org.kde.nepomuk.ResourceWatcher");

    sessionBus.registerObject("/watcher", this, QDBusConnection::ExportScriptableSlots);
}

Soprano::Error::ErrorCode Nepomuk::ResourceWatcherModel::addStatement(const Soprano::Statement& statement)
{
    Soprano::Error::ErrorCode err = Soprano::FilterModel::addStatement(statement);

    /*const QUrl & subUri = statement.subject().uri();

    QHash<QUrl, ResourceWatcherConnection*>::iterator i = m_hash.find( subUri );
    if( i != m_hash.end() ) {
        ResourceWatcherConnection * res = i.value();
        emit res->statementAdded();
        emit res->statementAdded( statement );
    }*/

    return err;
}

Soprano::Error::ErrorCode Nepomuk::ResourceWatcherModel::removeAllStatements(const Soprano::Statement& statement)
{
    Soprano::Error::ErrorCode err = Soprano::FilterModel::removeAllStatements(statement);
    /*const QUrl & subUri = statement.subject().uri();
    
    QHash<QUrl, ResourceWatcherConnection*>::iterator i = m_hash.find( subUri );
    if( i != m_hash.end() ) {
        ResourceWatcherConnection * res = i.value();
        emit res->statementRemoved();
        emit res->statementRemoved( statement );

        if( statement.predicate().isEmpty() && statement.object().isEmpty() )
            emit res->removed();
    }*/

    return err;
}

Soprano::Error::ErrorCode Nepomuk::ResourceWatcherModel::removeStatement(const Soprano::Statement& statement)
{
    return removeAllStatements( statement );
}

QDBusObjectPath Nepomuk::ResourceWatcherModel::watchResource(const QStringList& resources, const QStringList& properties, const QStringList& types)
{
    kDebug();

    if( resources.isEmpty() && properties.isEmpty() && types.isEmpty() )
        return QDBusObjectPath();

    ResourceWatcherConnection * con = new ResourceWatcherConnection( this );
    foreach( const QString & res, resources ) {
        m_subHash.insert( QUrl(res), con );
    }

    foreach( const QString & prop, properties ) {
        m_propHash.insert( QUrl(prop), con );
    }

    foreach( const QString & type, properties ) {
        m_typeHash.insert( QUrl(type), con );
    }
    
    return con->dBusPath();
}

namespace {
    void remove( QMultiHash<QUrl, Nepomuk::ResourceWatcherConnection*> & hash,
                 const Nepomuk::ResourceWatcherConnection * con )
    {
        QMutableHashIterator<QUrl, Nepomuk::ResourceWatcherConnection*> it( hash );
        while( it.hasNext() ) {
            it.next();

            if( it.value() == con )
                it.remove();
        }
    }
}

void Nepomuk::ResourceWatcherModel::stopWatchingResource(const QString& objectName)
{
    kDebug();

    ResourceWatcherConnection * con = 0;
    QObjectList cl = children();
    foreach( QObject * obj, cl ) {
        ResourceWatcherConnection * c = static_cast<ResourceWatcherConnection*>( obj );
        if( c->objectName() == objectName ) {
            con = c;
            break;
        }
    }

    remove( m_subHash, con );
    remove( m_propHash, con );
    remove( m_typeHash, con );
}


