/* This file is part of the Nepomuk-KDE libraries
   Copyright (c) 2007 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _NEPOMUK_ENTITY_H_
#define _NEPOMUK_ENTITY_H_

#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtCore/QSharedData>

#include <Soprano/Node>

#include <kglobal.h>
#include <klocale.h>

#include "nepomuk_export.h"

namespace Nepomuk {
    namespace Types {
        class EntityPrivate;

        /**
         * Base class for static ontology entities Class and Property.
         * It encapsulates the generic labels and comments that both
         * types have.
         */
        class NEPOMUK_EXPORT Entity
        {
        public:
            /**
             * Default copy constructor.
             */
            Entity( const Entity& );

            /**
             * Destructor.
             */
            virtual ~Entity();

            /**
             * Copy operator.
             */
            Entity& operator=( const Entity& );

            /**
             * The name of the resource. The name equals the fragment of the
             * URI.
             */
            QString name() const;

            /**
             * The URI of the resource
             */
            QUrl uri() const;

            /**
             * Retrieve the label of the entity (rdfs:label)
             *
             * \param language The code of the language to use. Defaults to the session
             *                 language configured in KDE.
             *
             * \return The label translated into \p language or the default fallback label
             * if no translation is available or an empty string if no label could be found
             * at all.
             */
            QString label( const QString& language = KGlobal::locale()->language() );

            /**
             * Retrieve the comment of the entity (rdfs:comment)
             *
             * \param language The code of the language to use. Defaults to the session
             *                 language configured in KDE.
             *
             * \return The comment translated into \p language or the default fallback comment
             * if no translation is available or an empty string if no comment could be found
             * at all.
             */
            QString comment( const QString& language = KGlobal::locale()->language() );

            /**
             * Is this a valid Entity, i.e. has it a valid URI.
             * A valid Entity does not necessarily have a label and a comment, it
             * does not even have to exist in the Nepomuk store.
             *
             * \sa isAvailable
             */
            bool isValid() const;

            /**
             * Is this Entity available locally, i.e. could its properties
             * be loaded from the Nepomuk store.
             */
            bool isAvailable();

            /**
             * An Entity can be used as a QUrl automagically.
             */
            operator QUrl() const { return uri(); }

            /**
             * Compares two Entity instances by simply comparing their URI.
             */
            bool operator==( const Entity& other );

            /**
             * Compares two Entity instances by simply comparing their URI.
             */
            bool operator!=( const Entity& other );

        protected:
            /**
             * Create an invalid Entity instance.
             */
            Entity();

            QExplicitlySharedDataPointer<EntityPrivate> d;
        };
    }
}


namespace Nepomuk {

    class Ontology;

    /**
     * \deprecated in favor of Nepomuk::Types::Entity
     */
    class KDE_DEPRECATED NEPOMUK_EXPORT Entity
    {
    public:
        Entity( const Entity& );
        ~Entity();

        Entity& operator=( const Entity& );

        /**
         * The ontology in which the resource is defined.
         */
        const Ontology* definingOntology() const;

        /**
         * The name of the resource. The name equals the fragment of the
         * URI.
         */
        QString name() const;

        /**
         * The URI of the resource
         */
        QUrl uri() const;
	    
        QString label( const QString& language = QString() ) const;
        QString comment( const QString& language = QString() ) const;

    protected:
        Entity();

    private:
        class Private;
        QSharedDataPointer<Private> d;

        friend class OntologyManager;
    };
}

#endif
