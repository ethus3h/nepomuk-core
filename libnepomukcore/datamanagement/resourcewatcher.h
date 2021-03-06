/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>
    Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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


#ifndef RESOURCEWATCHER_H
#define RESOURCEWATCHER_H

#include "class.h"
#include "property.h"
#include "resource.h"

#include <QtDBus/QDBusVariant>
#include <QDBusPendingCallWatcher>
#include <QtCore/QVariant>

#include "nepomuk_export.h"

namespace Nepomuk2 {

    /**
     * \class ResourceWatcher resourcewatcher.h
     *
     * \brief Selectively monitor the nepomuk repository for changes.
     *
     * Resources may be monitored on the basis of types, properties, and uris.
     *
     * Changes may be monitored in one of the following ways:
     * -# By resources -
     *    Specify the exact resources that should be watched. Any changes made to the specified resources
     *    (Excluding \ref nepomuk_dms_metadata) will be notified through the propertyAdded() and propertyRemoved()
     *    signals. Notifications will also be sent if any of the watched resources is deleted.
     * -# By resources and properties -
     *    Specify the exact resources and their properties. Any changes made to the specified resources
     *    which touch one of the specified properties will be notified through the propertyAdded() and propertyRemoved()
     *    signals.
     * -# By types -
     *    Specific types may be specified via add/setType. If types are set, then notifications will be
     *    sent for all new resources of that type. This includes property changes and resource creation and removal.
     *    TODO: add flags that allow to only watch for resource creation and removal.
     * -# By types and properties -
     *    Both the types and properties may be specified. Notifications will be sent for property changes
     *    in resource with the specified types.
     *
     * \section nepomuk_rw_examples Resource Watcher Usage Example
     *
     * The following code creates a new ResourceWatcher, configures it to listen to changes on the \c nmm:performer
     * property on one specific resource \c res.
     *
     * \code
     * Nepomuk2::ResourceWatcher* watcher = new Nepomuk2::ResourceWatcher(this);
     * watcher->addResource(res);
     * watcher->addProperty(NMM::performer());
     * connect(watcher, SIGNAL(propertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
     *         this, SLOT(slotPropertyChanged()));
     * connect(watcher, SIGNAL(propertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
     *         this, SLOT(slotPropertyChanged()));
     * rwatcher->start();
     * \endcode
     *
     * \author Vishesh Handa <handa.vish@gmail.com>, Sebastian Trueg <trueg@kde.org>
     *
     * \ingroup nepomuk_datamanagement
     */
    class NEPOMUK_EXPORT ResourceWatcher : public QObject
    {
        Q_OBJECT

    public:
        /**
         * \brief Create a new %ResourceWatcher instance.
         *
         * This instance will not emit any signals before it has been configured
         * and started.
         */
        ResourceWatcher( QObject* parent = 0 );

        /**
         * \brief Destructor.
         */
        virtual ~ResourceWatcher();

    public Q_SLOTS:
        /**
         * \brief Add a type to be watched.
         *
         * Every resource of this type will be watched for changes.
         *
         * \sa setTypes()
         */
        void addType( const Types::Class & type );

        /**
         * \brief Add a resource to be watched.
         *
         * Every change to this resource will be
         * signalled, depending on the configured properties().
         *
         * \sa setResources()
         */
        void addResource( const Nepomuk2::Resource & res );

        /**
         * \brief Add a resource to be watched.
         *
         * Every change to this resource will be
         * signalled, depending on the configured properties().
         *
         * \sa setResources()
         */
        void addResource( const QUrl& resUri );

        /**
         * \brief Add a property to be watched.
         *
         * Every change to a value of this property
         * will be signalled, depending on the configured resources() or types().
         *
         * \sa setProperties()
         */
        void addProperty( const Types::Property & property );

        /**
         * \brief Remove a type to be watched.
         *
         * Every resource of this type will be watched for changes.
         *
         * \sa setTypes()
         */
        void removeType( const Types::Class & type );

        /**
         * \brief Remove a resource to be watched.
         *
         * Every change to this resource will be
         * signalled, depending on the configured properties().
         *
         * \sa setResources()
         */
        void removeResource( const Nepomuk2::Resource & res );

        /**
         * \brief Remove a resource to be watched.
         *
         * Every change to this resource will be
         * signalled, depending on the configured properties().
         *
         * \sa setResources()
         */
        void removeResource( const QUrl& resUri );

        /**
         * \brief Remove a property to be watched.
         *
         * Every change to a value of this property
         * will be signalled, depending on the configured resources() or types().
         *
         * \sa setProperties()
         */
        void removeProperty( const Types::Property & property );

        /**
         * \brief Set the types to be watched.
         *
         * Every resource having one of these types will be watched for changes.
         *
         * \sa addType()
         */
        void setTypes( const QList<Types::Class> & types_ );

        /**
         * \brief Set the resources to be watched.
         *
         * Every change to one of these resources will be
         * signalled, depending on the configured properties().
         *
         * \sa addResource()
         */
        void setResources( const QList<Nepomuk2::Resource> & resources_ );

