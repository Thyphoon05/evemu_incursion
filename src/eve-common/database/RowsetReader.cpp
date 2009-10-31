/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2008 The EVEmu Team
    For the latest information visit http://evemu.mmoforge.org
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:     Zhur
*/

#include "EVECommonPCH.h"

#include "database/RowsetReader.h"
#include "database/RowsetToSQL.h"
#include "python/PyRep.h"

BaseRowsetReader::base_iterator::base_iterator(const BaseRowsetReader *parent, bool is_end)
: m_end(is_end),
  m_index(0),
  m_baseReader(parent)
{
}

const BaseRowsetReader::base_iterator &RowsetReader::base_iterator::operator++() {
    m_index++;
    if(m_index >= m_baseReader->size())
        m_end = true;
    return(*this);
}

bool BaseRowsetReader::base_iterator::_baseEqual(const base_iterator &other) const {
    if(m_end && other.m_end)
        return true;    //'end' iterator
    if(m_end || other.m_end)
        return false;
    return(m_index == other.m_index);
}


uint32 BaseRowsetReader::base_iterator::_find(const char *fieldname) const {
    if(m_end)
        return(InvalidRow);
    std::map<std::string, uint32>::const_iterator res;
    res = m_baseReader->m_fieldMap.find(fieldname);
    if(res == m_baseReader->m_fieldMap.end())
        return(InvalidRow);
    return(res->second);
}

PyRep *BaseRowsetReader::base_iterator::_find(uint32 index) const {
    if(m_end)
        return NULL;
    if(index == InvalidRow)
        return NULL;

    PyRep *row = m_baseReader->_getRow(m_index);
    if(row == NULL || !row->IsList())
        return NULL;

    PyList *rowl = (PyList *) row;
    if(rowl->items.size() <= index)
        return NULL;

    return(rowl->items[index]);
}

//not using _find to allow for more verbose error messages
const char *BaseRowsetReader::base_iterator::GetString(const char *fieldname) const {
    if(m_end)
        return("INVALID ROW");
    std::map<std::string, uint32>::const_iterator res;
    res = m_baseReader->m_fieldMap.find(fieldname);
    if(res == m_baseReader->m_fieldMap.end())
        return("UNKNOWN COLUMN");
    return(GetString(res->second));
}

//not using _find to allow for more verbose error messages
const char *BaseRowsetReader::base_iterator::GetString(uint32 index) const {
    PyRep *row = m_baseReader->_getRow(m_index);
    if(row == NULL || !row->IsList())
        return("NON-LIST ROW");
    PyList *rowl = (PyList *) row;
    if(rowl->items.size() <= index)
        return("INVALID INDEX");
    PyRep *ele = rowl->items[index];
    if(!ele->IsString())
        return("NON-STRING ELEMENT");
    PyString *str = (PyString *) ele;
    return str->content().c_str();
}

uint32 BaseRowsetReader::base_iterator::GetInt(uint32 index) const {
    PyRep *ele = _find(index);
    if(ele == NULL || !ele->IsInt())
        return(0x7F000000LL);
    PyInt *e = (PyInt *) ele;
    return(e->value());
}

uint64 BaseRowsetReader::base_iterator::GetInt64(uint32 index) const {
    PyRep *ele = _find(index);
    if(ele == NULL || !ele->IsInt())
        return(0x7F0000007F000000LL);
    PyInt *e = (PyInt *) ele;
    return(e->value());
}

double BaseRowsetReader::base_iterator::GetReal(uint32 index) const {
    PyRep *ele = _find(index);
    if(ele == NULL || !ele->IsFloat())
        return(-9999999.0);
    PyFloat *e = (PyFloat *) ele;
    return(e->value());
}

bool BaseRowsetReader::base_iterator::IsNone(uint32 index) const {
    PyRep *ele = _find(index);
    return(ele != NULL && ele->IsNone());
}

PyRep::PyType BaseRowsetReader::base_iterator::GetType(uint32 index) const {
    PyRep *ele = _find(index);
    return(ele->GetType());
}

