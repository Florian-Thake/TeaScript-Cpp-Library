/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ValueObject.hpp"
#include "Context.hpp"

#include <optional>
#include <sstream>


#ifndef PICOJSON_USE_LOCALE
// we dont want locale stuff by default
# define PICOJSON_USE_LOCALE  0
#endif
// we need int64_t support
#define PICOJSON_USE_INT64    1

#include "thirdparty/picojson.h"


namespace teascript {

class JsonAdapterPico
{
private:
    static void DispatchKeyValue( Context &rContext, Tuple &rParent, std::optional< std::string > const &key, picojson::value const &value )
    {
        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};

        if( value.is<picojson::object>() ) {
            auto const &o = value.get<picojson::object>();

            Tuple object;
            DispatchObject( rContext, o, object );
            if( not key ) {
                rParent.AppendValue( ValueObject( std::move(object), cfg));
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(std::move(object), cfg));
            }
        } else if( value.is<picojson::array>() ) {
            auto const &a = value.get<picojson::array>();

            Tuple arr;
            DispatchArray( rContext, a, arr );
            if( not key ) {
                rParent.AppendValue( ValueObject( std::move( arr ), cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(std::move(arr), cfg));
            }
        } else if( value.is<std::string>() ) {
            auto const &s = value.get<std::string>();
            if( not key ) {
                rParent.AppendValue( ValueObject( s, cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(s, cfg));
            }
        } else if( value.is<int64_t>() ) { // NOTE: must first test for int64_t b/c double will be true for int64_t as well!
            I64 const i = value.get<int64_t>();
            if( not key ) {
                rParent.AppendValue( ValueObject( i, cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(i, cfg));
            }
        } else if( value.is<double>() ) {
            F64 const f = value.get<double>();
            if( not key ) {
                rParent.AppendValue( ValueObject( f, cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(f, cfg));
            }
        } else if( value.is<bool>() ) {
            Bool const b = value.get<bool>();
            if( not key ) {
                rParent.AppendValue( ValueObject( b, cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(b, cfg));
            }
        } else if( value.is<picojson::null>() ) { // we use NaV as null here.
            if( not key ) {
                rParent.AppendValue( ValueObject( NotAValue{}, cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(NotAValue{}, cfg));
            }
        }
    }

    static void DispatchArray( Context &rContext, picojson::array const &arr, Tuple &rParent )
    {
        // special case: empty array!
        // An empty Tuple cannot be distinguished whether it belongs to an empty object or empty array!
        // for that reason we insert an empty Buffer, which is not a valid value for Json.
        // If the array is filled later, the empty buffer must be removed for show correct size.
        if( arr.empty() ) {
            auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
            rParent.AppendValue( ValueObject( Buffer(), cfg ) );
        } else for( auto const &v : arr ) {
            DispatchKeyValue( rContext, rParent, std::nullopt, v );
        }
    }

    static void DispatchObject( Context &rContext, picojson::object const &obj, Tuple &rParent )
    {
        for( auto const &[key, value] : obj ) {
            DispatchKeyValue( rContext, rParent, key, value );
        }
    }

public:
    using JsonType = picojson::value;

    static constexpr char Name[] = "PicoJson";

    /// Constructs a ValueObject from the given Json formatted string.
    static ValueObject ReadJsonString( Context &rContext, std::string const &rJsonStr )
    {
        picojson::value  json;
        auto const errorstr = picojson::parse( json, rJsonStr );
        if( not errorstr.empty() ) {
            // TODO better error handling! Use Error once it exist!
            return ValueObject( TypeNaV, false ); // false and null (NaV) is a valid return value, so we use TypeInfo for NaV to indicate error!
        }

        return ToValueObject( rContext, json );
    }

    /// Constructs a Json formatted string from the given ValueObject. 
    /// \return the constructed string or false on error.
    /// \note the object must only contain supported types and layout for Json.
    static ValueObject WriteJsonString( ValueObject const &rObj )
    {
        JsonType json;
        try {
            FromValueObject( rObj, json );
        } catch( std::exception const & ) {
            // TODO better error handling! Use Error once it exist!
            return ValueObject( false );
        }
        std::ostringstream  os;
        os << json;
        return ValueObject( std::move(os).str() );
    }

    static ValueObject ToValueObject( Context &rContext, JsonType const &json )
    {
        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};

        // according to Json spec the root value can be any valid value from null to object.
        if( json.is<picojson::object>() ) {
            Tuple  res;
            DispatchObject( rContext, json.get<picojson::object>(), res );
            return ValueObject( std::move( res ), cfg );
        } else if( json.is<picojson::array>() ) {
            Tuple  res;
            DispatchArray( rContext, json.get<picojson::array>(), res );
            return ValueObject( std::move( res ), cfg );
        } else if( json.is<std::string>() ) {
            return ValueObject( json.get<std::string>(), cfg );
        } else if( json.is<int64_t>() ) { // NOTE: must first test for int64_t b/c double will be true for int64_t as well!
            return ValueObject( static_cast<I64>(json.get<int64_t>()), cfg );
        } else if( json.is<double>() ) {
            return ValueObject( json.get<double>(), cfg );
        } else if( json.is<bool>() ) {
            return ValueObject( json.get<bool>(), cfg );
        } else if( json.is<picojson::null>() ) {
            return ValueObject( NotAValue{}, cfg );
        }

        // TODO better error handling! Use Error once it exist!
        return ValueObject( TypeNaV, false ); // false and null (NaV) is a valid return value, so we use TypeInfo for NaV to indicate error!       
    }

    static void FromValueObject( ValueObject const &rObj, JsonType &rOut )
    {
        rOut = picojson::value();
        switch( rObj.InternalType() ) {
        case ValueObject::TypeTuple:
            // first check whether it is an object or an array.
            if( Tuple const &tup = rObj.GetValue<Tuple>(); tuple::TomlJsonUtil::IsTupAnArray( tup ) ) {
                picojson::array arr;
                if( not tuple::TomlJsonUtil::IsArrayEmpty( tup ) ) {
                    arr.reserve( tup.Size() );
                    for( auto const &[_, v] : tup ) {
                        picojson::value  value;
                        FromValueObject( v, value );
                        arr.push_back( std::move(value) );
                    }
                }
                rOut.set<picojson::array>( std::move( arr ) );
            } else {
                picojson::object obj;
                for( auto const &[k, v] : tup ) {
                    picojson::value  value;
                    FromValueObject( v, value );
                    obj.emplace( k, std::move(value) );
                }
                rOut.set<picojson::object>( std::move( obj ) );
            }
            break;
        case ValueObject::TypeString:
            rOut.set<std::string>( rObj.GetValue<std::string>() );
            break;
        case ValueObject::TypeF64:
            rOut.set<double>( rObj.GetValue<double>() );
            break;
        case ValueObject::TypeU64:
            if( rObj.GetValue<U64>() > static_cast<U64>(std::numeric_limits<int64_t>::max()) ) {
                throw exception::out_of_range( "value is too big for int64_t" );
            } else {
                // we need this local for resolve linker issues
                auto const i = static_cast<int64_t>(rObj.GetValue<U64>());
                rOut.set<int64_t>( i );
            }
            break;
        case ValueObject::TypeI64:
            {
                // we need this local for resolve linker issues
                auto const i = static_cast<int64_t>(rObj.GetValue<I64>());
                rOut.set<int64_t>( i );
            }
            break;
        case ValueObject::TypeU8:
            {
                // we need this local for resolve linker issues
                auto const i = static_cast<int64_t>(rObj.GetValue<U8>());
                rOut.set<int64_t>( i );
            }
            break;
        case ValueObject::TypeBool:
            rOut.set<bool>( rObj.GetValue<bool>() );
            break;
        case ValueObject::TypeNaV:
            break; // null is the correct result.
        default:
            throw exception::runtime_error( "unsupported type for json!" );
        }
    }

};

} // namespace teascript

