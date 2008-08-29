/* 
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING.LIB" for the exact licensing terms.
 */

#ifndef _RESOURCE_TEMPLATE_H_
#define _RESOURCE_TEMPLATE_H_


static const QString gplTemplate = 
"/*\n"
" * This file is part of the Nepomuk KDE project.\n"
" *\n"
" * This library is free software; you can redistribute it and/or modify\n"
" * it under the terms of the GNU Lesser General Public License as published by\n"
" * the Free Software Foundation; either version 2 of the License, or\n"
" * (at your option) any later version.\n"
" * See the file \"COPYING\" for the exact licensing terms.\n"
" */\n"
"\n"
"/*\n"
" * This file has been generated by the Nepomuk Resource class generator.\n"
" * DO NOT EDIT THIS FILE.\n"
" * ANY CHANGES WILL BE LOST.\n"
" */\n";

static const QString headerTemplate = gplTemplate +
"\n"
"#ifndef _NEPOMUK_RESOURCENAMEUPPER_H_\n"
"#define _NEPOMUK_RESOURCENAMEUPPER_H_\n"
"\n"
"class QDateTime;\n"
"class QDate;\n"
"class QTime;\n"
"\n"
"namespace Nepomuk {\n"
"NEPOMUK_OTHERCLASSES"
"}\n"
"\n"
"#include NEPOMUK_PARENT_INCLUDE\n"
"#include <nepomuk/nepomuk_export.h>\n"
"\n"
"namespace Nepomuk {\n"
"\n"
"NEPOMUK_RESOURCECOMMENT\n"
"    class NEPOMUK_EXPORT NEPOMUK_RESOURCENAME : public NEPOMUK_PARENTRESOURCE\n"
"    {\n"
"    public:\n"
"        /**\n"
"         * Create a new empty and invalid NEPOMUK_RESOURCENAME instance\n"
"         */\n"
"        NEPOMUK_RESOURCENAME();\n"
"        /**\n"
"         * Default copy constructor\n"
"         */\n"
"        NEPOMUK_RESOURCENAME( const NEPOMUK_RESOURCENAME& );\n"
"        NEPOMUK_RESOURCENAME( const Resource& );\n"
"        /**\n"
"         * Create a new NEPOMUK_RESOURCENAME instance representing the resource\n"
"         * referenced by \\a uriOrIdentifier.\n"
"         */\n"
"        NEPOMUK_RESOURCENAME( const QString& uriOrIdentifier );\n"
"        /**\n"
"         * Create a new NEPOMUK_RESOURCENAME instance representing the resource\n"
"         * referenced by \\a uri.\n"
"         */\n"
"        NEPOMUK_RESOURCENAME( const QUrl& uri );\n"
"        ~NEPOMUK_RESOURCENAME();\n"
"\n"
"        NEPOMUK_RESOURCENAME& operator=( const NEPOMUK_RESOURCENAME& );\n"
"\n"
"NEPOMUK_METHODS\n"
"\n"
"        /**\n"
"         * \\return The URI of the resource type that is used in NEPOMUK_RESOURCENAME instances.\n"
"         */\n"
"        static QString resourceTypeUri();\n"
"\n"
"    protected:\n"
"       NEPOMUK_RESOURCENAME( const QString& uri, const QUrl& type );\n"
"       NEPOMUK_RESOURCENAME( const QUrl& uri, const QUrl& type );\n"
"   };\n"
"}\n"
"\n"
"#endif\n";


static const QString sourceTemplate = gplTemplate +
"\n"
"#include <nepomuk/tools.h>\n"
"#include <nepomuk/variant.h>\n"
"#include <nepomuk/resourcemanager.h>\n"
"#include \"NEPOMUK_RESOURCENAMELOWER.h\"\n"
"\n"
"NEPOMUK_INCLUDES"
"\n"
"#include <QtCore/QDateTime>\n"
"#include <QtCore/QDate>\n"
"#include <QtCore/QTime>\n"
"\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME()\n"
"  : NEPOMUK_PARENTRESOURCE( QUrl(), QUrl::fromEncoded(\"NEPOMUK_RESOURCETYPEURI\") )\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const NEPOMUK_RESOURCENAME& res )\n"
"  : NEPOMUK_PARENTRESOURCE( res )\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const Nepomuk::Resource& res )\n"
"  : NEPOMUK_PARENTRESOURCE( res )\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QString& uri )\n"
"  : NEPOMUK_PARENTRESOURCE( uri, QUrl::fromEncoded(\"NEPOMUK_RESOURCETYPEURI\") )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QUrl& uri )\n"
"  : NEPOMUK_PARENTRESOURCE( uri, QUrl::fromEncoded(\"NEPOMUK_RESOURCETYPEURI\") )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QString& uri, const QUrl& type )\n"
"  : NEPOMUK_PARENTRESOURCE( uri, type )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QUrl& uri, const QUrl& type )\n"
"  : NEPOMUK_PARENTRESOURCE( uri, type )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME::~NEPOMUK_RESOURCENAME()\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::NEPOMUK_RESOURCENAME& Nepomuk::NEPOMUK_RESOURCENAME::operator=( const NEPOMUK_RESOURCENAME& res )\n"
"{\n"
"    Resource::operator=( res );\n"
"    return *this;\n"
"}\n"
"\n"
"\n"
"QString Nepomuk::NEPOMUK_RESOURCENAME::resourceTypeUri()\n"
"{\n"
"    return \"NEPOMUK_RESOURCETYPEURI\";\n"
"}\n"
"\n"
"NEPOMUK_METHODS\n";

#endif