/*void RowsetReader::Dump(LogType type) {
    iterator cur, end;
    cur = begin();
    end = end();
    for(; cur != end; cur++) {
        _log(type, "
    }
}*/

std::string BaseRowsetReader::base_iterator::GetAsString(const char *fieldname) const {
    if(m_end)
        return("INVALID ROW");
    std::map<std::string, uint32>::const_iterator res;
    res = m_baseReader->m_fieldMap.find(fieldname);
    if(res == m_baseReader->m_fieldMap.end())
        return("UNKNOWN COLUMN");
    return(GetAsString(res->second));
}

std::string BaseRowsetReader::base_iterator::GetAsString(uint32 index) const {
    PyRep *row = m_baseReader->_getRow(m_index);
    if(row == NULL || !row->IsList())
        return("NON-LIST ROW");
    PyList *rowl = (PyList *) row;
    if(rowl->items.size() <= index)
        return("INVALID INDEX");
    PyRep *ele = rowl->items[index];
    if(ele->IsString()) {
        PyString *str = (PyString *) ele;
        return str->content();
    } else if(ele->IsInt()) {
        PyInt *i = (PyInt *) ele;
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", i->value());
        return(std::string(buf));
    } else if(ele->IsFloat()) {
        PyFloat *i = (PyFloat *) ele;
        char buf[64];
        snprintf(buf, sizeof(buf), "%f", i->value());
        return(std::string(buf));
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "(%s)", ele->TypeString());
        return(std::string(buf));
    }
}








RowsetReader::RowsetReader(const util_Rowset *rowset)
: BaseRowsetReader(),
  m_set(rowset)
{
    std::vector<std::string>::const_iterator cur, end;
    cur = rowset->header.begin();
    end = rowset->header.end();
    uint32 index;
    for(index = 0; cur != end; ++cur, ++index) {
        m_fieldMap[*cur] = index;
    }
}

RowsetReader::iterator RowsetReader::begin() {
    if(m_set->lines->empty())
        return end();
    return iterator(this, false);
}

size_t RowsetReader::size() const {
    return(m_set->lines->size());
}

bool RowsetReader::empty() const {
    return(m_set->lines->empty());
}

RowsetReader::iterator::iterator()
: base_iterator(NULL, true),
  m_parent(NULL)
{
}

RowsetReader::iterator::iterator(RowsetReader *parent, bool is_end)
: base_iterator(parent, is_end),
  m_parent(parent)
{
}

bool RowsetReader::iterator::operator==(const iterator &other) {
    if(m_parent != other.m_parent)
        return false;
    return(_baseEqual(other));
}

bool RowsetReader::iterator::operator!=(const iterator &other) {
    if(m_parent != other.m_parent)
        return true;
    return(!_baseEqual(other));
}

PyRep *RowsetReader::_getRow(uint32 index) const {
    if( m_set->lines->size() <= index )    //InvalidRow will be caught here!
        return NULL;
    return m_set->lines->GetItem( index );
}

size_t RowsetReader::ColumnCount() const {
    return(m_set->header.size());
}

const char *RowsetReader::ColumnName(uint32 index) const {
    if(index < ColumnCount())
        return(m_set->header[index].c_str());
    return("INVALID COLUMN INDEX");
}











TuplesetReader::TuplesetReader(const util_Tupleset *Tupleset)
: BaseRowsetReader(),
  m_set(Tupleset)
{
    std::vector<std::string>::const_iterator cur, end;
    cur = Tupleset->header.begin();
    end = Tupleset->header.end();
    uint32 index;
    for(index = 0; cur != end; cur++, index++) {
        m_fieldMap[*cur] = index;
    }
}

TuplesetReader::iterator TuplesetReader::begin() {
    if( m_set->lines->empty() )
        return end();
    return iterator( this, false );
}

size_t TuplesetReader::size() const {
    return m_set->lines->size();
}

