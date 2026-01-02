/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Func.hpp"
#include "Context.hpp"
#include "Content.hpp"
#include "Parser.hpp"

#if TEASCRIPT_FMTFORMAT
# include "fmt/args.h"
#endif


#include <fstream>
#include <tuple>


namespace teascript {

namespace util {

template< typename T >
inline
T get_value_ex( ValueObject &rObj )
{
    if constexpr( std::is_same_v<std::remove_cvref_t<T>, ValueObject> ) {
        return rObj;
    } else if constexpr( not std::is_reference_v<T> ) {
        // obtain a copy, but use the const overload for can handle const and non const values.
        return rObj.GetValue< std::add_const_t<T> >();
    } else {
        return rObj.GetValue< std::remove_volatile_t<std::remove_reference_t<T>> >();
    }
}

// helper function for invoke a function with arbitrary parameters from a ValueObject vector, either with the inner value or with the ValueObject,
// depending of the function parameters (which are passed as TS param pack)
template< typename F, typename ...TS, size_t ... IS>
auto call_helper( F func, std::vector<ValueObject> &rParams, std::integer_sequence<size_t, IS...> ) -> auto
{
    return func( util::get_value_ex<TS>( rParams[IS] )... );
}

// helper struct for building a tuple from the parameter pack where the first parameter is omitted.
template< bool, typename ...TS >
struct RemoveFirst;

template< typename ...TS >
struct RemoveFirst<false, TS...>
{
    using type = std::tuple<TS...>;
};
template< typename T, typename ...TS >
struct RemoveFirst<true, T, TS...>
{
    using type = std::tuple<TS...>;
};

// helper function for invoke a function with arbitrary parameters from a ValueObject vector, either with the inner value or with the ValueObject,
// depending of the function parameters (which are passed as TS param pack).
// Here the first parameter is the Context, so we must skip the first parameter from the pack. The integer sequence must be adjusted already!
template< typename F, typename ...TS, size_t ... IS>
auto call_context_helper( F func, Context &rContext, std::vector<ValueObject> &rParams, std::integer_sequence<size_t, IS...> ) -> auto
{
    using tuple_new = typename RemoveFirst<true, TS...>::type;
    return func( rContext, util::get_value_ex<std::tuple_element_t<IS, tuple_new>>( rParams[IS] )... );
}

// helper struct for determine whether first parameter of a parameter pack is of type Context
template< typename Tup >
struct check_context_param;

template<>
struct check_context_param<std::tuple<>> // must specialize for empty tuple for avoid index out of bound errors.
{
    static constexpr bool value = false;
};

template<typename ...TS>
struct check_context_param<std::tuple<TS...>>
{
    static constexpr bool value = std::is_same_v< Context &, std::remove_const_t<std::tuple_element_t<0, std::tuple<TS...>>>>;
};


} // namespace util


template<typename F>
class LibraryFunction;

/// class LibraryFunction for calling arbitrary C++ functions from TeaScript.
/// A shared_ptr with an instance of this class can be put into a ValueObject and then be stored
/// as a variable in the Context. This variable is then callable from TeaScript and the inner
/// C++ function will be called with the corresponding parameters.
/// The first parameter can optionally be the Context which will then be passed through.
/// The C++ function can either take ValueObjects or any type which can be directly stored
/// inside the ValueObject as parameter types. The same is true for the return type.
/// This is the new and improved variant which can handle all cases and more of the old
/// LibraryFunction0 to LibraryFunction5.
/// This new variant will fully replace the old variants. The old variants will be removed in the future.
/// \note For the time being only Function pointers are supported. For other cases, please, use UserCallbackFunc
template< typename RET, typename ...TS >
class LibraryFunction< RET (TS...) > : public FunctionBase
{
public:
    using FuncPtrType = RET (*)(TS...);
    using ReturnType  = RET;
    using ArgumentTuple = std::tuple< TS... >;
    static constexpr bool HasContextParam = util::check_context_param<ArgumentTuple>::value;
    static constexpr int  ArgN = static_cast<int>(std::tuple_size_v<ArgumentTuple>) - static_cast<int>(HasContextParam);
private:
    FuncPtrType mpFunc;

public:
    LibraryFunction( FuncPtrType f )
        : FunctionBase()
        , mpFunc( f )
    { }

    virtual ~LibraryFunction() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( rParams.size() != static_cast<size_t>(ArgN) ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters! Expected: " + std::to_string(ArgN) );
        }

        if constexpr( std::is_same_v<ReturnType, ValueObject> ) {
            if constexpr( HasContextParam ) {
                return util::call_context_helper< FuncPtrType, TS...>( mpFunc, rContext, rParams, std::make_index_sequence<ArgN>{} );
            } else {
                return util::call_helper< FuncPtrType, TS...>( mpFunc, rParams, std::make_index_sequence<ArgN>{} );
            }
        } else if constexpr( not std::is_same_v<ReturnType, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            if constexpr( HasContextParam ) {
                return ValueObject( util::call_context_helper< FuncPtrType, TS...>( mpFunc, rContext, rParams, std::make_index_sequence<ArgN>{} ), cfg );
            } else {
                return ValueObject( util::call_helper< FuncPtrType, TS...>( mpFunc, rParams, std::make_index_sequence<ArgN>{} ), cfg );
            }
        } else {
            if constexpr( HasContextParam ) {
                util::call_context_helper< FuncPtrType, TS...>( mpFunc, rContext, rParams, std::make_index_sequence<ArgN>{} );
            } else {
                util::call_helper< FuncPtrType, TS...>( mpFunc, rParams, std::make_index_sequence<ArgN>{} );
            }
            return {};
        }
    }

    int ParamCount() const final
    {
        return ArgN;
    }
};



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

        Parser p; //FIXME: for later versions: must use correct state with correct factory.
        p.OverwriteDialect( rContext.dialect ); // use eventually modified dialect.
        p.SetDebug( rContext.is_debug );
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

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &/*rLoc*/ ) override
    {
        Tuple  tuple;

        if( rParams.size() > 1 ) {
            tuple.Reserve( rParams.size() );
        }
        if( mFlavor == eFlavor::Dictionary ) {
            for( auto const &v : rParams ) {
                Tuple const *p = v.GetValuePtr<Tuple>();
                if( p == nullptr || p->Size() != 2 || p->begin()->second.InternalType() != ValueObject::TypeString ) {
                    throw exception::bad_value_cast( "dictionaries need pairs with key|value as input, key must be a String!" ); //TODO: change to Error return later?!
                }
                tuple.AppendKeyValue( p->GetValueByIdx_Unchecked( 0 ).GetValue<std::string>(), p->GetValueByIdx_Unchecked( 1 ) );
            }
        } else for( auto const &v : rParams ) {
            tuple.AppendValue( v );
        }

        // special case for mark an empty toml/json array.
        if( mFlavor == eFlavor::TomlJsonArray && tuple.IsEmpty() ) {
            tuple.AppendValue( ValueObject( Buffer(), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} ) );
        }

        return ValueObject( std::move( tuple ), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} );
    }
};


class FormatStringFunc : public FunctionBase
{
public:
    FormatStringFunc() = default;
    virtual ~FormatStringFunc() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
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

        return ValueObject( std::move( res ), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} );
#else
        (void)rContext;
        (void)rParams;
        throw exception::runtime_error( rLoc, "FormatStringFunc Call: You must use libfmt to make this working!" );
#endif
    }
};

} // namespace teascript
