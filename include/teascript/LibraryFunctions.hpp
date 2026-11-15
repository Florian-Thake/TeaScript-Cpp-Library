/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "LibraryFunctionTemplate.hpp"
#include "Content.hpp"
#include "Parser.hpp"

#if TEASCRIPT_FMTFORMAT
# include "fmt/args.h"
#endif


#include <fstream>


namespace teascript {


/// The function object for evaluate TeaScript code within TeaScript code.
class EvalFunc : public FunctionBase
{
    bool mLoadFile;
public:
    explicit EvalFunc( bool file )
        : mLoadFile( file )
    {
    }
    virtual ~EvalFunc() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 1u != rParams.size() ) { // maybe can be relaxed (e.g. optional parameters, or list of expr)?
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 1)!" );
        }

        //NOTE: since we don't open a new scope here, we can add/modify the scope of caller!
        //TODO: This might have unwanted side effects. Must provide an optional way for a clean scope and/or clean environment.

        Content content;
        std::vector<char> buf;
        std::string str;
        std::string filename;
        if( mLoadFile ) {
            // TODO: parameter for script ? Can register args as real ValueObjects instead of string! But must avoid to override args of caller script!!!
            str = rParams[0].GetValueCopy<std::string>();
            // NOTE: TeaScript strings are UTF-8
            // TODO: apply include paths before try absolute()
            std::filesystem::path const script = std::filesystem::absolute( util::utf8_path( str ) );
            std::ifstream file( script, std::ios::binary | std::ios::ate ); // ate jumps to end.
            if( file ) {
                auto size = file.tellg();
                file.seekg( 0 );
                buf.resize( static_cast<size_t>(size) + 1 ); // ensure zero terminating!
                file.read( buf.data(), size );
                content = Content( buf.data(), buf.size() );
                // build utf-8 filename again... *grrr*
                filename = util::utf8_path_to_str( script );
            } else {
                // TODO: Better return an error ?
                throw exception::load_file_error( rLoc, str );
            }
        } else {
            str = rParams[0].GetValueCopy<std::string>();
            content = Content( str );
            filename = "_EVALFUNC_";
        }

        Parser p;
        p.OverwriteDialect( rContext.GetSettings().GetDialect() ); // use eventually modified dialect.
        p.SetDebug( rContext.GetSettings().IsDebug() );
        try {
            return p.Parse( content, filename )->Eval( rContext );
        } catch( exception::eval_error const &/*ex*/ ) {
            throw;
            //return {}; // TODO: unified and improved error handling. Return an eval_error? or just dont catch?
        } catch( exception::parsing_error const &/*ex*/ ) {
            throw;
            //return {}; // TODO: unified and improved error handling. Return an eval_error? or just dont catch?
        }
    }

    int ParamCount() const final
    {
        return 1;
    }
};

/// creates a (unamed) tuple object with arbiratry amount of elements, e.g., accepts 0..N parameters.
class MakeTupleFunc : public FunctionBase
{
public:
    enum class eFlavor
    {
        Normal,
        TomlJsonArray,
        Dictionary,
    } mFlavor;

    inline explicit MakeTupleFunc( eFlavor const flavor = eFlavor::Normal ) : FunctionBase(), mFlavor( flavor ) {}
    virtual ~MakeTupleFunc() {}

    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        Tuple  tuple;

        if( rParams.size() > 1 ) {
            tuple.Reserve( rParams.size() );
        }
        if( mFlavor == eFlavor::Dictionary ) {
            for( auto const &v : rParams ) {
                Tuple const *p = v.GetValuePtr<Tuple>();
                if( p == nullptr || p->Size() != 2 || p->begin()->second.InternalType() != ValueObject::TypeString ) {
                    throw exception::bad_value_cast( "dictionaries need pairs with key|value as input, key must be a String!", rLoc ); //TODO: change to Error return later?!
                }
                tuple.AppendKeyValue( p->GetValueByIdx_Unchecked( 0 ).GetValue<std::string>(), p->GetValueByIdx_Unchecked( 1 ) );
            }
        } else for( auto const &v : rParams ) {
            tuple.AppendValue( v );
        }

        // special case for mark an empty toml/json array.
        if( mFlavor == eFlavor::TomlJsonArray && tuple.IsEmpty() ) {
            tuple.AppendValue( ValueObject( Buffer(), ValueConfig{ValueShared,ValueMutable} ) );
        }

        return ValueObject( std::move( tuple ), ValueConfig{ValueShared,ValueMutable} );
    }
};

/// creates a new map with given key-value pairs (as tuples).
class MakeMapFunc : public FunctionBase
{
public:
    ValueObject Call( Context &/*rContext*/, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        Map map;
        for( auto const &v : rParams ) {
            Tuple const *p = v.GetValuePtr<Tuple>();
            if( p == nullptr || p->Size() != 2 ) {
                throw exception::bad_value_cast( "maps need pairs with key|value as input!", rLoc ); //TODO: change to Error return later?!
            }
            map.emplace( p->GetValueByIdx_Unchecked( 0 ), p->GetValueByIdx_Unchecked( 1 ) );
        }
        return ValueObject( std::move( map ), ValueConfig{ValueShared,ValueMutable} );
    }
};


class FormatStringFunc : public FunctionBase
{
public:
    FormatStringFunc() = default;
    virtual ~FormatStringFunc() {}

    ValueObject Call( [[maybe_unused]] Context &rContext, [[maybe_unused]] std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
#if TEASCRIPT_FMTFORMAT
        if( rParams.size() < 1 || not rParams[0].GetTypeInfo()->IsSame<std::string>() ) {
            throw exception::eval_error( rLoc, "FormatStringFunc Call: Need first parameter as the format string!" );
        }
        std::string const &format_str = rParams[0].GetConstValue<std::string>();
        fmt::dynamic_format_arg_store<fmt::format_context>  store;
        // skip first, which is the format string.
        for( size_t idx = 1; idx < rParams.size(); ++idx ) {
            switch( rParams[idx].InternalType() ) {
            case ValueObject::TypeBool:
                store.push_back( rParams[idx].GetValueCopy<Bool>() );
                break;
            case ValueObject::TypeI64:
                store.push_back( rParams[idx].GetValueCopy<I64>() );
                break;
            case ValueObject::TypeU64:
                store.push_back( rParams[idx].GetValueCopy<U64>() );
                break;
            case ValueObject::TypeU8:
                store.push_back( rParams[idx].GetValueCopy<U8>() );
                break;
            case ValueObject::TypeF64:
                store.push_back( rParams[idx].GetValueCopy<F64>() );
                break;
            case ValueObject::TypeString:
                store.push_back( std::cref(rParams[idx].GetConstValue<String>()) );
                break;
            default:
                // try as a String
                store.push_back( rParams[idx].GetAsString() );
            }
        }

        std::string res = fmt::vformat( format_str, store );

        return ValueObject( std::move( res ), ValueConfig{ValueShared,ValueMutable} );
#else
        throw exception::runtime_error( rLoc, "FormatStringFunc Call: You must use libfmt to make this working!" );
#endif
    }
};

} // namespace teascript
