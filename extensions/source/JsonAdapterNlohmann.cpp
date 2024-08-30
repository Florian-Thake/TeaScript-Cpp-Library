/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#include "teascript/ext/JsonAdapterNlohmann.hpp"

#include <sstream>

#if __has_include( "nlohmann/json.hpp" )
# include "nlohmann/json.hpp"
#else
# error You must add the include directory for nlohmann json to your project.
#endif

namespace teascript {

ValueObject JsonAdapterNlohmann::ReadJsonString( Context &rContext, std::string const &rJsonStr )
{
    nlohmann::json json;
    try {
        json = nlohmann::json::parse( rJsonStr );
    } catch( std::exception const & ) {
        // TODO better error handling! Use Error once it exist!
        return ValueObject( TypeNaV, false ); // false and null (NaV) is a valid return value, so we use TypeInfo for NaV to indicate error!
    }
    return ToValueObject( rContext, json );
}

ValueObject JsonAdapterNlohmann::WriteJsonString( ValueObject const &rObj )
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

ValueObject JsonAdapterNlohmann::ToValueObject( Context & rContext, JsonType const &json )
{
    auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
    switch( json.type() ) {
        case nlohmann::json::value_t::object:
            {
                Tuple tup;
                tup.Reserve( json.size() );
                for( auto const &[k, v] : json.items() ) {
                    auto value = ToValueObject( rContext, v );
                    if( not value.GetTypeInfo()->IsSame<TypeInfo>() ) { // skip inner unsupported types like 'discarded'
                        tup.AppendKeyValue( k, value );
                    }
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case nlohmann::json::value_t::array:
            {
                Tuple tup;
                tup.Reserve( json.size() );
                if( json.empty() ) {
                    // special case: empty array!
                    // An empty Tuple cannot be distinguished whether it belongs to an empty object or empty array!
                    // for that reason we insert an empty Buffer, which is not a valid value for Json.
                    // If the array is filled later, the empty buffer must be removed for show correct size.
                    tup.AppendValue( ValueObject( Buffer(), cfg ) );
                } else for( auto const &v : json ) {
                    auto value = ToValueObject( rContext, v );
                    if( not value.GetTypeInfo()->IsSame<TypeInfo>() ) { // skip inner unsupported types like 'discarded'
                        tup.AppendValue( value );
                    }
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case nlohmann::json::value_t::string:
            return ValueObject( json.get<std::string>(), cfg );
        case nlohmann::json::value_t::number_float:
            return ValueObject( json.get<double>(), cfg );
        case nlohmann::json::value_t::number_unsigned:
            {
                // Json cannot distinguish between different integer types. So, we use I64 as default, except the value is too big.
                auto val = json.get<unsigned long long>();
                if( val > static_cast<unsigned long long>(std::numeric_limits<long long>::max()) ) {
                    return ValueObject( val, cfg );
                }
                return ValueObject( static_cast<long long>(val), cfg);
            }
        case nlohmann::json::value_t::number_integer:
            return ValueObject( json.get<long long>(), cfg );
        case nlohmann::json::value_t::boolean:
            return ValueObject( json.get<bool>(), cfg );
        case nlohmann::json::value_t::null:
            return ValueObject( NotAValue{}, cfg );
        case nlohmann::json::value_t::binary:
            /* not supported */
            break;
        case nlohmann::json::value_t::discarded:
            break;
        default:
            throw std::runtime_error( "Unknown nlohmann::json::value_t" );   
    }
    // TODO better error handling! Use Error once it exist!
    return ValueObject( TypeNaV, false ); // false and null (NaV) is a valid return value, so we use TypeInfo for NaV to indicate error!
}

void JsonAdapterNlohmann::FromValueObject( ValueObject const &rObj, JsonType & rOut )
{
    rOut = nlohmann::json();
    switch( rObj.InternalType() ) {
        case ValueObject::TypeTuple:
           // first check whether it is an object or an array.
            if( Tuple const &tup = rObj.GetValue<Tuple>(); tuple::TomlJsonUtil::IsTupAnArray( tup ) ) {
                if( tuple::TomlJsonUtil::IsArrayEmpty( tup ) ) {
                    rOut = nlohmann::json( nlohmann::json::value_t::array );
                } else for( auto const &[_, v] : tup ) {
                    nlohmann::json value;
                    FromValueObject( v, value );
                    rOut.emplace_back( std::move( value ) );
                }
            } else {
                rOut = nlohmann::json( nlohmann::json::value_t::object );
                for( auto const &[k, v] : tup ) {
                    nlohmann::json value;
                    FromValueObject( v, value );
                    rOut.emplace( k, std::move( value ) );
                }
            }
            break;
        case ValueObject::TypeString:
            rOut = rObj.GetValue<std::string>();
            break;
        case ValueObject::TypeF64:
            rOut = rObj.GetValue<F64>();
            break;
        case ValueObject::TypeU64:
            rOut = rObj.GetValue<U64>();
            break;
        case ValueObject::TypeI64:
            rOut = rObj.GetValue<I64>();
            break;
        case ValueObject::TypeU8:
            rOut = static_cast<U64>(rObj.GetValue<U8>());
            break;
        case ValueObject::TypeBool:
            rOut = rObj.GetValue<Bool>();
            break;
        case ValueObject::TypeNaV:
            break; // null is the correct result.
        default:
            throw exception::runtime_error( "unsupported type for json!" );
    }
}

} // namespace teascript
