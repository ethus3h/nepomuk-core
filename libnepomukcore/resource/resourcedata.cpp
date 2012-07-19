/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2010 Sebastian Trueg <trueg@kde.org>
 * Copyright (C) 2010-2012 Vishesh Handa <handa.vish@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "resourcedata.h"
#include "resourcemanager.h"
#include "resourcemanager_p.h"
#include "resource.h"
#include "tools.h"
#include "nie.h"
#include "nfo.h"
#include "pimo.h"
#include "nepomukmainmodel.h"
#include "class.h"
#include "datamanagement.h"
#include "createresourcejob.h"
#include "resourcewatcher.h"

#include <Soprano/Statement>
#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/Xesam>
#include <Soprano/Vocabulary/NAO>

#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QMutexLocker>

#include <kdebug.h>
#include <kurl.h>
#include <kcomponentdata.h>

using namespace Soprano;

#define MAINMODEL (m_rm->m_manager->mainModel())

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

Nepomuk2::ResourceData::ResourceData( const QUrl& uri, const QUrl& kickOffUri, const QUrl& type, ResourceManagerPrivate* rm )
    : m_uri(uri),
      m_type(type),
      m_modificationMutex(QMutex::Recursive),
      m_cacheDirty(false),
      m_addedToWatcher(false),
      m_rm(rm)
{
    if( type.isEmpty() )
        m_type = RDFS::Resource();

    m_rm->dataCnt.ref();

    if( !uri.isEmpty() ) {
        m_cacheDirty = true;
        m_rm->m_initializedData.insert( uri, this );
        m_kickoffUris.insert( uri );
    }
    if( !kickOffUri.isEmpty() ) {
        m_kickoffUris.insert( kickOffUri );

        if( kickOffUri.scheme().isEmpty() ) {
            // Label
            const QString label = kickOffUri.toString();
            m_cache.insert( Soprano::Vocabulary::NAO::identifier(), label );
        }
        else if( kickOffUri.scheme() != QLatin1String("nepomuk") ) {
            // It's probably the nie:url
            m_cache.insert( Nepomuk2::Vocabulary::NIE::url(), kickOffUri );
        }
    }
    m_rm->addToKickOffList( this, m_kickoffUris );
}


Nepomuk2::ResourceData::~ResourceData()
{
    resetAll(true);
    m_rm->dataCnt.deref();
}


bool Nepomuk2::ResourceData::isFile()
{
    return( m_uri.scheme() == QLatin1String("file") ||
            m_nieUrl.scheme() == QLatin1String("file") ||
            (!m_kickoffUris.isEmpty() && (*m_kickoffUris.begin()).scheme() == QLatin1String("file")) ||
            hasProperty( RDF::type(), NFO::FileDataObject() ) );
    // The hasProperty should be const - It shouldn't load the entire cache. Maybe
}


QUrl Nepomuk2::ResourceData::uri() const
{
    return m_uri;
}


QUrl Nepomuk2::ResourceData::type()
{
    load();

    QUrl mainType = RDFS::Resource();
    QList<QUrl> types = m_cache[RDF::type()].toUrlList();
    foreach(const QUrl& t, types) {
        Types::Class currentTypeClass = mainType;
        Types::Class storedTypeClass = t;

        // Keep the type that is further down the hierarchy
        if ( storedTypeClass.isSubClassOf( currentTypeClass ) ) {
            mainType = storedTypeClass.uri();
        }
        else {
            // This is a little convenience hack since the user is most likely
            // more interested in the file content than the actual file
            // the same is true for nie:DataObject vs. nie:InformationElement
            Types::Class nieInformationElementClass( NIE::InformationElement() );
            Types::Class nieDataObjectClass( NIE::DataObject() );
            if( ( currentTypeClass == nieDataObjectClass ||
                  currentTypeClass.isSubClassOf( nieDataObjectClass ) ) &&
                ( storedTypeClass == nieInformationElementClass ||
                  storedTypeClass.isSubClassOf( nieInformationElementClass ) ) ) {
                mainType = storedTypeClass.uri();
            }
        }
    }

    return mainType;
}


