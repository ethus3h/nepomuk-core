/* 
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

static const QString headerTemplate = 
"/*\n"
" *\n"
" * $Id: $\n"
" *\n"
" * This file is part of the Nepomuk KDE project.\n"
" * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>\n"
" *\n"
" * This program is free software; you can redistribute it and/or modify\n"
" * it under the terms of the GNU General Public License as published by\n"
" * the Free Software Foundation; either version 2 of the License, or\n"
" * (at your option) any later version.\n"
" * See the file \"COPYING\" for the exact licensing terms.\n"
" */\n"
"\n"
"/*\n"
" * This file has been generated by the libKMetaData Resource class generator.\n"
" * DO NOT EDIT THIS FILE.\n"
" * ANY CHANGES WILL BE LOST.\n"
" */\n"
"\n"
"#ifndef _NEPOMUK_KMETADATA_RESOURCENAMEUPPER_H_\n"
"#define _NEPOMUK_KMETADATA_RESOURCENAMEUPPER_H_\n"
"\n"
"#include <kmetadata/PARENTRESOURCELOWER.h>\n"
"#include <kmetadata/kmetadata_export.h>\n"
"\n"
"namespace Nepomuk {\n"
"namespace KMetaData {\n"
"\n"
"RESOURCECOMMENT\n"
"class KMETADATA_EXPORT RESOURCENAME : public PARENTRESOURCE\n"
"{\n"
" public:\n"
"   /**\n"
"    * Create a new RESOURCENAME instance representing the resource\n"
"    * referenced by \\a uri.\n"
"    */\n"
"   RESOURCENAME( const QString& uri );\n"
"   ~RESOURCENAME();\n"
"\n"
"METHODS\n"
" protected:\n"
"   RESOURCENAME( const QString& uri, const QString& type );\n"
"};\n"
"}\n"
"}\n"
"\n"
"#endif\n";


static const QString sourceTemplate = 
"/*\n"
" *\n"
" * $Id: $\n"
" *\n"
" * This file is part of the Nepomuk KDE project.\n"
" * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>\n"
" *\n"
" * This program is free software; you can redistribute it and/or modify\n"
" * it under the terms of the GNU General Public License as published by\n"
" * the Free Software Foundation; either version 2 of the License, or\n"
" * (at your option) any later version.\n"
" * See the file \"COPYING\" for the exact licensing terms.\n"
" */\n"
"\n"
"/*\n"
" * This file has been generated by the libKMetaData Resource class generator.\n"
" * DO NOT EDIT THIS FILE.\n"
" * ANY CHANGES WILL BE LOST.\n"
" */\n"
"\n"
"#include \"RESOURCENAMELOWER.h\"\n"
"\n"
"Nepomuk::KMetaData::RESOURCENAME::RESOURCENAME( const QString& uri )\n"
"  : PARENTRESOURCE( uri, \"RESOURCETYPEURI\" )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::KMetaData::RESOURCENAME::RESOURCENAME( const QString& uri, const QString& type )\n"
"  : PARENTRESOURCE( uri, type )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::KMetaData::RESOURCENAME::~RESOURCENAME()\n"
"{\n"
"}\n"
"\n"
"METHODS\n";
