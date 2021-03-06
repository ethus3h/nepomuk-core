/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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

#include "dbustypes.h"

#include <QtCore/QStringList>
#include <QtCore/QDate>
#include <QtCore/QTime>
#include <QtCore/QDateTime>
#include <QtDBus/QDBusMetaType>

#include <KUrl>
#include <KDebug>

QString Nepomuk2::DBus::convertUri(const QUrl& uri)
{
    return KUrl(uri).url();
}

QStringList Nepomuk2::DBus::convertUriList(const QList<QUrl>& uris)
{
    QStringList uriStrings;
    foreach(const QUrl& uri, uris)
        uriStrings << convertUri(uri);
    return uriStrings;
}

QVariantList Nepomuk2::DBus::normalizeVariantList(const QVariantList& l)
{
    QVariantList newL;
    QListIterator<QVariant> it(l);
    while(it.hasNext()) {
        QVariant v = it.next();
        if(v.userType() == qMetaTypeId<KUrl>()) {
            newL.append(QVariant(QUrl(v.value<KUrl>())));
        }
        else {
            newL.append(v);
        }
    }
    return newL;
}

QVariant Nepomuk2::DBus::resolveDBusArguments(const QVariant& v)
{
    //
    // trueg: QDBus does not automatically convert non-basic types but gives us a QDBusArgument in a QVariant.
    // Thus, we need to handle QUrl, QTime, QDate, and QDateTime as a special cases here. They is the only complex types we support.
    //
    if(v.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();

        QVariant v;
        if(arg.currentSignature() == QLatin1String("(s)")) {
            QUrl url;
            arg >> url;
            return url;
        }
        else if(arg.currentSignature() == QLatin1String("(iii)")) {
            QDate date;
            arg >> date;
            return date;
        }
        else if(arg.currentSignature() == QLatin1String("(iiii)")) {
            QTime time;
            arg >> time;
            return time;
        }
        else if(arg.currentSignature() == QLatin1String("((iii)(iiii)i)")) {
            QDateTime dt;
            arg >> dt;
            return dt;
        }
        else {
            kDebug() << "Unknown type signature in property hash value:" << arg.currentSignature();
            return QVariant();
        }
    }
    else {
        return v;
    }
}

QVariantList Nepomuk2::DBus::resolveDBusArguments(const QVariantList& l)
{
    QVariantList newL;
    QListIterator<QVariant> it(l);
    while(it.hasNext()) {
        newL.append(resolveDBusArguments(it.next()));
    }
    return newL;
}

void Nepomuk2::DBus::registerDBusTypes()
{
    // we need QUrl to be able to pass it in a QVariant
    qDBusRegisterMetaType<QUrl>();

    // the central struct for storeResources and describeResources
    qDBusRegisterMetaType<Nepomuk2::SimpleResource>();

    // we use a list instead of a struct for SimpleResourceGraph
    qDBusRegisterMetaType<QList<Nepomuk2::SimpleResource> >();

    // required for the additional metadata in storeResources
    qDBusRegisterMetaType<Nepomuk2::PropertyHash>();

    // required for returning the mappings in storeResources
    qDBusRegisterMetaType< QHash<QString, QString> >();
}

// We need the QUrl serialization to be able to pass URIs in variants
QDBusArgument& operator<<( QDBusArgument& arg, const QUrl& url )
{
    arg.beginStructure();
    arg << QString::fromAscii(url.toEncoded());
    arg.endStructure();
    return arg;
}

// We need the QUrl serialization to be able to pass URIs in variants
const QDBusArgument& operator>>( const QDBusArgument& arg, QUrl& url )
{
    arg.beginStructure();
    QString uriString;
    arg >> uriString;
    url = QUrl::fromEncoded(uriString.toAscii());
    arg.endStructure();
    return arg;
}

QDBusArgument& operator<<( QDBusArgument& arg, const Nepomuk2::PropertyHash& ph )
{
    arg.beginMap( QVariant::String, qMetaTypeId<QDBusVariant>());
    for(Nepomuk2::PropertyHash::const_iterator it = ph.constBegin();
        it != ph.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << QString::fromAscii(it.key().toEncoded());

        // a small hack to allow usage of KUrl
        if(it.value().userType() == qMetaTypeId<KUrl>())
            arg << QDBusVariant(QUrl(it.value().value<KUrl>()));
        else
            arg << QDBusVariant(it.value());

        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

const QDBusArgument& operator>>( const QDBusArgument& arg, Nepomuk2::PropertyHash& ph )
{
    ph.clear();
    arg.beginMap();
    while(!arg.atEnd()) {
        QString key;
        QDBusVariant value;
        arg.beginMapEntry();
        arg >> key >> value;

        const QUrl p = QUrl::fromEncoded(key.toAscii());
        const QVariant v = Nepomuk2::DBus::resolveDBusArguments(value.variant());

        ph.insertMulti(p, v);

        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

QDBusArgument& operator<<( QDBusArgument& arg, const Nepomuk2::SimpleResource& res )
{
    arg.beginStructure();
    arg << QString::fromAscii(res.uri().toEncoded());
    arg << res.properties();
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>( const QDBusArgument& arg, Nepomuk2::SimpleResource& res )
{
    arg.beginStructure();
    QString uriS;
    Nepomuk2::PropertyHash props;
    arg >> uriS;
    res.setUri( QUrl::fromEncoded(uriS.toAscii()) );
    arg >> props;
    res.setProperties(props);
    arg.endStructure();
    return arg;
}
