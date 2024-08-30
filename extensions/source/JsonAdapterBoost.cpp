/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#include "teascript/ext/JsonAdapterBoost.hpp"

#include <sstream>

#if __has_include( <boost/json/src.hpp> )
# include <boost/json/src.hpp>
#else
# error You must add the include directory for boost to your project.
#endif


namespace teascript {

ValueObject JsonAdapterBoost::ReadJsonString( Context &rContext, std::string const &rJsonStr )
{
    boost::json::value json;
    try {
        std::istringstream is( rJsonStr );
        json = boost::json::parse( is );
    } catch( std::exception const & ) {
        // TODO better error handling! Use Error once it exist!
        return ValueObject( TypeNaV, false ); // false and null (NaV) is a valid return value, so we use TypeInfo for NaV to indicate error!
    }
    return ToValueObject( rContext, json );
}

ValueObject JsonAdapterBoost::WriteJsonString( ValueObject const &rObj )
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

ValueObject JsonAdapterBoost::ToValueObject( Context &rContext, JsonType const &json )
{
    auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
    switch( json.kind() ) {
        case boost::json::kind::object:
            {
                Tuple tup;
                tup.Reserve( json.get_object().size() );
                for( auto const &[k, v] : json.get_object() ) {
                    auto value = ToValueObject( rContext, v );
                    tup.AppendKeyValue( k, value );
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case boost::json::kind::array:
            {
                Tuple tup;
                tup.Reserve( json.get_array().size() );
                if( json.get_array().empty() ) {
                    // special case: empty array!
                    // An empty Tuple cannot be distinguished whether it belongs to an empty object or empty array!
                    // for that reason we insert an empty Buffer, which is not a valid value for Json.
                    // If the array is filled later, the empty buffer must be removed for show correct size.
                    tup.AppendValue( ValueObject( Buffer(), cfg ) );
                } else for( auto const &v : json.get_array() ) {
                    auto value = ToValueObject( rContext, v );
                    tup.AppendValue( value );
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case boost::json::kind::string:
            return ValueObject( json.get_string().subview(), cfg);
        case boost::json::kind::double_:
            return ValueObject( json.get_double(), cfg);
        case boost::json::kind::uint64:
        {
            // Json cannot distinguish between different integer types. So, we use I64 as default, except the value is too big.
            auto val = static_cast<unsigned long long>(json.get_uint64());
            if( val > static_cast<unsigned long long>(std::numeric_limits<long long>::max()) ) {
                return ValueObject( val, cfg );
            }
            return ValueObject( static_cast<long long>(val), cfg );
        }
        case boost::json::kind::int64:
            return ValueObject( static_cast<long long>(json.get_int64()), cfg);
        case boost::json::kind::bool_:
            return ValueObject( json.get_bool(), cfg);
        case boost::json::kind::null:
            return ValueObject( NotAValue{}, cfg );
        default:
            throw std::runtime_error( "Unknown boost::json::kind" ); // there is none.
    }
}

void JsonAdapterBoost::FromValueObject( ValueObject const &rObj, JsonType &rOut )
{
    rOut = boost::json::value();
    switch( rObj.InternalType() ) {
        case ValueObject::TypeTuple:
           // first check whether it is an object or an array.
            if( Tuple const &tup = rObj.GetValue<Tuple>(); tuple::TomlJsonUtil::IsTupAnArray( tup ) ) {
                rOut.emplace_array();
                if( tuple::TomlJsonUtil::IsArrayEmpty( tup ) ) {
                    ; /* nop */
                } else for( auto const &[_, v] : tup ) {
                    boost::json::value value;
                    FromValueObject( v, value );
                    rOut.get_array().emplace_back( std::move( value ) );
                }
            } else {
                rOut.emplace_object();
                for( auto const &[k, v] : tup ) {
                    boost::json::value value;
                    FromValueObject( v, value );
                    rOut.get_object().emplace( k, std::move( value ) );
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
            throw exception::runtime_error( "usupported type for json!" );
    }
}

} // namespace teascript
