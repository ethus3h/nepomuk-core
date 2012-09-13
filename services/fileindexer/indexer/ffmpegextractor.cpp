/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    Code adapted from Strigi FFmpeg Analyzer -
    Copyright (C) 2010 Evgeny Egorochkin <phreedom.stdin@gmail.com>
    Copyright (C) 2011 Tirtha Chatterjee <tirtha.p.chatterjee@gmail.com>

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


#include "ffmpegextractor.h"

#ifdef __cplusplus
    #define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
    #undef _STDINT_H
#endif
    # include <stdint.h>
#endif

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavcodec/avcodec.h>
}

#include "nmm.h"
#include "nie.h"
#include "nfo.h"
#include <nco.h>

#include <Soprano/Vocabulary/NAO>

#include <KDebug>
#include <QtCore/QDateTime>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

namespace Nepomuk2 {

FFmpegExtractor::FFmpegExtractor(QObject* parent, const QVariantList&)
: Extractor(parent)
{

}

QStringList FFmpegExtractor::mimetypes()
{
    QStringList types;

    types << QLatin1String("video/mpeg");
    types << QLatin1String("video/x-msvideo");

    //FIXME: Discover more types

    return types;
}

SimpleResourceGraph FFmpegExtractor::extract(const QUrl& resUri, const QUrl& fileUrl)
{
    AVFormatContext *fmt_ctx = NULL;

    av_register_all();

    QByteArray arr = fileUrl.toLocalFile().toUtf8();

    fmt_ctx = avformat_alloc_context();
    if ( int ret = avformat_open_input(&fmt_ctx, arr.data(), NULL, NULL) ) {
        kError() << "avformat_open_input error: " << ret;
        return SimpleResourceGraph();
    }

    int ret = avformat_find_stream_info(fmt_ctx, NULL);
    if( ret < 0 ) {
        kError() << "avform_find_stream_info error: " << ret;
        return SimpleResourceGraph();
    }

    SimpleResourceGraph graph;
    SimpleResource fileRes( resUri );

    if( fmt_ctx->nb_streams == 1 && fmt_ctx->streams[0]->codec->codec_type == AVMEDIA_TYPE_AUDIO ) {
        fileRes.addType( NMM::MusicPiece() );
    }
    else {
        fileRes.addType( NFO::Video() );
    }

    int totalSecs = fmt_ctx->duration / AV_TIME_BASE;
    int bitrate = fmt_ctx->bit_rate;

    fileRes.setProperty( NFO::duration(), totalSecs );
    fileRes.setProperty( NFO::averageBitrate(), bitrate );

    for( uint i=0; i<fmt_ctx->nb_streams; i++ ) {
        const AVStream* stream = fmt_ctx->streams[i];
        const AVCodecContext* codec = stream->codec;

        if( codec->codec_type == AVMEDIA_TYPE_AUDIO || codec->codec_type == AVMEDIA_TYPE_VIDEO ) {
            SimpleResource subRes;
            subRes.addType( NFO::EmbeddedFileDataObject() );
            subRes.addProperty( NIE::isPartOf(), fileRes );

            int dur = stream->duration / AV_TIME_BASE;
            if( dur ) {
                subRes.addProperty( NFO::duration(), dur );
            }

            QString codecName = QString::fromLatin1( codec->codec_name );
            if( codecName.isEmpty() ) {
                const AVCodec* p = avcodec_find_decoder( codec->codec_id );
                if( p )
                    codecName = QString::fromLatin1( p->name );
            }

            if( !codecName.isEmpty() ) {
                subRes.addProperty( NFO::codec(), codecName );
            }

            int bitrate = codec->bit_rate;
            if( bitrate ) {
                subRes.addProperty( NFO::averageBitrate(), bitrate );
            }

            if( codec->codec_type == AVMEDIA_TYPE_AUDIO ) {
                subRes.addType( NFO::Audio() );
                subRes.addProperty( NFO::sampleRate(), codec->sample_rate );
                subRes.addProperty( NFO::channels(), codec->channels );

                //TODO: Fetch Sample Format
            }
            else if( codec->codec_type == AVMEDIA_TYPE_VIDEO ) {
                subRes.addType( NFO::Video() );
                subRes.addProperty( NFO::width(), codec->width );
                subRes.addProperty( NFO::height(), codec->height );

                int aspectRatio = codec->sample_aspect_ratio.num / codec->sample_aspect_ratio.den;
                int frameRate = stream->r_frame_rate.num / stream->r_frame_rate.den;

                subRes.addProperty( NFO::aspectRatio(), aspectRatio );
                subRes.addProperty( NFO::frameRate(), frameRate );
            }

            fileRes.addProperty( NIE::hasPart(), subRes );
            fileRes.addProperty( NAO::hasSubResource(), subRes );
            graph << subRes;
        }
    }

    AVDictionary* dict = fmt_ctx->metadata;
    AVDictionaryEntry *entry;

    entry = av_dict_get(dict, "title", NULL, 0);
    if( entry ) {
        fileRes.addProperty( NIE::title(), QString::fromUtf8( entry->value ) );
    }


    entry = av_dict_get(dict, "author", NULL, 0);
    if( entry ) {
        SimpleResource author;
        author.addType( NCO::Contact() );
        author.addProperty( NCO::fullname(), QString::fromUtf8( entry->value ) );
        graph << author;

        fileRes.addProperty( NCO::creator(), author );
        fileRes.addProperty( NAO::hasSubResource(), author );
    }

    entry = av_dict_get(dict, "copyright", NULL, 0);
    if( entry ) {
        fileRes.addProperty( NIE::copyright(), QString::fromUtf8( entry->value ) );
    }

    entry = av_dict_get(dict, "comment", NULL, 0);
    if( entry ) {
        fileRes.addProperty( NIE::comment(), QString::fromUtf8( entry->value ) );
    }

    entry = av_dict_get(dict, "album", NULL, 0);
    if( entry ) {
        SimpleResource album;
        album.addType( NMM::MusicAlbum() );
        album.setProperty( NIE::title(), QString::fromUtf8( entry->value ) );
        graph << album;

        fileRes.addProperty( NMM::musicAlbum(), album );
        fileRes.addProperty( NAO::hasSubResource(), album );
    }

    entry = av_dict_get(dict, "genre", NULL, 0);
    if( entry ) {
        QString value = QString::fromUtf8( entry->value );
        fileRes.addProperty( NMM::genre(), value );
    }


    entry = av_dict_get(dict, "track", NULL, 0);
    if( entry ) {
        QString value = QString::fromUtf8( entry->value );

        bool ok = false;
        int track = value.toInt( &ok );
        if( ok && track )
            fileRes.addProperty( NMM::trackNumber(), track );
    }

    entry = av_dict_get(dict, "year", NULL, 0);
    if( entry ) {
        //FIXME: Parse date in different formats
        QString value = QString::fromUtf8( entry->value );
        QDate date = QDate::fromString("yyyy", value);
        fileRes.addProperty( NIE::contentCreated(), date );
    }

    avformat_close_input(&fmt_ctx);

    graph << fileRes;
    return graph;
}

}

NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::FFmpegExtractor, "nepomukffmpegextractor" )