/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

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


#ifndef BACKUPSYNCSERVICE_H
#define BACKUPSYNCSERVICE_H

#include <QtCore/QObject>
#include <Soprano/Model>

namespace Nepomuk2 {

    class DiffGenerator;

    class SyncManager;
    class BackupManager;
    class OntologyLoader;

    class BackupSyncService : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.BackupSync")
    public:
        BackupSyncService( OntologyLoader* loader, Soprano::Model *model, QObject *parent );
        ~BackupSyncService();

    public Q_SLOTS:
        Q_SCRIPTABLE void test();

    private:
        DiffGenerator * m_diffGenerator;

        SyncManager * m_syncManager;
        BackupManager * m_backupManager;
	};

}

#endif // BACKUPSYNCSERVICE_H