        /**
         * \brief Set the properties to be watched.
         *
         * Every change to a value of any of these properties
         * will be signalled, depending on the configured resources() or types().
         *
         * \sa addProperty()
         */
        void setProperties( const QList<Types::Property> & properties_ );

        /**
         * \brief The types that have been configured via addType() and setTypes().
         *
         * Every resource having one of these types will be watched
         * for changes.
         */
        QList<Types::Class> types() const;

        /**
         * \brief Return the number of types that are being watched
         *
         * This method is slightly faster than called types().count()
         *
         * \sa types addType setType
         */
        int typeCount() const;

        /**
         * \brief The resources that have been configured via addResource() and setResources().
         *
         * Every change to one of these resources will be
         * signalled, depending on the configured properties().
         */
        QList<Nepomuk2::Resource> resources() const;

        /**
         * \brief Return the number of resources that are being watched
         *
         * This method is slightly faster than called resources().count()
         *
         * \sa resources addResource setResources
         */
        int resourceCount() const;

        /**
         * \brief The properties that have been configured via addProperty() and setProperties().
         *
         * Every change to a value of any of these properties
         * will be signalled, depending on the configured resources() or types().
         */
        QList<Types::Property> properties() const;

        /**
         * \brief Return the number of properties that are being watched
         *
         * This method is slightly faster than called properties().count()
         *
         * \sa properties addProperty setProperty
         */
        int propertyCount() const;

        /**
         * \brief Start the signalling of changes.
         *
         * Before calling this method no signal will be emitted. In
         * combination with stop() this allows to suspend the watching.
         * Calling start() multiple times has no effect.
         */
        bool start();

        /**
         * \brief Stop the signalling of changes.
         *
         * Allows to stop the watcher which has been started
         * via start(). Calling stop() multiple times has no effect.
         */
        void stop();

    Q_SIGNALS:
        /**
         * \brief This signal is emitted when a new resource is created.
         * \param resource The newly created resource.
         * \param types The types the new resource has. If types() have been configured this list will always
         * contain one of the configured types.
         */
        void resourceCreated( const Nepomuk2::Resource & resource, const QList<QUrl>& types ); //FIXME: Use either Resource or uri, not a mix

        /**
         * \brief This signal is emitted when a resource is deleted.
         * \param uri The resource URI of the removed resource.
         * \param types The types the removed resource had. If types() have been configured this list will always
         * contain one of the configured types.
         */
        void resourceRemoved( const QUrl & uri, const QList<QUrl>& types );

        /**
         * \brief This signal is emitted when a type has been added to a resource. This does not include creation which
         * is signalled via resourceCreated(). It only applies to changes in a resource's types.
         * \param res The changed resource.
         * \param type The newly added type. If types() have been configured it will be one of them.
         */
        void resourceTypeAdded( const Nepomuk2::Resource & res, const Nepomuk2::Types::Class & type );

        /**
         * \brief This signal is emitted when a type has been removed from a resource.
         *
         * This does not include removal of entire resources which is signalled via resourceRemoved().
         * It only applies to changes in a resource's types.
         * \param res The changed resource.
         * \param type The removed type. If types() have been configured it will be one of them.
         */
        void resourceTypeRemoved( const Nepomuk2::Resource & res, const Nepomuk2::Types::Class & type );

        /**
         * \brief This signal is emitted when a property value is added.
         * \param resource The changed resource.
         * \param property The property which has a new value.
         * \param value The newly added property value.
         */
        void propertyAdded( const Nepomuk2::Resource & resource,
                            const Nepomuk2::Types::Property & property,
                            const QVariant & value );

        /**
         * \brief This signal is emitted when a property value is removed.
         * \param resource The changed resource.
         * \param property The property which was changed.
         * \param value The removed property value.
         */
        void propertyRemoved( const Nepomuk2::Resource & resource,
                              const Nepomuk2::Types::Property & property,
                              const QVariant & value );

        /**
         * \brief This signal is emitted when a property value is changed.
         *
         * This signal is essentially a combination of the propertyAdded and propertyRemoved signals.
         *
         * Be aware that removing and then adding a property will result in two separate
         * propertyChanged signals. They are never combined.
         *
         * Specially, since one could theoretically take forever between the removal and the
         * setting of the property.
         *
         * \param resource The changed resource.
         * \param property The property which was changed.
         * \param addedValues The values that have been added.
         * \param removedValues The values that have been removed.
         */
        void propertyChanged( const Nepomuk2::Resource & resource,
                              const Nepomuk2::Types::Property & property,
                              const QVariantList & addedValues,
                              const QVariantList & removedValues );

    private Q_SLOTS:
        void slotWatchFinished(QDBusPendingCallWatcher* replyWatcher);
        void slotResourceCreated(const QString& res, const QStringList& types);
        void slotResourceRemoved(const QString& res, const QStringList& types);
        void slotResourceTypesAdded(const QString& res, const QStringList& types);
        void slotResourceTypesRemoved(const QString& res, const QStringList& types);
        void slotPropertyChanged(const QString& res, const QString& prop_, const QVariantList& addedObjs, const QVariantList& removedObjs);
    private:
        class Private;
        Private * d;
    };
}

#endif // RESOURCEWATCHER_H
