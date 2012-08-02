/*
   Copyright (c) 2010 Sebastian Trueg <trueg@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _NEPOMUK_COUNT_QUERY_RUNNABLE_H_
#define _NEPOMUK_COUNT_QUERY_RUNNABLE_H_

#include <QtCore/QRunnable>
#include <QtCore/QPointer>
#include <QtCore/QMutex>

#include "folder.h"

namespace Nepomuk2 {
    namespace Query {
        class Folder;

        class CountQueryRunnable : public QRunnable
        {
        public:
            CountQueryRunnable( Soprano::Model* model, Folder* folder );
            ~CountQueryRunnable();

            /**
             * Cancel the search and detach it from the folder.
             */
            void cancel();

            void run();

        private:
            Soprano::Model* m_model;
            QPointer<Folder> m_folder;
            QMutex m_folderMutex;
        };
    }
}

#endif
