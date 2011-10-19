/* This file is part of the KDE Project
   Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "nepomukserver.h"
#include "nepomukserver_export.h"

#include "nepomukversion.h"

#include <kuniqueapplication.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kglobal.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>

#include <QtDBus/QDBusConnection>

#include <signal.h>

namespace Nepomuk {
    class ServerApplication : public KUniqueApplication
    {
    public:
        ServerApplication()
            : KUniqueApplication(false /* no gui */),
              m_server( 0 ) {
        }

        int newInstance() {
            if ( !m_server ) {
                m_server = new Server( this );
            }
            return 0;
        }

        void shutdown() {
            if( m_server ) {
                m_server->quit();
            }
            else {
                KUniqueApplication::quit();
            }
        }

    private:
        Server* m_server;
    };
}


namespace {
#ifndef Q_OS_WIN
    void signalHandler( int signal )
    {
        switch( signal ) {
        case SIGHUP:
        case SIGQUIT:
        case SIGTERM:
        case SIGINT:
            if ( qApp ) {
                static_cast<Nepomuk::ServerApplication*>(qApp)->shutdown();
            }
        }
    }
#endif

    void installSignalHandler() {
#ifndef Q_OS_WIN
        struct sigaction sa;
        ::memset( &sa, 0, sizeof( sa ) );
        sa.sa_handler = signalHandler;
        sigaction( SIGHUP, &sa, 0 );
        sigaction( SIGINT, &sa, 0 );
        sigaction( SIGQUIT, &sa, 0 );
        sigaction( SIGTERM, &sa, 0 );
#endif
    }
}


extern "C" NEPOMUK_SERVER_EXPORT int kdemain( int argc, char** argv )
{
    KAboutData aboutData( "NepomukServer", "nepomukserver",
                          ki18n("Nepomuk Server"),
                          NEPOMUK_VERSION_STRING,
                          ki18n("Nepomuk Server - Manages Nepomuk storage and services"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2008-2011, Sebastian Trüg"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Sebastian Trüg"),ki18n("Maintainer"), "trueg@kde.org");

    KCmdLineArgs::init( argc, argv, &aboutData );

    KUniqueApplication::addCmdLineOptions();

    KComponentData componentData( &aboutData );

    if ( !KUniqueApplication::start() ) {
        fprintf( stderr, "Nepomuk server already running.\n" );
        return 0;
    }

    installSignalHandler();

    Nepomuk::ServerApplication app;
    app.disableSessionManagement();
    app.setQuitOnLastWindowClosed( false );
    QDBusConnection::sessionBus().unregisterObject(QLatin1String("/MainApplication"));
    return app.exec();
}
