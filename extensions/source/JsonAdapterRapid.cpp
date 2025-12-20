/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#include "teascript/ext/JsonAdapterRapid.hpp"

#include <sstream>

#if __has_include( "rapidjson/document.h" )
# include "rapidjson/document.h"
# include "rapidjson/writer.h"
# include "rapidjson/istreamwrapper.h"
# include "rapidjson/ostreamwrapper.h"
#else
# error You must add the include directory for RapidJson to your project.
#endif


namespace teascript {

ValueObject JsonAdapterRapid::ReadJsonString( Context &rContext, std::string const &rJsonStr )
{
    rapidjson::Document json;
    try {
        std::istringstream is( rJsonStr );
        rapidjson::IStreamWrapper isw( is );
        json.ParseStream( isw );
    } catch( std::exception const & ex ) {
        return ValueObject( Error::MakeRuntimeError( std::string( "Error reading JSON String: " ) + ex.what() ), ValueConfig( ValueUnshared, ValueMutable ) );
    }
    return ToValueObject( rContext, json );
}

ValueObject JsonAdapterRapid::WriteJsonString( ValueObject const &rObj )
{
    JsonType json;
    try {
        FromValueObject( rObj, json );
    } catch( std::exception const & ex ) {
        return ValueObject( Error::MakeRuntimeError( std::string( "Error writing JSON String: " ) + ex.what() ), ValueConfig( ValueUnshared, ValueMutable ) );
    }
    std::ostringstream  os;
    rapidjson::OStreamWrapper osw( os );
    rapidjson::Writer<rapidjson::OStreamWrapper> writer( osw );
    json.Accept( writer );

    return ValueObject( std::move(os).str() );
}

ValueObject JsonAdapterRapid::ToValueObject( Context &rContext, JsonValue const &json )
{
    auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
    switch( json.GetType() ) {
        case rapidjson::kObjectType:
            {
                Tuple tup;
                tup.Reserve( json.MemberCount() );
                for( auto const &[k, v] : json.GetObject() ) {
                    auto value = ToValueObject( rContext, v );
                    if( not value.GetTypeInfo()->IsSame<TypeInfo>() ) { // skip inner unsupported types like 'discarded'
                        tup.AppendKeyValue( k.GetString(), value);
                    }
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case rapidjson::kArrayType:
            {
                Tuple tup;
                tup.Reserve( json.Size() );
                if( json.Empty() ) {
                    // special case: empty array!
                    // An empty Tuple cannot be distinguished whether it belongs to an empty object or empty array!
                    // for that reason we insert an empty Buffer, which is not a valid value for Json.
                    // If the array is filled later, the empty buffer must be removed for show correct size.
                    tup.AppendValue( ValueObject( Buffer(), cfg ) );
                } else for( auto const &v : json.GetArray() ) {
                    auto value = ToValueObject( rContext, v );
                    if( not value.GetTypeInfo()->IsSame<TypeInfo>() ) { // skip inner unsupported types like 'discarded'
                        tup.AppendValue( value );
                    }
                }
                return ValueObject( std::move( tup ), cfg );
            }
        case rapidjson::kStringType:
            return ValueObject( std::string(json.GetString(), json.GetStringLength()), cfg);
        case rapidjson::kNumberType:
            // Json cannot distinguish between different integer types. So, we use I64 as default, except the value is too big.
            if( json.Is<uint64_t>() ) {
                auto val = json.Get<uint64_t>();
                if( val > static_cast<uint64_t>(std::numeric_limits<I64>::max()) ) {
                    return ValueObject( static_cast<U64>(val), cfg);
                }
                return ValueObject( static_cast<I64>(val), cfg );
            } else if( json.Is<int64_t>() ) {
                return ValueObject( static_cast<I64>(json.Get<int64_t>()), cfg );
            } else if( json.Is<unsigned int>() ) {
                return ValueObject( static_cast<I64>(json.Get<unsigned int>()), cfg );
            } else if( json.Is<int>() ) {
                return ValueObject( static_cast<I64>(json.Get<int>()), cfg );
            }
            return ValueObject( json.GetDouble(), cfg);
        case rapidjson::kFalseType:
        case rapidjson::kTrueType:
            return ValueObject( json.GetBool(), cfg);
        case rapidjson::kNullType:
            return ValueObject( NotAValue{}, cfg );
        default:
            throw std::runtime_error( "Unknown rapidjson::Type" );
    }
}

void JsonAdapterRapid::FromValueObject( ValueObject const &rObj, JsonType &rOut )
{
    rOut = rapidjson::Document();
    FromValueObject( rObj, rOut, rOut );
}

void JsonAdapterRapid::FromValueObject( ValueObject const &rObj, JsonType &rDoc, JsonValue &rOut )
{
    switch( rObj.InternalType() ) {
        case ValueObject::TypeTuple:
           // first check whether it is an object or an array.
            if( Tuple const &tup = rObj.GetValue<Tuple>(); tuple::TomlJsonUtil::IsTupAnArray( tup ) ) {
                rOut.SetArray();
                if( tuple::TomlJsonUtil::IsArrayEmpty( tup ) ) {
                    ; /* nop */
                } else for( auto const &[_, v] : tup ) {
                    rapidjson::Document::ValueType value;
                    FromValueObject( v, rDoc, value );
                    rOut.PushBack( std::move( value ), rDoc.GetAllocator() );
                }
            } else {
                rOut.SetObject();
                for( auto const &[k, v] : tup ) {
                    JsonValue value;
                    FromValueObject( v, rDoc, value );
                    JsonValue name; 
                    name.SetString( k.c_str(), rDoc.GetAllocator() );
                    rOut.AddMember( name, std::move(value), rDoc.GetAllocator());
                }
            }
            break;
        case ValueObject::TypeString:
            rOut.SetString( rObj.GetValue<std::string>().c_str(), rDoc.GetAllocator() );
            break;
        case ValueObject::TypeF64:
            rOut = rObj.GetValue<F64>();
            break;
        case ValueObject::TypeU64:
            rOut = static_cast<uint64_t>(rObj.GetValue<U64>());
            break;
        case ValueObject::TypeI64:
            rOut = static_cast<int64_t>(rObj.GetValue<I64>());
            break;
        case ValueObject::TypeU8:
            rOut = static_cast<uint64_t>(rObj.GetValue<U8>());
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
