/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _NEPOMUK_RESOURCE_H_
#define _NEPOMUK_RESOURCE_H_

#include <QtCore>

#include "nepomuk_export.h"


namespace Nepomuk {

    class ResourceData;
    class Variant;

    // do not remove. Will be replaced with class declarations by the Nepomuk class generator.
        class Tag;
        class Resource;


    enum ErrorCode {
        NoError = 0,
        CommunicationError, /**< A commication error, i.e. connection failure */
        InvalidType,
        UnknownError
    };

    /**
     * \return A human-readble string.
     */
    // FIXME: add the uri of the resource as parameter
    NEPOMUK_EXPORT QString errorString( ErrorCode code );

    /**
     * \brief Resource is the central object type in Nepomuk. It represents a piece of
     * information of any kind.
     *
     * Resources are identified by their unique URI (which
     * correlates directly with the URI in the local NEPOMUK RDF storage.
     *
     * Resource objects with the same URI share their data.
     *
     * All methods in Resource are thread-safe.
     *
     * See \ref hacking for details on how to use Resource.
     *
     * \see ResourceManager
     */
    class NEPOMUK_EXPORT Resource
    {
    public:
        /**
         * Creates an empty invalid Resource
         */
        Resource();

        Resource( const Resource& );

        /**
         * Creates a new Resource object.
         *
         * The actual resource data is loaded on demand. Thus, it is possible to work
         * with Resources as if they were in memory all the time.
         *
         * \param uriOrIdentifier The unique URI or an arbitrary identifier of the resource.
         *                        If it exists as a resource URI in the RDF store it is used to load the
         *                        related properties. If not the passed string is treated
         *                        as an identifier.
         *                        If a resource exists in the store which has this identifier
         *                        set this resource's properties are loaded. Otherwise the
         *                        resource is created in the store
         *                        with a new random URI which can be accessed through \a uri
         *                        after the resource has been synced. The resource can later
         *                        again be found through the same identifier.
         *                        In Nepomuk there are two ways of identifying a resource
         *                        uniquely:
         *                        \li The URI of the resource which is generated randomly
         *                        by the framework and can only be accessed in a read-only
         *                        fashion.
         *                        \li An identifier in combination with the resource type (Be
         *                        aware that this identification can only be guranteed if identifiers
         *                        are never set manually via addIdentifier but only through the
         *                        constructor.
         *
         * \param type The URI identifying the type of the resource. If it is empty
         *             Resource falls back to http://www.w3.org/2000/01/rdf-schema\#Resource or
         *             in case the resource already exists the type will be read from the
         *             store.
         *
         * Example:
         *
         * The best way to understand the URI and identifier system is through file resources.
         * When a Resource object is created with the local path of the file as an identifier:
         *
         * \code
         * Resource myfile( "/tmp/testfile.txt" );
         * \endcode
         *
         * Now the URI of the resource in the store representing metadata for the file /tmp/testfile.txt
         * is referred to by myfile.uri() which differs from the path of the file. However, the path of
         * the file is saved as a \a hasIdentifier relation which means that it can be used to easily find
         * the related resource.
         */
        Resource( const QString& uriOrIdentifier, const QString& type = QString() );

        /**
         * Constructor used internally.
         */
        Resource( ResourceData* );

        virtual ~Resource();

        Resource& operator=( const Resource& );

        /**
         * The URI of the resource, uniquely identifying it. This URI in most
         * cases is a virtual one which has been created from a generic base
         * namespace and some identifier.
         *
         * the most important thing to remember is that the URI of for example
         * a file has no relation to its local path.
         *
         * \sa getIdentifiers
         */
        QString uri() const;

        /**
         * The type URI.
         * \sa name()
         */
        QString type() const;

        /**
         * The name of the class this Resource represents an object of.
         * The classname is derived from the type URI (see Resource::uri).
         * For a translated user readable name of the resource see
         * Ontology::typeName.
         *
         * \sa type()
         */
        QString className() const;

        /**
         * \return A list of all defined properties
         */
        QHash<QString, Variant> allProperties() const;

        /**
         * Check if property identified by \a uri is defined
         * for this resource.
         *
         * \param uri The URI identifying the property.
         *
         * \return true if property \a uri has a value set.
         */
        bool hasProperty( const QString& uri ) const;

        /**
         * Retrieve the value of property \a uri. If the property is not defined for
         * this resource an invalid, empty Variant object is returned.
         *
         * \param uri The URI identifying the property.
         */
        Variant property( const QString& uri ) const;

        /**
         * Set a property of the resource.
         *
         * \param uri The URI identifying the property.
         * \param value The value of the property (i.e. the object of the RDF triple(s))
         */
        void setProperty( const QString& uri, const Variant& value );

        /**
         * Remove property \a uri from this resource object.
         *
         * \param uri The URI identifying the property.
         */
        void removeProperty( const QString& uri );

        /**
         * Remove this resource completely.
         * CAUTION: After calling this method the resource will have been removed from the store
         * without any trace.
         */
        void remove();

        /**
         * \return true if this resource (i.e. the uri of this resource) exists in the local
         * NEPOMUK RDF store.
         */
        bool exists() const;

        /**
         * \return true if this Resource object is valid, i.e. has a proper URI and type and
         * can be synced with the local NEPOMUK RDF store.
         */
        bool isValid() const;

        /**
         * Operator to compare two Resource objects. Normally one does not need this. It is
         * mainly intended for testing and debugging purposes.
         */
        bool operator==( const Resource& ) const;

        // do not remove. Will be replaced with method declarations by the Nepomuk class generator.
            /**
             * Get property 'label'. 
             */
            QString label() const;

