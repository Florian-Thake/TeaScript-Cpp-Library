/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


// Define this if toml++ is present in your include pathes but you want to disable the support in TeaScript.
//#define TEASCRIPT_DISABLE_TOMLSUPPORT    1


// https://github.com/marzer/tomlplusplus
// toml.hpp file exists since 3.4 (before it was toml.h)
#if __has_include( "toml++/toml.hpp" ) && !defined( TEASCRIPT_DISABLE_TOMLSUPPORT )
# define TEASCRIPT_TOMLSUPPORT  1
# include "toml++/toml.hpp"
#else
# define TEASCRIPT_TOMLSUPPORT  0
#endif


#if TEASCRIPT_TOMLSUPPORT

#include "ValueObject.hpp"
#include "Context.hpp"

#include <sstream> // std::ostringstream
#include <optional>


namespace teascript {

class TomlSupport
{
    // Toml value --> Tuple value
    static void DispatchKeyValue( Context &rContext, Tuple &rParent, std::optional<std::string> const &key, toml::node const &rNode )
    {
        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};

        switch( rNode.type() ) {
        case toml::node_type::table:
            {
                Tuple  table;
                DispatchTable( rContext, table, *rNode.as_table() );
                if( not key ) {
                    rParent.AppendValue( ValueObject( std::move( table ), cfg ) );
                } else {
                    rParent.AppendKeyValue( key.value(), ValueObject(std::move(table), cfg));
                }
            }
            break;
        case toml::node_type::array:
            {
                Tuple  arr;
                DispatchArray( rContext, arr, *rNode.as_array() );
                if( not key ) {
                    rParent.AppendValue( ValueObject( std::move( arr ), cfg ) );
                } else {
                    rParent.AppendKeyValue( key.value(), ValueObject(std::move(arr), cfg));
                }
            }
            break;
        case toml::node_type::integer:
            if( not key ) {
                rParent.AppendValue( ValueObject( static_cast<I64>(rNode.as_integer()->get()), cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(static_cast<I64>(rNode.as_integer()->get()), cfg));
            }
            break;
        case toml::node_type::floating_point:
            if( not key ) {
                rParent.AppendValue( ValueObject( rNode.as_floating_point()->get(), cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(rNode.as_floating_point()->get(), cfg));
            }
            break;
        case toml::node_type::string:
            if( not key ) {
                rParent.AppendValue( ValueObject( rNode.as_string()->get(), cfg));
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(rNode.as_string()->get(), cfg));
            }
            break;
        case toml::node_type::boolean:
            if( not key ) {
                rParent.AppendValue( ValueObject( rNode.as_boolean()->get(), cfg ) );
            } else {
                rParent.AppendKeyValue( key.value(), ValueObject(rNode.as_boolean()->get(), cfg));
            }
            break;
        case toml::node_type::date_time:
        case toml::node_type::date:
        case toml::node_type::time:
            {
                std::ostringstream  os;
                switch( rNode.type() ) {
                case toml::node_type::date_time:
                    os << rNode.value<toml::date_time>().value();
                    break;
                case toml::node_type::date:
                    os << rNode.value<toml::date>().value();
                    break;
                case toml::node_type::time:
                    os << rNode.value<toml::time>().value();
                    break;
                default:
                    //TODO: use std::unreachable in C++23
#if defined( _MSC_VER ) // MSVC
                    __assume(false);
#else // GCC, clang, ...
                    __builtin_unreachable();
#endif
                }

                if( not key ) {
                    rParent.AppendValue( ValueObject( os.str(), cfg));
                } else {
                    rParent.AppendKeyValue( key.value(), ValueObject(os.str(), cfg));
                }
            }
            break;
        case toml::node_type::none:
            break;
        default: // should not happen, but just in case... (unfortunately the compiler did not warn if one case of node_type was forgotten... :( )
            throw std::runtime_error( "Unexpected / unknown TOML node type!" );
            break;
        }
    }

    // Toml array --> Tuple
    static void DispatchArray( Context &rContext, Tuple &rParent, toml::array const &rArray )
    {
        // special case: empty array!
        // An empty Tuple cannot be distinguished whether it belongs to an empty object or empty array!
        // for that reason we insert an empty Buffer, which is not a valid value for Toml.
        // If the array is filled later, the empty buffer must be removed for show correct size.
        if( rArray.empty() ) {
            auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
            rParent.AppendValue( ValueObject( Buffer(), cfg ) );
        } else for( auto const &node : rArray ) {
            DispatchKeyValue( rContext, rParent, std::nullopt, node );
        }
    }

    // Toml table --> Tuple
    static void DispatchTable( Context &rContext, Tuple &rParent, toml::table const &rTable )
    {
        for( auto const &kv : rTable ) {
            DispatchKeyValue( rContext, rParent, std::string(kv.first), kv.second);
        }
    }

