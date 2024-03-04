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


namespace teascript {

class TomlSupport
{
    static void DispatchKeyValue( Context &rContext, Tuple &rParent, std::string_view const key, toml::node const &rNode )
    {
        auto const cfg = ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()};

        switch( rNode.type() ) {
        case toml::node_type::table:
            {
                Tuple  table;
                DispatchTable( rContext, table, *rNode.as_table() );
                if( key.empty() ) {
                    rParent.AppendValue( ValueObject( std::move( table ), cfg ) );
                } else {
                    rParent.AppendKeyValue( std::string( key ), ValueObject( std::move( table ), cfg ) );
                }
            }
            break;
        case toml::node_type::array:
            {
                Tuple  arr;
                DispatchArray( rContext, arr, *rNode.as_array() );
                if( key.empty() ) {
                    rParent.AppendValue( ValueObject( std::move( arr ), cfg ) );
                } else {
                    rParent.AppendKeyValue( std::string( key ), ValueObject( std::move( arr ), cfg ) );
                }
            }
            break;
        case toml::node_type::integer:
            if( key.empty() ) {
                rParent.AppendValue( ValueObject( static_cast<I64>(rNode.as_integer()->get()), cfg ) );
            } else {
                rParent.AppendKeyValue( std::string( key ), ValueObject( static_cast<I64>(rNode.as_integer()->get()), cfg ) );
            }
            break;
        case toml::node_type::floating_point:
            if( key.empty() ) {
                rParent.AppendValue( ValueObject( rNode.as_floating_point()->get(), cfg ) );
            } else {
                rParent.AppendKeyValue( std::string( key ), ValueObject( rNode.as_floating_point()->get(), cfg ) );
            }
            break;
        case toml::node_type::string:
            if( key.empty() ) {
                rParent.AppendValue( ValueObject( rNode.as_string()->get(), cfg));
            } else {
                rParent.AppendKeyValue( std::string( key ), ValueObject( rNode.as_string()->get(), cfg ) );
            }
            break;
        case toml::node_type::boolean:
            if( key.empty() ) {
                rParent.AppendValue( ValueObject( rNode.as_boolean()->get(), cfg ) );
            } else {
                rParent.AppendKeyValue( std::string( key ), ValueObject( rNode.as_boolean()->get(), cfg ) );
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

                if( key.empty() ) {
                    rParent.AppendValue( ValueObject( os.str(), cfg));
                } else {
                    rParent.AppendKeyValue( std::string( key ), ValueObject( os.str(), cfg ) );
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

    static void DispatchArray( Context &rContext, Tuple &rParent, toml::array const &rArray )
    {
        for( auto const &node : rArray ) {
            DispatchKeyValue( rContext, rParent, "", node );
        }
    }

    static void DispatchTable( Context &rContext, Tuple &rParent, toml::table const &rTable )
    {
        for( auto const &kv : rTable ) {
            DispatchKeyValue( rContext, rParent, kv.first, kv.second );
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
};

} // namespace teascript

#endif // TEASCRIPT_TOMLSUPPORT

