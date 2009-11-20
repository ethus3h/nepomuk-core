/*
 * This file has been generated by the onto2vocabularyclass tool
 * copyright (C) 2007-2008 Sebastian Trueg <trueg@kde.org>
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

#include "nfo.h"

class NfoPrivate
{
public:
    NfoPrivate()
        : nfo_namespace( QUrl::fromEncoded( "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#", QUrl::StrictMode ) ),
          nfo_FileDataObject( QUrl::fromEncoded( "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#FileDataObject", QUrl::StrictMode ) ),
          nfo_fileName( QUrl::fromEncoded( "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#fileName", QUrl::StrictMode ) ) {
    }

    QUrl nfo_namespace;
    QUrl nfo_FileDataObject;
    QUrl nfo_fileName;
};

Q_GLOBAL_STATIC( NfoPrivate, s_nfo )

QUrl Nepomuk::Vocabulary::NFO::nfoNamespace()
{
    return s_nfo()->nfo_namespace;
}

QUrl Nepomuk::Vocabulary::NFO::FileDataObject()
{
    return s_nfo()->nfo_FileDataObject;
}

QUrl Nepomuk::Vocabulary::NFO::fileName()
{
    return s_nfo()->nfo_fileName;
}