/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

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


#ifndef NEPOMUK_DATAMANAGEMENT_RESOURCEMERGER_H
#define NEPOMUK_DATAMANAGEMENT_RESOURCEMERGER_H

#include <QtCore/QHash>
#include <QtCore/QVariant>
#include <QtCore/QUrl>
#include <QtCore/QSet>

#include <KUrl>
#include <Soprano/Error/ErrorCache>

#include "datamanagement.h"
#include "resourceidentifier.h"
#include "syncresource.h"

namespace Soprano {
    class Node;
    class Statement;
    class Graph;
}

namespace Nepomuk2 {
    class DataManagementModel;
    class ResourceWatcherManager;

    class ResourceMerger : public Soprano::Error::ErrorCache
    {
    public:
        ResourceMerger( Nepomuk2::DataManagementModel * model, const QString & app,
                        const StoreResourcesFlags& flags, bool discardable );
        ~ResourceMerger();

        void setMappings( const QHash<QUrl, QUrl> & mappings );
        QHash<QUrl, QUrl> mappings() const;

        bool merge(const Sync::ResourceHash& resHash);

    private:
        /**
         * Checks the cardinality + domain/range of the properties in the \p res.
         * Returns true if the data is valid, sets an error otherwise.
         *
         * It does not modify \p res, but temporarily alters it. The pass by reference is to
         * avoid making a copy
         */
        bool hasValidData( const QHash<KUrl, Sync::SyncResource>& resHash, Sync::SyncResource& res );

        bool push( const QUrl& graph, const Nepomuk2::Sync::ResourceHash& resHash );

        //
        // Resolution
        //
        Soprano::Node resolveBlankNode( const Soprano::Node& node );
        Soprano::Node resolveMappedNode( const Soprano::Node& node );

        Sync::ResourceHash resolveBlankNodes( const Sync::ResourceHash& resHash );

        /// The new uris which are being created from blank nodes
        QSet<QUrl> m_newUris;

        QHash<QUrl, QUrl> m_mappings;

        /// a list of all the statements that have been removed (only used for the resource watcher)
        Sync::ResourceHash m_resRemoveHash;

        /**
         * Each statement that is being merged and already exists, belongs to a graph. This hash
         * maps that oldGraph -> newGraph.
         * The newGraph is generated by mergeGraphs, and contains the metdata from the oldGraph
         * and the additionalMetadata provided.
         *
         * \sa mergeGraphs
         */
        QHash<QUrl, QUrl> m_graphHash;

        bool m_discardbale;

        QString m_app;
        QUrl m_appUri;
        QUrl m_graph;

        StoreResourcesFlags m_flags;
        Nepomuk2::DataManagementModel * m_model;

        /**
         * Checks if \p node is of rdf:type \p type.
         *
         * \param newTypes contains additional types that should be considered as belonging to \p node
         */
        bool isOfType( const Soprano::Node& node, const QUrl& type, const QList<QUrl>& newTypes = QList<QUrl>() ) const;

        QSet<QUrl> metadataProperties;
        ResourceWatcherManager *m_rvm;
    };

}

#endif // NEPOMUK_DATAMANAGEMENT_RESOURCEMERGER_H
