/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

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


#include "resourcewatcher.h"

class Nepomuk::ResourceWatcher::Private {
public:
    QList<Types::Class> m_types;
    QList<Nepomuk::Resource> m_resources;
    QList<Types::Property> m_properties;
};

Nepomuk::ResourceWatcher::ResourceWatcher(QObject* parent): QObject(parent)
{

}

Nepomuk::ResourceWatcher::~ResourceWatcher()
{

}

void Nepomuk::ResourceWatcher::watch()
{
    //
    // Convert to list of strings
    //
    QList<QString> uris;
    foreach( const Nepomuk::Resource & res, d->m_resources ) {
        uris << res.resourceUri().toString();
    }

    QList<QString> props;
    foreach( const Types::Property & prop, d->m_properties ) {
        props << prop.uri().toString();
    }

    QList<QString> types_;
    foreach( const Types::Class & cl, d->m_types ) {
        types_ << cl.uri().toString();
    }

    //
    // Create the dbus object to watch
    //
}

void Nepomuk::ResourceWatcher::addProperty(const Nepomuk::Types::Property& property)
{
    d->m_properties << property;
}

void Nepomuk::ResourceWatcher::addResource(const Nepomuk::Resource& res)
{
    d->m_resources << res;
}

void Nepomuk::ResourceWatcher::addType(const Nepomuk::Types::Class& type)
{
    d->m_types << type;
}

QList< Nepomuk::Types::Property > Nepomuk::ResourceWatcher::properties() const
{
    return d->m_properties;
}

QList<Nepomuk::Resource> Nepomuk::ResourceWatcher::resources() const
{
    return d->m_resources;
}

QList< Nepomuk::Types::Class > Nepomuk::ResourceWatcher::types() const
{
    return d->m_types;
}

void Nepomuk::ResourceWatcher::setProperties(const QList< Nepomuk::Types::Property >& properties_)
{
    d->m_properties = properties_;
}

void Nepomuk::ResourceWatcher::setResources(const QList< Nepomuk::Resource >& resources_)
{
    d->m_resources = resources_;
}

void Nepomuk::ResourceWatcher::setTypes(const QList< Nepomuk::Types::Class >& types_)
{
    d->m_types = types_;
}





