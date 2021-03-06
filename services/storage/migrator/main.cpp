/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2013  Vishesh Handa <handa.vish@gmail.com>

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

#include "wizard.h"
#include "systray.h"

#include <KUniqueApplication>
#include <KAboutData>
#include <KLocale>
#include <KCmdLineArgs>
#include <KDebug>

#include <QtDBus/QDBusConnection>

#include <QDBusConnectionInterface>

int main( int argc, char** argv )
{
    KAboutData aboutData("nepomukmigrator",
                         "nepomukmigrator",
                         ki18n("NepomukMigrator"),
                         "1.0",
                         ki18n("Nepomuk Data Migration Tool"),
                         KAboutData::License_GPL,
                         ki18n("(c) 2013, Nepomuk-KDE Team"),
                         KLocalizedString(),
                         "http://nepomuk.kde.org");
    aboutData.addAuthor(ki18n("Vishesh Handa"), ki18n("Maintainer"), "handa.vish@gmail.com");

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    options.add("advanced", ki18n("Opens the advanced menu for migration"));

    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();

    KUniqueApplication app;
    const KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    bool nepomukRunning = QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.NepomukStorage");

    if( args->isSet("advanced") ) {
        Nepomuk2::MigrationWizard* wizard = new Nepomuk2::MigrationWizard();

        if( !nepomukRunning ) {
            wizard->showError(i18nc("@info", "Nepomuk does not seem to be running. <br/>Cannot migrate the data without it") );
        }
        wizard->show();
    }
    else {
        Nepomuk2::SysTray* systray = new Nepomuk2::SysTray();
    }

    return app.exec();
}
