/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2007-2011 Sebastian Trueg <trueg@kde.org>
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

#ifndef _NEPOMUK2_TAG_H_
#define _NEPOMUK2_TAG_H_

#include "resource.h"
#include "nepomuk_export.h"

namespace Nepomuk2 {

    /**
     * \class Tag tag.h Nepomuk2/Tag
     *
     * A Tag can be assigned to any Thing. This allows simple grouping
     * of resources. Each Tag is identifed by its label which should
     * be unique.
     *
     * \author Sebastian Trueg <trueg@kde.org>
     */
    class NEPOMUK_EXPORT Tag : public Resource
    {
    public:
        /**
         * Create a new empty and invalid Tag instance
         */
        Tag();

        /**
         * Default copy constructor
         */
        Tag( const Tag& );

        /**
         * \overload
         */
        Tag( const Resource& );

        /**
         * Create a new Tag instance representing the resource
         * referenced by \a uriOrIdentifier.
         */
        Tag( const QString& uriOrIdentifier );

        /**
         * Create a new Tag instance representing the resource
         * referenced by \a uri.
         */
        Tag( const QUrl& uri );

        /**
         * Destructor
         */
        ~Tag();

        Tag& operator=( const Tag& );

        /**
         * Get all resources that have this resource set as property 'Tag'.
         * Each Resource can be tagged with an arbitrary number of Tags.
         * This allows a simple grouping of resources. \sa ResourceManager::allResourcesWithProperty
         */
        QList<Resource> tagOf() const;

        /**
         * Retrieve a list of all available Tag resources. This list consists
         * of all resource of type Tag that are stored in the local Nepomuk
         * meta data storage and any changes made locally. Be aware that
         * in some cases this list can get very big.
         *
         * In those cases it might be better to use the asyncronous approach
         * via Query::QueryServiceClient and a Query::ResourceTypeTerm with
         * type Soprano::Vocabulary::NAO::Tag().
         */
        static QList<Tag> allTags();

    protected:
        Tag( const QString& uri, const QUrl& type );
        Tag( const QUrl& uri, const QUrl& type );
   };
}

#endif
