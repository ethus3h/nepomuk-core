/*
 * Copyright (C) 2012  Rohan Garg <rohangarg@kubuntu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef OKULAREXTRACTOR_H
#define OKULAREXTRACTOR_H

#include "extractorplugin.h"

namespace Nepomuk2 {
class OkularExtractor : public ExtractorPlugin
{
public:
    OkularExtractor(QObject *parent, const QVariantList&);
    virtual QStringList mimetypes();
    virtual SimpleResourceGraph extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType);
};
}
#endif // OKULAREXTRACTOR_H