bool TuplesetReader::empty() const {
    return m_set->lines->empty();
}

TuplesetReader::iterator::iterator()
: base_iterator(NULL, true),
  m_parent(NULL)
{
}

TuplesetReader::iterator::iterator(TuplesetReader *parent, bool is_end)
: base_iterator(parent, is_end),
  m_parent(parent)
{
}

bool TuplesetReader::iterator::operator==(const iterator &other) {
    if(m_parent != other.m_parent)
        return false;
    return(_baseEqual(other));
}

bool TuplesetReader::iterator::operator!=(const iterator &other) {
    if(m_parent != other.m_parent)
        return true;
    return(!_baseEqual(other));
}

PyRep *TuplesetReader::_getRow(uint32 index) const {
    if( m_set->lines->size() <= index )    //InvalidRow will be caught here!
        return NULL;
    return m_set->lines->GetItem( index );
}

size_t TuplesetReader::ColumnCount() const {
    return(m_set->header.size());
}

const char *TuplesetReader::ColumnName(uint32 index) const {
    if(index < ColumnCount())
        return(m_set->header[index].c_str());
    return("INVALID COLUMN INDEX");
}

SetSQLDumper::SetSQLDumper( FILE* f, const char* table )
: mFile( f ),
  mTable( table )
{
}

bool SetSQLDumper::VisitObject( const PyObject* rep )
{
    if( rep->type()->content() == "util.Rowset" )
    {
        //we found a friend, decode it
        //must be duplicated in order to be decoded ...
        PyObject* dup = new PyObject( *rep );

        util_Rowset rowset;
        if( !rowset.Decode( &dup ) )
        {
            codelog( COMMON__PYREP, "Unable to load a rowset from the object body!" );

            PyDecRef( dup );
        }
        else
        {
            PyDecRef( dup );

            RowsetReader reader( &rowset );
            if( !ReaderToSQL<RowsetReader>( mTable.c_str(), NULL, mFile, reader ) )
                codelog( COMMON__PYREP, "Failed to convert rowset to SQL." );
            else
                return true;
        }
    }

    //fallback
    return PyVisitor::VisitObject( rep );
}

bool SetSQLDumper::VisitTuple( const PyTuple* rep )
{
    //first we want to check to see if this could possibly even be a tupleset.
    if(    rep->size() == 2
        && rep->GetItem( 0 )->IsList()
        && rep->GetItem( 1 )->IsList() )
    {
        const PyList* possible_header = &rep->GetItem( 0 )->AsList();
        const PyList* possible_items = &rep->GetItem( 1 )->AsList();

        //check each element of the lists to make sure they line up.
        bool valid = true;
        PyList::const_iterator cur, end;

        cur = possible_header->begin();
        end = possible_header->end();
        for(; valid && cur != end; ++cur)
        {
            if( !(*cur)->IsString() )
                valid = false;
        }

        cur = possible_items->begin();
        end = possible_items->end();
        for(; valid && cur != end; ++cur)
        {
            if( !(*cur)->IsList() )
                valid = false;

            //it would be possible I guess to check each element of each item to make sure
            //it is a terminal type (non-container), but I dont care right now.
        }

        if( valid )
        {
            //ok, it looks like a tupleset... nothing we can do now but interpret it as one...
            //must be duplicated in order to be decoded ...
            PyTuple* dup = new PyTuple( *rep );

            util_Tupleset rowset;
            if( !rowset.Decode( &dup ) )
            {
                codelog( COMMON__PYREP, "Unable to interpret tuple as a tupleset, it may not even be one." );

                PyDecRef( dup );
            }
            else
            {
                PyDecRef( dup );

                TuplesetReader reader( &rowset );
                if( !ReaderToSQL<TuplesetReader>( mTable.c_str(), NULL, mFile, reader ) )
                    codelog( COMMON__PYREP, "Failed to convert tupleset to SQL." );
                else
                    return true;
            }
        }
    }

    //fallback
    return PyVisitor::VisitTuple( rep );
}