void Nepomuk2::ResourceData::resetAll( bool isDelete )
{
    // remove us from all caches (store() will re-insert us later if necessary)
    m_rm->mutex.lock();

    // IMPORTANT:
    // Remove from the kickOffList before removing from the resource watcher
    // This is required cause otherwise the Resource::fromResourceUri creates a new
    // resource which is correctly identified to the ResourceData (this), and it is
    // then deleted, which calls resetAll and this cycle continues.
    Q_FOREACH( const KUrl& uri, m_kickoffUris )
        m_rm->m_uriKickoffData.remove( uri );

    if( !m_uri.isEmpty() ) {
        m_rm->m_initializedData.remove( m_uri );
        if( m_rm->m_watcher && m_addedToWatcher ) {
            // See load() for an explanation of the QMetaObject call

            // stop the watcher since we do not want to watch all changes in case there is no ResourceData left
            if(m_rm->m_watcher->resources().count() == 1) {
                QMetaObject::invokeMethod(m_rm->m_watcher, "stop", Qt::AutoConnection);
            }

            // remove this Resource from the list of watched resources
            QMetaObject::invokeMethod(m_rm->m_watcher, "removeResource", Qt::AutoConnection, Q_ARG(Nepomuk2::Resource, Resource::fromResourceUri(m_uri)));
            m_addedToWatcher = false;
        }
    }
    m_rm->mutex.unlock();

    // reset all variables
    m_uri = QUrl();
    m_nieUrl = KUrl();
    m_kickoffUris.clear();
    m_cache.clear();
    m_cacheDirty = false;
    m_type.clear();
}


QHash<QUrl, Nepomuk2::Variant> Nepomuk2::ResourceData::allProperties()
{
    load();
    return m_cache;
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& uri )
{
    load();
    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( uri );
    if( it == m_cache.constEnd() )
        return false;

    return true;
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& p, const Variant& v )
{
    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( p );
    if( it == m_cache.constEnd() )
        return false;

    QList<Variant> thisVals = it.value().toVariantList();
    QList<Variant> vals = v.toVariantList();
    Q_FOREACH( const Variant& val, vals ) {
        if( !thisVals.contains(val) )
            return false;
    }
    return true;
}


Nepomuk2::Variant Nepomuk2::ResourceData::property( const QUrl& uri )
{
    load();

    // we need to protect the reading, too. load my be triggered from another thread's
    // connection to a Soprano statement signal
    QMutexLocker lock(&m_modificationMutex);

    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( uri );
    if ( it == m_cache.constEnd() ) {
        return Variant();
    }
    else {
        return *it;
    }
}


bool Nepomuk2::ResourceData::store()
{
    QMutexLocker lock(&m_modificationMutex);

    if ( m_uri.isEmpty() ) {
        QMutexLocker rmlock(&m_rm->mutex);

        QList<QUrl> types;
        if ( m_nieUrl.isValid() &&
             m_nieUrl.isLocalFile() &&
             m_type != NFO::FileDataObject() ) {
            types << NFO::FileDataObject();
        }

        types << m_type;
        Nepomuk2::CreateResourceJob* job = Nepomuk2::createResource(types, QString(), QString());
        if( !job->exec() ) {
            //TODO: Set the error somehow
            kWarning() << job->errorString();
            return false;
        }
        else {
            m_uri = job->resourceUri();
            m_cache.insert(RDF::type(), types);
        }

        // Add us to the initialized data, i.e. make us "valid"
        m_rm->m_initializedData.insert( m_uri, this );

        // each initialized resource has to be in a kickoff list
        // thus, we make sure that is the case.
        if( m_kickoffUris.isEmpty() ) {
            m_kickoffUris.insert( m_uri );
            m_rm->addToKickOffList( this, m_kickoffUris );
        }

        foreach( const KUrl& url, m_kickoffUris ) {
            if( url.scheme().isEmpty() )
                setProperty( Soprano::Vocabulary::NAO::identifier(), Variant(url.url()) );
            else
                setProperty( Nepomuk2::Vocabulary::NIE::url(), Variant(url.url()) );
        }
    }

    return true;
}


