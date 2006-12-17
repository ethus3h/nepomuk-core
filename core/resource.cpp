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

#include "resource.h"
#include "resourcedata.h"
#include "resourcemanager.h"

#include <klocale.h>


Nepomuk::KMetaData::Resource::Resource()
{
  // invalid data
  m_data = new ResourceData();
  m_data->ref();
}


Nepomuk::KMetaData::Resource::Resource( const Nepomuk::KMetaData::Resource::Resource& res )
{
  m_data = res.m_data;
  m_data->ref();
}


Nepomuk::KMetaData::Resource::Resource( const QString& uri, const QString& type )
{
  m_data = ResourceData::data( uri, type );
  m_data->ref();
}


Nepomuk::KMetaData::Resource::~Resource()
{
  if( !m_data->deref() ) {
    if( ResourceManager::instance()->autoSync() && modified() )
      sync();
    m_data->deleteData();
  }
}


Nepomuk::KMetaData::Resource& Nepomuk::KMetaData::Resource::operator=( const Resource& res )
{
  if( m_data != res.m_data ) {
    if( !m_data->deref() ) {
      if( ResourceManager::instance()->autoSync() && modified() )
	sync();
      m_data->deleteData();
    }
    m_data = res.m_data;
    m_data->ref();
  }

  return *this;
}


const QString& Nepomuk::KMetaData::Resource::uri() const
{
  return m_data->uri;
}


const QString& Nepomuk::KMetaData::Resource::type() const
{
  m_data->init();
  return m_data->type;
}


QString Nepomuk::KMetaData::Resource::className() const
{
  return type().section( '#', -1 );
}


int Nepomuk::KMetaData::Resource::sync()
{
  // FIXME: inform the manager about failures

  m_data->init();

  if( m_data->merge() )
    if( m_data->save() )
      return 0;

  return -1;
}


QHash<QString, Nepomuk::KMetaData::Variant> Nepomuk::KMetaData::Resource::allProperties() const
{
  m_data->init();

  QHash<QString, Variant> l;
  for( ResourceData::PropertiesMap::const_iterator it = m_data->properties.constBegin();
       it != m_data->properties.constEnd(); ++it )
    l.insert( it.key(), it.value().first );
  return l;
}


Nepomuk::KMetaData::Variant Nepomuk::KMetaData::Resource::getProperty( const QString& uri ) const
{
  m_data->init();

  ResourceData::PropertiesMap::iterator it = m_data->properties.find( uri );
  if( it != m_data->properties.end() )
    if( !( it.value().second & ResourceData::Deleted ) )
      return it.value().first;
  
  return Variant();
}


void Nepomuk::KMetaData::Resource::setProperty( const QString& uri, const Nepomuk::KMetaData::Variant& value )
{
  m_data->init();

  // mark the value as modified
  m_data->properties[uri] = qMakePair<Variant, int>( value, ResourceData::Modified );
}


void Nepomuk::KMetaData::Resource::removeProperty( const QString& uri )
{
  m_data->init();

  ResourceData::PropertiesMap::iterator it = m_data->properties.find( uri );
  if( it != m_data->properties.end() )
    it.value().second = ResourceData::Modified|ResourceData::Deleted;
}


bool Nepomuk::KMetaData::Resource::modified() const
{
  for( ResourceData::PropertiesMap::const_iterator it = m_data->properties.constBegin();
       it != m_data->properties.constEnd(); ++it )
    if( it.value().second & ResourceData::Modified )
      return true;

  return false;
}


bool Nepomuk::KMetaData::Resource::inSync() const
{
  return m_data->inSync();
}


bool Nepomuk::KMetaData::Resource::exists() const
{
  return m_data->exists();
}


bool Nepomuk::KMetaData::Resource::isValid() const
{
  return m_data->isValid();
}


bool Nepomuk::KMetaData::Resource::operator==( const Resource& other ) const
{
  if( this == &other )
    return true;

  return( *m_data == *other.m_data );
}


QString Nepomuk::KMetaData::errorString( int code )
{
  switch( code ) {
  case ERROR_SUCCESS:
    return i18n("Success");
  case ERROR_COMMUNICATION:
    return i18n("Communication error");
  case ERROR_INVALID_TYPE:
    return i18n("Invalid type in Database");
  default:
    return i18n("Unknown error");
  }
}
