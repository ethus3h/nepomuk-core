/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Vishesh Handa <me@vhanda.in>

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

#include "simpleindexer.h"
#include "datamanagement.h"
#include "storeresourcesjob.h"

#include "nfo.h"
#include "nie.h"
#include "kext.h"

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>

#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>

#include <KMimeType>
#include <KDebug>
#include <KJob>
#include <kde_file.h>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

Nepomuk2::SimpleIndexingJob::SimpleIndexingJob(const QUrl& fileUrl, QObject* parent)
    : KJob( parent )
    , m_nieUrl( fileUrl )
{
}

Nepomuk2::SimpleIndexingJob::SimpleIndexingJob(const QUrl& fileUrl, const QString& mimeType, QObject* parent)
    : KJob(parent)
    , m_nieUrl( fileUrl )
    , m_mimeType( mimeType )
{
}

void Nepomuk2::SimpleIndexingJob::start()
{
    SimpleResource m_res;
    m_resUri = m_res.uri();

    m_res.addProperty(NIE::url(), m_nieUrl);
    m_res.addProperty(NFO::fileName(), m_nieUrl.fileName());

    m_res.addType(NFO::FileDataObject());
    m_res.addType(NIE::InformationElement());

    QFileInfo fileInfo(m_nieUrl.toLocalFile());
    if( fileInfo.isDir() ) {
        m_res.addType(NFO::Folder());
    }
    else {
        m_res.addProperty(NFO::fileSize(), fileInfo.size());
    }

    //
    // Types by mime type
    //
    if( m_mimeType.isEmpty() ) {
        m_mimeType = KMimeType::findByUrl( m_nieUrl )->name();
    }
    QList<QUrl> types = typesForMimeType( m_mimeType );
    foreach(const QUrl& type, types)
        m_res.addType( type );

    m_res.addProperty(NIE::mimeType(), m_mimeType);

    m_res.setProperty(NIE::created(), fileInfo.created());
    m_res.setProperty(NIE::lastModified(), fileInfo.lastModified());

    // Indexing Level
    m_res.setProperty(KExt::indexingLevel(), 1);

#ifdef Q_OS_UNIX
    KDE_struct_stat statBuf;
    if( KDE_stat( QFile::encodeName(fileInfo.absoluteFilePath()).data(), &statBuf ) == 0 ) {
        m_res.setProperty( KExt::unixFileMode(), int(statBuf.st_mode) );
    }

    if( !fileInfo.owner().isEmpty() ) {
        m_res.setProperty( KExt::unixFileOwner(), fileInfo.owner() );
    }
    if( !fileInfo.group().isEmpty() ) {
        m_res.setProperty( KExt::unixFileGroup(), fileInfo.group() );
    }
#endif // Q_OS_UNIX

    QHash<QUrl, QVariant> additionalMetadata;
    additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

    SimpleResourceGraph graph;
    graph << m_res;

    // In order to be compatibile with earlier releases we keep the old app name
    KComponentData component = KGlobal::mainComponent();
    if( component.componentName() != QLatin1String("nepomukindexer") ) {
        component = KComponentData( QByteArray("nepomukindexer"),
                                    QByteArray(), KComponentData::SkipMainComponentRegistration );
    }
    StoreResourcesJob* job( Nepomuk2::storeResources( graph, IdentifyNew,
                                                      NoStoreResourcesFlags, additionalMetadata, component ) );

    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)) );
}

void Nepomuk2::SimpleIndexingJob::slotJobFinished(KJob* job_)
{
    StoreResourcesJob* job = dynamic_cast<StoreResourcesJob*>(job_);
    if( job->error() ) {
        kError() << "SimpleIndexError: " << job->errorString();

        setError( job->error() );
        setErrorText( job->errorString() );
    }

    m_resUri = job->mappings().value( m_resUri );
    emitResult();
}


QList<QUrl> Nepomuk2::SimpleIndexingJob::typesForMimeType(const QString& mimeType)
{
    QSet<QUrl> types;

    // Basic types
    if( mimeType.contains(QLatin1String("audio")) )
        types << NFO::Audio();
    if( mimeType.contains(QLatin1String("video")) )
        types << NFO::Video();
    if( mimeType.contains(QLatin1String("image")) )
        types << NFO::Image();
    if( mimeType.contains(QLatin1String("text")) )
        types << NFO::PlainTextDocument();
    if( mimeType.contains(QLatin1String("document")) )
        types << NFO::Document();

    // Presentation
    if( mimeType.contains(QLatin1String("powerpoint") ) )
        types << NFO::Presentation();
    // Spreadsheet
    if( mimeType.contains(QLatin1String("excel")) )
        types << NFO::Spreadsheet();
    // Html
    if( mimeType.contains(QLatin1String("text/html") ) )
        types << NFO::HtmlDocument();

    static QHash<QString, QUrl> typeMapper;
    if( typeMapper.isEmpty() ) {
        // Microsoft
        typeMapper.insert( QLatin1String("application/msword"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/vnd.ms-powerpoint"), NFO::Presentation() );
        typeMapper.insert( QLatin1String("application/vnd.ms-excel"), NFO::Spreadsheet() );

        // Open document formats - http://en.wikipedia.org/wiki/OpenDocument_technical_specification
        typeMapper.insert( QLatin1String("application/vnd.oasis.opendocument.text"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/vnd.oasis.opendocument.presentation"), NFO::Presentation() );
        typeMapper.insert( QLatin1String("application/vnd.oasis.opendocument.spreadsheet"), NFO::Spreadsheet() );

        // Others
        typeMapper.insert( QLatin1String("application/pdf"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/postscript"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/x-dvi"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/rtf"), NFO::PaginatedTextDocument() );

        // Ebooks
        typeMapper.insert( QLatin1String("application/epub+zip"), NFO::PaginatedTextDocument() );
        typeMapper.insert( QLatin1String("application/x-mobipocket-ebook"), NFO::PaginatedTextDocument() );

        // Archives - http://en.wikipedia.org/wiki/List_of_archive_formats
        typeMapper.insert( QLatin1String("application/x-tar"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-bzip2"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-gzip"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-lzip"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-lzma"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-lzop"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-compress"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-7z-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-ace-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-astrotite-afa"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-alz-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/vnd.android.package-archive"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-arj"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/vnd.ms-cab-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-cfs-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-dar"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-lzh"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-lzx"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-rar-compressed"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-stuffit"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-stuffitx"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/x-gtar"), NFO::Archive() );
        typeMapper.insert( QLatin1String("application/zip"), NFO::Archive() );

        // Special images
        typeMapper.insert( QLatin1String("image/vnd.microsoft.icon"), NFO::Icon() );
        typeMapper.insert( QLatin1String("image/svg+xml"), NFO::VectorImage() );
    }

    types << typeMapper.value( mimeType );
    // TODO: Add some basic NMM types?

    return types.toList();
}

QString Nepomuk2::SimpleIndexingJob::mimeType()
{
    return m_mimeType;
}

QUrl Nepomuk2::SimpleIndexingJob::uri()
{
    return m_resUri;
}

