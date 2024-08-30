/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Collection.hpp"
#include "ValueObject.hpp"
#include "Type.hpp"
#include "SourceLocation.hpp"

namespace teascript {

//in ValueObject.hpp\\//using Tuple = Collection<ValueObject>;

namespace tuple {

// using ForEachElementFunctor = std::function< bool( ValueObject &, int )>;

/// applies recursively all values to given functor for all elements or stops
/// further recursive calls if functor returns false.
/// Functor must have signature: bool ( ValueObject &, int )
template< typename F>
void foreach_element( ValueObject &rVal, bool inclusive, F && f, int level = 0 )
{
    if( inclusive ) {
        if( !f( rVal, level ) ) {
            return;
        }
    }
    if( rVal.GetTypeInfo()->IsSame<Tuple>() ) {
        Tuple &tuple = rVal.GetValue< Tuple >();
        for( auto &kv : tuple ) {
            foreach_element( kv.second, true, f, level + 1 );
        }
    }
}

// using ForEachNamedElementFunctor = std::function< bool( std::string const &, ValueObject &, int )>;

/// applies recursively all values with its full name to given functor for all elements or stops
/// further recursive calls if functor returns false.
/// Functor must have signature: bool ( std::string const &, ValueObject &, int )
template< typename F>
void foreach_named_element( std::string const &fullname, ValueObject &rVal, bool inclusive, F && f, int level = 0 )
{
    if( inclusive ) {
        if( !f( fullname, rVal, level ) ) {
            return;
        }
    }
    if( rVal.GetTypeInfo()->IsSame<Tuple>() ) {
        Tuple &tuple = rVal.GetValue< Tuple >();
        size_t idx = 0;
        for( auto &kv : tuple ) {
            std::string const name = kv.first.empty() ?
                tuple.IndexOfKey( kv.first ) == static_cast<size_t>(-1) ? std::to_string( idx ) : "\"\""
                : kv.first;
            foreach_named_element( fullname + "." + name, kv.second, true, f, level + 1 );
            ++idx;
        }
    }
}

/// utility function collection for ease life with Tuple for/from Toml or Json.
class TomlJsonUtil
{
public:
    static bool IsAnArray( ValueObject const &rObj )
    {
        if( rObj.InternalType() != ValueObject::TypeTuple ) {
            return false;
        }
        return IsTupAnArray( rObj.GetValue<Tuple>() );
    }

    static bool IsTupAnArray( Tuple const &rTuple )
    {
        if( rTuple.IsEmpty() ) {
            return false; // empty tuples are always interpreted as josn objects/toml tables.
        }
        auto const &[key, _] = *rTuple.begin();
        // For Toml and Json an empty string "" is allowed as key, so we must check that.
        if( not key.empty() || rTuple.IndexOfKey( key ) != Tuple::npos ) {
            return false; // only tables can have keys
        }

        return true;
    }

    static bool IsArrayEmpty( Tuple const &rTuple )
    {
        if( not IsTupAnArray( rTuple ) ) {
            return false;
        }
        // here we know it is not empty.
        auto const &[_, val] = *rTuple.begin();

        // special case: For distinguish an empty Tuple whether it is an array or table,
        // we mark empty arrays with an empty Buffer.
        if( rTuple.Size() == 1 && val.InternalType() == ValueObject::TypeBuffer && val.GetValue<Buffer>().empty() ) {
            return true;
        }

        return false;
    }

    static void ArrayAppend( Tuple &rTuple, ValueObject &rVal )
    {
        // special case: For distinguish an empty Tuple whether it is an array or table,
        // we mark empty arrays with an empty Buffer.
        // This marker must be removed when the first element is appended.
        if( IsArrayEmpty( rTuple ) ) {
            rTuple.Clear(); // remove the buffer marker.
        }
        rTuple.AppendValue( rVal.MakeShared() );
    }

    static ValueObject ArrayInsert( Tuple &rTuple, long long const idx, ValueObject &rVal )
    {
        // special case: For distinguish an empty Tuple whether it is an array or table,
        // we mark empty arrays with an empty Buffer.
        // This marker must be removed when the first element is appended.
        if( IsArrayEmpty( rTuple ) ) {
            if( 0 == idx ) {
                rTuple.Clear(); // remove the buffer marker.
            } else {
                return ValueObject( false ); // TODO: return Error once it exists.
            }
        }
        try {
            rTuple.InsertValue( static_cast<std::size_t>(idx), rVal.MakeShared() );
        } catch( exception::out_of_range const & ) {
            return ValueObject( false ); // TODO: return Error once it exists.
        }
        return ValueObject( true );
    }