bool Nepomuk2::ResourceData::load()
{
    QMutexLocker lock(&m_modificationMutex);

    if ( m_cacheDirty ) {
        m_cache.clear();

        if(!m_rm->m_watcher) {
            m_rm->m_watcher = new ResourceWatcher(m_rm->m_manager);
            //
            // The ResourceWatcher is not thread-safe. Thus, we need to ensure the safety ourselves.
            // We do that by simply handling all RW related operations in the manager thread.
            // This also means to invoke methods on the watcher through QMetaObject to make sure they
            // get queued in case of calls between different threads.
            //
            m_rm->m_watcher->moveToThread(m_rm->m_manager->thread());
            QObject::connect( m_rm->m_watcher, SIGNAL(propertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                              m_rm->m_manager, SLOT(slotPropertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
            QObject::connect( m_rm->m_watcher, SIGNAL(propertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                              m_rm->m_manager, SLOT(slotPropertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
            m_rm->m_watcher->addResource( Nepomuk2::Resource::fromResourceUri(m_uri) );
        }
        else {
            QMetaObject::invokeMethod(m_rm->m_watcher, "addResource", Qt::AutoConnection, Q_ARG(Nepomuk2::Resource, Nepomuk2::Resource::fromResourceUri(m_uri)) );
        }
        // (re-)start the watcher in case this resource is the only one in the list of watched
        if(m_rm->m_watcher->resources().count() <= 1) {
            QMetaObject::invokeMethod(m_rm->m_watcher, "start", Qt::AutoConnection);
        }
        m_addedToWatcher = true;

        if ( m_uri.isValid() ) {
            //
            // We exclude properties that are part of the inference graph
            // It would only pollute the user interface
            //
            Soprano::QueryResultIterator it = MAINMODEL->executeQuery(QString("select distinct ?p ?o where { "
                                                                              "%1 ?p ?o . }").arg(Soprano::Node::resourceToN3(m_uri)),
                                                                      Soprano::Query::QueryLanguageSparqlNoInference);
            while ( it.next() ) {
                QUrl p = it["p"].uri();
                Soprano::Node o = it["o"];
                Nepomuk2::Variant var = Variant::fromNode( o );
                updateKickOffLists( p, var );
                m_cache[p].append( var );
            }

            m_cacheDirty = false;
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return true;
    }
}


void Nepomuk2::ResourceData::setProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( store() ) {
        // step 0: make sure this resource is in the store
        QMutexLocker lock(&m_modificationMutex);

        // update the store
        QVariantList varList;
        foreach( const Nepomuk2::Variant var, value.toVariantList() ) {
            // make sure resource values are in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                var.toResource().m_data->store();
                varList << var.toUrl();
            }
            else {
                varList << var.variant();
            }
        }

        KJob* job = Nepomuk2::setProperty(QList<QUrl>() << m_uri, uri, varList);
        if( !job->exec() ) {
            //TODO: Set the error somehow
            kWarning() << job->errorString();
            return;
        }

        // update the cache for now
        if( value.isValid() )
            m_cache[uri] = value;
        else
            m_cache.remove(uri);

        // update the kickofflists
        updateKickOffLists( uri, value );
    }
}


void Nepomuk2::ResourceData::addProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( value.isValid() && store() ) {
        // step 0: make sure this resource is in the store
        QMutexLocker lock(&m_modificationMutex);

        // update the store
        QVariantList varList;
        foreach( const Nepomuk2::Variant var, value.toVariantList() ) {
            // make sure resource values are in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                var.toResource().m_data->store();
                varList << var.toUrl();
            }
            else {
                varList << var.variant();
            }
        }

        KJob* job = Nepomuk2::addProperty(QList<QUrl>() << m_uri, uri, varList);
        if( !job->exec() ) {
            //TODO: Set the error somehow
            kWarning() << job->errorString();
            return;
        }

        // update the cache for now
        if( value.isValid() )
            m_cache[uri].append(value);

        // update the kickofflists
        updateKickOffLists( uri, value );
    }
}


void Nepomuk2::ResourceData::removeProperty( const QUrl& uri )
{
    Q_ASSERT( uri.isValid() );
    if( !m_uri.isEmpty() ) {
        QMutexLocker lock(&m_modificationMutex);

        KJob* job = Nepomuk2::removeProperties(QList<QUrl>() << m_uri, QList<QUrl>() << uri);
        if( !job->exec() ) {
            //TODO: Set the error somehow
            kWarning() << job->errorString();
            return;
        }

        // Update the cache
        m_cache.remove( uri );

        // update the kickofflists
        updateKickOffLists( uri, Variant() );
    }
}


void Nepomuk2::ResourceData::remove( bool recursive )
{
    Q_UNUSED(recursive)
    QMutexLocker lock(&m_modificationMutex);

    if( !m_uri.isEmpty() ) {
        KJob* job = Nepomuk2::removeResources(QList<QUrl>() << m_uri);
        if( !job->exec() ) {
            //TODO: Set the error somehow
            kWarning() << job->errorString();
            return;
        }
    }

    resetAll();
}


bool Nepomuk2::ResourceData::exists()
{
    if( m_uri.isValid() ) {
        const QString query = QString::fromLatin1("ask { %1 ?p ?o . }")
                              .arg( Soprano::Node::resourceToN3(m_uri) );
        return MAINMODEL->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
    }
    else {
        return false;
    }
}


bool Nepomuk2::ResourceData::isValid() const
{
    return !m_uri.isEmpty() || !m_kickoffUris.isEmpty();
}


Nepomuk2::ResourceData* Nepomuk2::ResourceData::determineUri()
{
    // We have the following possible situations:
    // 1. m_uri is already valid
    //    -> simple, nothing to do
    //
    // 2. m_uri is not valid
    //    -> we need to determine the URI
    //
    // 2.1. m_kickoffUri is valid
    // 2.1.1. it is a file URL
    // 2.1.1.1. it is nie:url for r
    //          -> use r as m_uri
    // 2.1.1.2. it points to a file on a removable device for which we have a filex:/ URL
    //          -> use the r in r nie:url filex:/...
    // 2.1.1.3. it is a file which is not an object in some nie:url relation
    //          -> create new random m_uri and use kickoffUriOrId() as m_nieUrl
    // 2.1.2. it is a resource URI
    //          -> use it as m_uri
    //
    // 2.2. m_kickOffUri is not valid
    // 2.2.1. m_kickOffUri is a nao:identifier for r
    //        -> use r as m_uri
    //

    if( m_uri.isEmpty() ) {
        Soprano::Model* model = MAINMODEL;

        if( !m_kickoffUris.isEmpty() ) {
            KUrl kickOffUri = *m_kickoffUris.begin();
            if( kickOffUri.scheme().isEmpty() ) {
                //
                // Not valid. Checking for nao:identifier
                //
                QString query = QString::fromLatin1("select distinct ?r where { ?r %1 %2. } LIMIT 1")
                                .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::identifier()) )
                                .arg( Soprano::Node::literalToN3( kickOffUri.url() ) );

                Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
                if( it.next() ) {
                    m_uri = it["r"].uri();
                    it.close();
                }
            }
            else {
                //
                // In one query determine if the URI is already used as resource URI or as
                // nie:url
                //
                QString query = QString::fromLatin1("select distinct ?r ?o where { "
                                                    "{ ?r %1 %2 . FILTER(?r!=%2) . } "
                                                    "UNION "
                                                    "{ %2 ?p ?o . } "
                                                    "} LIMIT 1")
                                .arg( Soprano::Node::resourceToN3( Nepomuk2::Vocabulary::NIE::url() ) )
                                .arg( Soprano::Node::resourceToN3( kickOffUri ) );
                Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
                if( it.next() ) {
                    QUrl uri = it["r"].uri();
                    if( uri.isEmpty() ) {
                        m_uri = kickOffUri;
                    }
                    else {
                        m_uri = uri;
                        m_nieUrl = kickOffUri;
                    }
                    it.close();
                }
                else if( kickOffUri.scheme() == QLatin1String("nepomuk") ) {
                    // for nepomuk URIs we simply use the kickoff URI as resource URI
                    m_uri = kickOffUri;
                }
                else {
                    // for everything else we use m_kickoffUri as nie:url with a new random m_uri
                    m_nieUrl = kickOffUri;
                }
            }
        }

        //
        // Move us to the final data hash now that the URI is known
        //
        if( !m_uri.isEmpty() ) {
            m_cacheDirty = true;
            ResourceDataHash::iterator it = m_rm->m_initializedData.find(m_uri);
            if( it == m_rm->m_initializedData.end() ) {
                m_rm->m_initializedData.insert( m_uri, this );
            }
            else {
                return it.value();
            }
        }
    }

    return this;
}


void Nepomuk2::ResourceData::invalidateCache()
{
    m_cacheDirty = true;
}


bool Nepomuk2::ResourceData::operator==( const ResourceData& other ) const
{
    if( this == &other )
        return true;

    return( m_uri == other.m_uri &&
            m_kickoffUris == other.m_kickoffUris );
}


QDebug Nepomuk2::ResourceData::operator<<( QDebug dbg ) const
{
    KUrl::List list = m_kickoffUris.toList();
    dbg << QString::fromLatin1("[kickoffuri: %1; uri: %2; ref: %4]")
        .arg( list.toStringList().join(QLatin1String(",")),
              m_uri.url() )
        .arg( m_ref );

    return dbg;
}


QDebug operator<<( QDebug dbg, const Nepomuk2::ResourceData& data )
{
    return data.operator<<( dbg );
}


void Nepomuk2::ResourceData::updateKickOffLists(const QUrl& prop, const Nepomuk2::Variant& v)
{
    KUrl oldUrl;
    KUrl newUrl;
    if( prop == Nepomuk2::Vocabulary::NIE::url() ) {
        oldUrl = m_nieUrl;
        newUrl = v.toUrl();
        m_nieUrl = newUrl;
    }
    else if( prop == Soprano::Vocabulary::NAO::identifier() ) {
        Q_FOREACH( const KUrl& url, m_kickoffUris ) {
            if( url.scheme().isEmpty() ) {
                oldUrl = url;
                break;
            }
        }
        newUrl = KUrl( v.toString() );
    }
    else {
        return;
    }

    if( oldUrl != newUrl ) {
        QMutexLocker rmlock(&m_rm->mutex);

        m_kickoffUris.remove( oldUrl );
        m_rm->m_uriKickoffData.remove( oldUrl );

        if( !newUrl.isEmpty() ) {
            m_kickoffUris.insert( newUrl );
            m_rm->m_uriKickoffData.insert( newUrl, this );
        }
    }
}
