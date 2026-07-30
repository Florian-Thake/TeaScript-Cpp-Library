/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ValueObject.hpp"
#include "Type.hpp"
#include "SourceLocation.hpp"

namespace teascript {

//in ValueObject.hpp\\//using Map = std::map<ValueObject, ValueObject>;

namespace map {

inline
TypeInfo const & get_type_info() noexcept
{
    static TypeInfo const TypeMap = MakeTypeInfo<Map>( "Map" );
    return TypeMap;
}

namespace {

void deep_copy( Map &rDest, Map const &rSrc, bool const keep_const = false )
{
    for( auto const &kv : rSrc ) {
        auto key = tuple::deep_copy( kv.first, keep_const );
        key.MakeShared(); // TODO: change to shared config!
        auto val = tuple::deep_copy( kv.second, keep_const );
        val.MakeShared(); // TODO: change to shared config!
        rDest.emplace( std::move( key ), std::move( val ) );
    }
}

// inline for removing the -Wunused-function warning in some TUs
inline
Tuple create_iterator( Map const &rMap )
{
    Tuple the_iterator;
    Tuple keys;
    for( auto const &[key, _] : rMap ) {
        keys.AppendValue( key ); // FIXME: Must create a deep copy here?
    }
    if( not rMap.empty() ) {
        the_iterator.AppendKeyValue( "key", ValueObject( keys[0] )); // this will be a shared ValueObject.
    }
    the_iterator.AppendKeyValue( "idx", ValueObject( 0LL, ValueConfig{ValueShared,ValueMutable} ) );
    the_iterator.AppendKeyValue( "_all_keys", ValueObject( std::move( keys ), ValueConfig{ValueShared,ValueConst} ) );
    return the_iterator;
}

// inline for removing the -Wunused-function warning in some TUs
inline
bool advance_iterator( ValueObject &rObj )
{
    Tuple &the_iterator = rObj.GetMutableValue<Tuple>();

    Tuple const &all_keys = the_iterator.GetValueByKey( "_all_keys" ).GetConstValue<Tuple>();
    ValueObject &key_obj = the_iterator.GetValueByKey( "key" );
    ValueObject &idx_obj = the_iterator.GetValueByKey( "idx" );
    auto const idx = idx_obj.GetValueCopy<Integer>();
    if( idx < 0LL ) {
        throw exception::bad_value_cast( "map iterator index is invalid!" );
    }
    auto const idx_new = idx + 1LL;
    if( all_keys.Size() <= static_cast<size_t>( idx_new ) ) {
        return false;
    }

    // update idx
    idx_obj.AssignValue( idx_new );

    // and finally the key.
    key_obj = ValueObject( all_keys[static_cast<size_t>(idx_new)] ); // this will be a shared ValueObject.

    return true;

}

} // namespace {

} // namespace map

} // namespace teascript