    static ValueObject ArrayRemove( Tuple &rTuple, long long const idx )
    {
        if( not IsTupAnArray( rTuple ) || IsArrayEmpty( rTuple ) ) {
            return ValueObject( false ); // TODO: return Error once it exists.
        }
        if( not rTuple.RemoveValueByIdx( static_cast<std::size_t>(idx) ) ) {
            return ValueObject( false ); // TODO: return Error once it exists.
        }
        // special case: For distinguish an empty Tuple whether it is an array or table,
        // we mark empty arrays with an empty Buffer.
        if( rTuple.IsEmpty() ) {
            auto const cfg = ValueConfig{ValueShared,ValueMutable,&TypeBuffer};
            rTuple.AppendValue( ValueObject( Buffer(), cfg ) );
        }
        return ValueObject( true );
    }
};

// needed for linking
namespace {

[[maybe_unused]]
bool is_same_structure( Tuple const &rTup1, Tuple const &rTup2 ) noexcept
{
    if( rTup1.Size() != rTup2.Size() ) {
        return false;
    }

    auto it1 = rTup1.begin();
    auto it2 = rTup2.begin();
    for( ; it1 != rTup1.end(); ++it1, ++it2 ) {
        if( it1->first != it2->first ) {
            return false;
        }
        if( not it1->second.GetTypeInfo()->IsSame( *it2->second.GetTypeInfo() ) ) {
            return false;
        }
        if( it1->second.GetTypeInfo()->IsSame<Tuple>() ) {
            if( not is_same_structure( it1->second.GetValue<Tuple>(), it2->second.GetValue<Tuple>() ) ) {
                return false;
            }
        }
    }
    return true;
}

std::strong_ordering compare_values( Tuple const &rTup1, Tuple const &rTup2 )
{
    if( rTup1.Size() != rTup2.Size() ) {
        return rTup1.Size() <=> rTup2.Size();
    }

    auto it1 = rTup1.begin();
    auto it2 = rTup2.begin();
    for( ; it1 != rTup1.end(); ++it1, ++it2 ) {
        // must be same names.
        if( it1->first != it2->first ) {
#if !_LIBCPP_VERSION // libc++14 fails here
            return it1->first <=> it2->first;
#else
            auto const res = it1->first.compare( it2->first );
            return res < 0 ? std::strong_ordering::less : res > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
        }
        // ... and same types
        if( not it1->second.GetTypeInfo()->IsSame( *it2->second.GetTypeInfo() ) ) {
#if !_LIBCPP_VERSION // libc++14 fails here
            return it1->second.GetTypeInfo()->ToTypeIndex() <=> it2->second.GetTypeInfo()->ToTypeIndex();
#else
            auto const i1 = it1->second.GetTypeInfo()->ToTypeIndex();
            auto const i2 = it2->second.GetTypeInfo()->ToTypeIndex();
            return i1 < i2 ? std::strong_ordering::less : i2 < i1 ? std::strong_ordering::greater : std::strong_ordering::equal;
#endif
        }
        // ... and same values
        auto const comp = it1->second <=> it2->second; // if both are tuples we will land in this function again...
        if( comp != std::strong_ordering::equal && comp != std::strong_ordering::equivalent ) {
            return comp;
        }
    }
    return std::strong_ordering::equal;
}


void deep_copy( Tuple &rDest, Tuple const &rSrc, bool const keep_const = false )
{
    if( rSrc.Size() > 0 ) {
        if( rSrc.Size() > 1 ) {
            rDest.Reserve( rSrc.Size() );
        }
        for( auto const &kv : rSrc ) {
            auto val = kv.second;
            if( val.GetTypeInfo()->IsSame<Tuple>() ) {
                Tuple  tuple;
                auto  const  cfg = ValueConfig{ValueShared, val.IsConst() && keep_const ? ValueConst : ValueMutable, val.GetTypeInfo()};
                deep_copy( tuple, kv.second.GetValue<Tuple>(), keep_const );
                val = ValueObject( std::move( tuple ), cfg );
            } else {
                val.Detach( keep_const ); // detach for have a distinct copy.
                val.MakeShared();         // TODO: make this optional later!
            }
            if( not kv.first.empty() ) {
                rDest.AppendKeyValue( kv.first, val );
            } else {
                // NOTE: check if empty key "" is in dictionary. if so, do same in the copy. FIXME: get rid of this lookup!!
                if( rSrc.IndexOfKey( kv.first ) != static_cast<std::size_t>(-1) ) {
                    rDest.AppendKeyValue( kv.first, val );
                } else {
                    rDest.AppendValue( val );
                }
            }
        }
    }
}

ValueObject deep_copy( ValueObject const &rVal, bool const keep_const = false )
{
    if( not rVal.GetTypeInfo()->IsSame<Tuple>() ) {
        auto copy( rVal );
        copy.Detach( keep_const );
        return copy;
    }

    Tuple  tuple;
    deep_copy( tuple, rVal.GetValue< Tuple >(), true ); // constness of each element will be kept.
    auto  const  cfg = ValueConfig{ValueShared, rVal.IsConst() && keep_const ? ValueConst : ValueMutable, rVal.GetTypeInfo()};
    return ValueObject( std::move( tuple ), cfg );
}

} // namespace anonymous 

} // namespace tuple

} // namespace teascript

inline
std::strong_ordering operator<=>( teascript::Tuple const &lhs, teascript::Tuple const &rhs )
{
    return teascript::tuple::compare_values( lhs, rhs );
}

/// Returns whether the 2 Tuples are equal.
inline bool operator==( teascript::Tuple const &lhs, teascript::Tuple const &rhs )
{
    auto const res = lhs <=> rhs;
    return res == std::strong_ordering::equal || res == std::strong_ordering::equivalent;
}

/// Returns whether the 2 Tuples are unequal.
inline bool operator!=( teascript::Tuple const &lhs, teascript::Tuple const &rhs )
{
    return !(lhs == rhs);
}