    static void DispatchTuple( Tuple const &rTuple, toml::array &rParent )
    {
        for( auto const &kv : rTuple ) {
            if( not kv.first.empty() ) {
                throw std::runtime_error( "toml arrays cannot have keys!" );
            }
            switch( kv.second.InternalType() ) {
            case ValueObject::TypeTuple:
                {
                    Tuple const &tup = kv.second.GetValue<Tuple>();
                    if( tuple::TomlJsonUtil::IsTupAnArray( tup ) ) { // array
                        auto & res = rParent.emplace_back<toml::array>();
                        DispatchTuple( tup, res );
                    } else { // table
                        auto & res = rParent.emplace_back<toml::table>();
                        DispatchTuple( tup, res );
                    }
                }
                break;
            case ValueObject::TypeString:
                std::ignore = rParent.emplace_back<std::string>( kv.second.GetValue<std::string>() );
                break;
            case ValueObject::TypeF64:
                std::ignore = rParent.emplace_back<double>( kv.second.GetValue<double>() );
                break;
            case ValueObject::TypeU8:
            case ValueObject::TypeI64:
            case ValueObject::TypeU64:
                std::ignore = rParent.emplace_back<int64_t>( kv.second.GetAsInteger() );
                break;
            case ValueObject::TypeBool:
                std::ignore = rParent.emplace_back<bool>( kv.second.GetValue<bool>() );
                break;
            case ValueObject::TypeBuffer:
                if( kv.second.GetValue<Buffer>().empty() ) { // special case: empty Buffer marks an empty array!
                    break;
                }
                [[fallthrough]]; // non empty Buffer are an error!
            default:
                throw std::runtime_error( "unsupported ValueObject type for Toml!" );
            }
        }
    }

    // Tuple --> Toml table
    static void DispatchTuple( Tuple const &rTuple, toml::table &rParent )
    {
        for( auto const &kv : rTuple ) {
            switch( kv.second.InternalType() ) {
            case ValueObject::TypeTuple:
                {
                    Tuple const &tup = kv.second.GetValue<Tuple>();
                    if( tuple::TomlJsonUtil::IsTupAnArray( tup ) ) { // array
                        auto res = rParent.emplace<toml::array>( kv.first );
                        if( not res.second ) {
                            throw std::runtime_error( "could not add new toml array with key " + kv.first );
                        }
                        DispatchTuple( tup, *res.first->second.as_array() );
                    } else { // table
                        auto res = rParent.emplace<toml::table>( kv.first );
                        if( not res.second ) {
                            throw std::runtime_error( "could not add new toml table with key " + kv.first );
                        }
                        DispatchTuple( tup, *res.first->second.as_table() );
                    }
                }
                break;
            case ValueObject::TypeString:
                std::ignore = rParent.emplace<std::string>( kv.first, kv.second.GetValue<std::string>() );
                break;
            case ValueObject::TypeF64:
                std::ignore = rParent.emplace<double>( kv.first, kv.second.GetValue<double>() );
                break;
            case ValueObject::TypeU8:
            case ValueObject::TypeI64:
            case ValueObject::TypeU64:
                std::ignore = rParent.emplace<int64_t>( kv.first, static_cast<int64_t>(kv.second.GetAsInteger()) );
                break;
            case ValueObject::TypeBool:
                std::ignore = rParent.emplace<bool>( kv.first, kv.second.GetValue<bool>() );
                break;
            default:
                throw std::runtime_error( "unsupported ValueObject type for Toml!" );
            }
        }
    }

public:
    /// Constructs a Tuple structure from the given Toml formatted string.
    static ValueObject ReadTomlString( Context &rContext, std::string const &rTomlStr )
    {
        Tuple res;

        try {
            auto const toml_table_root = toml::parse( rTomlStr );
            DispatchTable( rContext, res, toml_table_root );
        } catch( std::exception const & /*ex*/ ) {
            //TODO better error handlung.//auto s = ex.what();
            return ValueObject( false );
        }

        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};
        return ValueObject( std::move(res), cfg);
    }

    /// Constructs a Tuple from a given toml::table object. \throw This function throws on error.
    static void TomlToTuple( Context &rContext, Tuple &rOut, toml::table const &rTable )
    {
        rOut.Clear();
        DispatchTable( rContext, rOut, rTable );
    }

    /// Constructs a Toml formatted string from the given Tuple.
    /// \return the constructed string or false on error.
    /// \note the Tuple must only contain supported types and layout for Toml.
    static ValueObject WriteTomlString( Tuple const &rTuple )
    {
        toml::table  table;
        try {
            DispatchTuple( rTuple, table );
        } catch( std::exception const & /*ex*/ ) {
            //TODO better error handlung.//auto s = ex.what();
            return ValueObject( false );
        }
        std::ostringstream  os;
        os << table;
        return ValueObject( os.str() );
    }

    /// Constructs a Toml table from given Tuple. \throw This function throws on error.
    /// \note the Tuple must only contain supported types and layout for Toml.
    static void TupleToToml( Tuple const &rTuple, toml::table &rOut )
    {
        rOut.clear();
        DispatchTuple( rTuple, rOut );
    }
};

} // namespace teascript

#endif // TEASCRIPT_TOMLSUPPORT

