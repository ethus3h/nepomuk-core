/***************************************************************************
 *   Copyright (C) 2006 by Tobias Koenig <tokoe@kde.org>                   *
 *   Copyright (C) 2008-2011 by Sebastian Trueg <trueg@kde.org>            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "processcontrol.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>

#include <KDebug>

ProcessControl::ProcessControl( QObject *parent )
    : QObject( parent ), mFailedToStart( false ), mCrashCount( 0 )
{
    connect( &mProcess, SIGNAL( error( QProcess::ProcessError ) ),
             this, SLOT( slotError( QProcess::ProcessError ) ) );
    connect( &mProcess, SIGNAL( finished( int, QProcess::ExitStatus ) ),
             this, SLOT( slotFinished( int, QProcess::ExitStatus ) ) );
    mProcess.setProcessChannelMode( QProcess::ForwardedChannels );
}

ProcessControl::~ProcessControl()
{
    mProcess.disconnect(this);
    terminate(true);
}

void ProcessControl::start( const QString &application, const QStringList &arguments, CrashPolicy policy, int maxCrash )
{
    mFailedToStart = false;

    mApplication = application;
    mArguments = arguments;
    mPolicy = policy;
    mCrashCount = maxCrash;

    start();
}

void ProcessControl::setCrashPolicy( CrashPolicy policy )
{
    mPolicy = policy;
}

void ProcessControl::slotError( QProcess::ProcessError error )
{
    switch ( error ) {
    case QProcess::Crashed:
        // do nothing, we'll respawn in slotFinished
        break;
    case QProcess::FailedToStart:
    default:
        mFailedToStart = true;
        break;
    }

    qDebug( "ProcessControl: Application '%s' stopped unexpected (%s)",
            qPrintable( mApplication ), qPrintable( mProcess.errorString() ) );
}

void ProcessControl::slotFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    // the process went down -> inform clients
    emit finished(false);

    // Since Nepomuk services are KApplications and, thus, use DrKonqi QProcess does not
    // see a crash as an actual CrashExit but as a normal exit with an exit code != 0
    if ( exitStatus == QProcess::CrashExit ||
         exitCode != 0 ) {
        if ( mPolicy == RestartOnCrash ) {
            // don't try to start an unstartable application
            if ( !mFailedToStart && --mCrashCount >= 0 ) {
                qDebug( "Application '%s' crashed! %d restarts left.", qPrintable( commandLine() ), mCrashCount );
                start();
            }
            else {
                if ( mFailedToStart ) {
                    qDebug( "Application '%s' failed to start!", qPrintable( commandLine() ) );
                }
                else {
                    qDebug( "Application '%s' crashed to often. Giving up!", qPrintable( commandLine() ) );
                }
            }
        }
        else {
            qDebug( "Application '%s' crashed. No restart!", qPrintable( commandLine() ) );
        }
    }
    else {
        qDebug( "Application '%s' exited normally...", qPrintable( commandLine() ) );
    }
}

void ProcessControl::start()
{
    mProcess.start( mApplication, mArguments );
}

bool ProcessControl::isRunning() const
{
    return mProcess.state() != QProcess::NotRunning;
}

QString ProcessControl::commandLine() const
{
    return mApplication + QLatin1String(" ") + mArguments.join(QLatin1String(" "));
}

void ProcessControl::terminate( bool waitAndKill )
{
    if(isRunning()) {
        mProcess.terminate();
        // kill if not stopped after timeout
        if(waitAndKill ||
           QCoreApplication::instance()->closingDown()) {
            if(!mProcess.waitForFinished(20000)) {
                mProcess.kill();
            }
        }
        else {
            QTimer::singleShot(20000, &mProcess, SLOT(kill()));
        }
    }
}

bool ProcessControl::waitForStarted()
{
    return mProcess.waitForStarted();
}

#include "processcontrol.moc"
