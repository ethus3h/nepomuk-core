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

#include "ontology.h"
#include "variant.h"

static const QString NS_RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
static const QString NS_RDFS = "http://www.w3.org/2000/01/rdf-schema#";

QString Nepomuk::KMetaData::Ontology::defaultGraph()
{
  // FIXME: replace this with the real thing
  static QString s = "http://nepomuk.semanticdesktop.org/metadata";
  return s;
}


QString Nepomuk::KMetaData::Ontology::typePredicate()
{
  static QString s = NS_RDF + "type";
  return s;
}


QString Nepomuk::KMetaData::Ontology::valueToRDFLiteral( const Variant& v )
{
  // FIXME: replace this with the real thing
  return v.toString();
}


template<typename T> QStringList convertList( const QList<T>& vl )
{
  QStringList l;
  QListIterator<T> it( vl );
  while( it.hasNext() )
    l.append( Nepomuk::KMetaData::Ontology::valueToRDFLiteral( Nepomuk::KMetaData::Variant( it.next() ) ) );
  return l;
}


QStringList Nepomuk::KMetaData::Ontology::valuesToRDFLiterals( const Variant& v )
{
  if( v.simpleType() == qMetaTypeId<int>() )
    return convertList( v.toIntList() );
  else if( v.simpleType() == qMetaTypeId<double>() )
    return convertList( v.toDoubleList() );
  else if( v.simpleType() == qMetaTypeId<bool>() )
    return convertList( v.toBoolList() );
  else if( v.simpleType() == qMetaTypeId<QDate>() )
    return convertList( v.toDateList() );
  else if( v.simpleType() == qMetaTypeId<QTime>() )
    return convertList( v.toTimeList() );
  else if( v.simpleType() == qMetaTypeId<QDateTime>() )
    return convertList( v.toDateTimeList() );
  else if( v.simpleType() == qMetaTypeId<QUrl>() )
    return convertList( v.toUrlList() );
  else if( v.simpleType() == QVariant::String )
    return v.toStringList();
  else {
    qDebug() << "(Ontology) ERROR: unknown list type: " << v.simpleType() << endl;
    return QStringList();
  }
}


Nepomuk::KMetaData::Variant Nepomuk::KMetaData::Ontology::RDFLiteralToValue( const QString& s )
{
  // FIXME: replace this with the real thing
  return Variant( s );
}


QString Nepomuk::KMetaData::Ontology::rdfNamespace()
{
  return NS_RDF;
}


QString Nepomuk::KMetaData::Ontology::rdfsNamespace()
{
  return NS_RDFS;
}


QString Nepomuk::KMetaData::Ontology::nrlNamespace()
{
  // FIXME
  return QString();
}
