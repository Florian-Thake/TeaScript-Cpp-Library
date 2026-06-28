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
        //Needed??  //key.MakeShared();
        auto val = tuple::deep_copy( kv.second, keep_const );
        val.MakeShared(); // TODO: change to shared config!
        rDest.emplace( std::move( key ), std::move( val ) );
    }
}

} // namespace {

} // namespace map

} // namespace teascript