            /**
             * Set property 'label'. 
             */
            void setLabel( const QString& value );

            /**
             * \return The URI of the property 'label'. 
             */
            static QString labelUri();

            /**
             * Get property 'Symbol'. Each resource can have a symbol assigned. 
             * For now this is a simple string which can either be the patch to 
             * an actual pixmap file or just the name of an icon as defined by 
             * the freedesktop.org standard. 
             */
            QStringList symbols() const;

            /**
             * Set property 'Symbol'. Each resource can have a symbol assigned. 
             * For now this is a simple string which can either be the patch to 
             * an actual pixmap file or just the name of an icon as defined by 
             * the freedesktop.org standard. 
             */
            void setSymbols( const QStringList& value );

            /**
             * Add a value to property 'Symbol'. Each resource can have a symbol 
             * assigned. For now this is a simple string which can either be 
             * the patch to an actual pixmap file or just the name of an icon as 
             * defined by the freedesktop.org standard. 
             */
            void addSymbol( const QString& value );

            /**
             * \return The URI of the property 'Symbol'. 
             */
            static QString symbolUri();

            /**
             * Get property 'Identifier'. 
             */
            QStringList identifiers() const;

            /**
             * Set property 'Identifier'. 
             */
            void setIdentifiers( const QStringList& value );

            /**
             * Add a value to property 'Identifier'. 
             */
            void addIdentifier( const QString& value );

            /**
             * \return The URI of the property 'Identifier'. 
             */
            static QString identifierUri();

            /**
             * Get property 'isRelated'. 
             */
            QList<Resource> isRelateds() const;

            /**
             * Set property 'isRelated'. 
             */
            void setIsRelateds( const QList<Resource>& value );

            /**
             * Add a value to property 'isRelated'. 
             */
            void addIsRelated( const Resource& value );

            /**
             * \return The URI of the property 'isRelated'. 
             */
            static QString isRelatedUri();

            /**
             * Get property 'altLabel'. 
             */
            QStringList altLabels() const;

            /**
             * Set property 'altLabel'. 
             */
            void setAltLabels( const QStringList& value );

            /**
             * Add a value to property 'altLabel'. 
             */
            void addAltLabel( const QString& value );

            /**
             * \return The URI of the property 'altLabel'. 
             */
            static QString altLabelUri();

            /**
             * Get property 'Rating'. 
             */
            quint32 rating() const;

            /**
             * Set property 'Rating'. 
             */
            void setRating( const quint32& value );

            /**
             * \return The URI of the property 'Rating'. 
             */
            static QString ratingUri();

            /**
             * Get property 'description'. Everything can be annotated with 
             * a simple string comment. 
             */
            QString description() const;

            /**
             * Set property 'description'. Everything can be annotated with 
             * a simple string comment. 
             */
            void setDescription( const QString& value );

            /**
             * \return The URI of the property 'description'. 
             */
            static QString descriptionUri();

            /**
             * Get property 'isTopicOf'. 
             */
            QList<Resource> isTopicOfs() const;

            /**
             * Set property 'isTopicOf'. 
             */
            void setIsTopicOfs( const QList<Resource>& value );

            /**
             * Add a value to property 'isTopicOf'. 
             */
            void addIsTopicOf( const Resource& value );

            /**
             * \return The URI of the property 'isTopicOf'. 
             */
            static QString isTopicOfUri();

            /**
             * Get property 'annotation'. 
             */
            QList<Resource> annotations() const;

            /**
             * Set property 'annotation'. 
             */
            void setAnnotations( const QList<Resource>& value );

            /**
             * Add a value to property 'annotation'. 
             */
            void addAnnotation( const Resource& value );

            /**
             * \return The URI of the property 'annotation'. 
             */
            static QString annotationUri();

            /**
             * Get property 'Tag'. Each Resource can be tagged with an arbitrary 
             * number of Tags. This allows a simple grouping of resources. 
             */
            QList<Tag> tags() const;

            /**
             * Set property 'Tag'. Each Resource can be tagged with an arbitrary 
             * number of Tags. This allows a simple grouping of resources. 
             */
            void setTags( const QList<Tag>& value );

            /**
             * Add a value to property 'Tag'. Each Resource can be tagged with 
             * an arbitrary number of Tags. This allows a simple grouping of 
             * resources. 
             */
            void addTag( const Tag& value );

            /**
             * \return The URI of the property 'Tag'. 
             */
            static QString tagUri();

            /**
             * Get property 'Topic'. 
             */
            QList<Resource> topics() const;

            /**
             * Set property 'Topic'. 
             */
            void setTopics( const QList<Resource>& value );

            /**
             * Add a value to property 'Topic'. 
             */
            void addTopic( const Resource& value );

            /**
             * \return The URI of the property 'Topic'. 
             */
            static QString topicUri();

            /**
             * Get all resources that have this resource set as property 'annotation'. 
             * \sa ResourceManager::allResourcesWithProperty 
             */
            QList<Resource> annotationOf() const;

            /**
             * Get all resources that have this resource set as property 'isRelated'. 
             * \sa ResourceManager::allResourcesWithProperty 
             */
            QList<Resource> isRelatedOf() const;

            /**
             * Retrieve a list of all available Resource resources. This list 
             * consists of all resource of type Resource that are stored in 
             * the local Nepomuk meta data storage and any changes made locally. 
             * Be aware that in some cases this list can get very big. Then it 
             * might be better to use libKNep directly. 
             */
            static QList<Resource> allResources();


    private:
        ResourceData* m_data;

        class Private;
        Private* d; // unused

        friend class ResourceData;
    };
}

#endif
