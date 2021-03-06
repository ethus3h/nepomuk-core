/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef STORERESOURCEJOB_H
#define STORERESOURCEJOB_H

#include <KJob>
#include "nepomuk_export.h"
#include "datamanagement.h"

class QDBusPendingCallWatcher;

namespace Nepomuk2 {

    class NEPOMUK_EXPORT StoreResourcesJob : public KJob
    {
        Q_OBJECT
    public:
        ~StoreResourcesJob();

        QHash<QUrl, QUrl> mappings() const;

    private:
        StoreResourcesJob(const Nepomuk2::SimpleResourceGraph& resources,
                         Nepomuk2::StoreIdentificationMode identificationMode,
                         Nepomuk2::StoreResourcesFlags flags,
                         const QHash<QUrl, QVariant>& additionalMetadata,
                         const KComponentData& component);
        class Private;
        Private *d;

    // We do not want to change the order of the functions. This has been done
    // in order to preserve binary compatibility
    public:
        void start();
    private:

        Q_PRIVATE_SLOT( d, void _k_slotDBusCallFinished( QDBusPendingCallWatcher* ) )

        friend Nepomuk2::StoreResourcesJob* storeResources(const SimpleResourceGraph&,
                                                         StoreIdentificationMode,
                                                         StoreResourcesFlags,
                                                         const QHash<QUrl, QVariant>&,
                                                         const KComponentData& );
    };

}
#endif // STORERESOURCEJOB_H
