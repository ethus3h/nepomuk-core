/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2009 Sebastian Trueg <trueg@kde.org>

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

#include "negationterm.h"
#include "negationterm_p.h"
#include "querybuilderdata_p.h"

QString Nepomuk::Query::NegationTermPrivate::toSparqlGraphPattern( const QString& resourceVarName, QueryBuilderData* qbd ) const
{
    QString varName = qbd->uniqueVarName();
    return QString( "OPTIONAL { %1 . FILTER(%2=%3) . } . FILTER(!BOUND(%2)) . " )
        .arg( m_subTerm.d_ptr->toSparqlGraphPattern( varName, qbd ) )
        .arg( varName )
        .arg( resourceVarName );
}


QString Nepomuk::Query::NegationTermPrivate::toString() const
{
    return QString( "[NOT %1]" ).arg( m_subTerm.d_ptr->toString() );
}


Nepomuk::Query::NegationTerm::NegationTerm()
    : SimpleTerm( new NegationTermPrivate() )
{
}


Nepomuk::Query::NegationTerm::NegationTerm( const NegationTerm& term )
    : SimpleTerm( term )
{
}


Nepomuk::Query::NegationTerm::~NegationTerm()
{
}


Nepomuk::Query::NegationTerm& Nepomuk::Query::NegationTerm::operator=( const NegationTerm& term )
{
    d_ptr = term.d_ptr;
    return *this;
}


Nepomuk::Query::Term Nepomuk::Query::NegationTerm::negateTerm( const Term& term )
{
    if ( term.isNegationTerm() ) {
        return term.toNegationTerm().subTerm();
    }
    else {
        NegationTerm nt;
        nt.setSubTerm( term );
        return nt;
    }
}
