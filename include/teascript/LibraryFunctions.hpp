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

// DEPRECATED!
template< typename T >
inline
T &get_value( ValueObject &rObj )
{
    if constexpr( std::is_same_v<T, ValueObject> ) {
        return rObj;
    } else {
        return rObj.GetValue<T>();
    }
}

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


//DONE! During a rainy afternoon and after 2 cups of coffee, I will convert these LibraryFunctions to one generic one dealing with arbitrary parameter count
//      by using variadic parameter pack... :-)
//      Update: but it was during a hot summer afternoon!


// IMPORTANT: The next functions, LibraryFunction0 to LibraryFunction5, are DEPRECATED!
//            Please, use the new and improved LibraryFunction (above) instead!

// DEPRECATED! Please, use LibraryFunction instead!
template< typename F, typename RES = void >
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction0 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction0( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction0() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 0u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 0)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc();
        } else if constexpr( not std::is_same_v<RES, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            return ValueObject( mpFunc(), cfg );
        } else {
            mpFunc();
            return {};
        }
    }

    int ParamCount() const final
    {
        return 0;
    }
};

// DEPRECATED! Please, use LibraryFunction instead!
// experimental variant with bool with_context. If true a first parameter is added to thw call: the Context.
template< typename F, typename T1, typename RES = void, bool with_context = false>
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction1 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction1( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction1() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 1u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 1)!" );
        }
        if constexpr( not with_context ) {
            if constexpr( std::is_same_v<RES, ValueObject> ) {
                return mpFunc( util::get_value<T1>( rParams[0] ) );
            } else if constexpr( not std::is_same_v<RES, void> ) {
                auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
                return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ) ), cfg );
            } else {
                mpFunc( util::get_value<T1>( rParams[0] ) );
                return {};
            }
        } else {
            if constexpr( std::is_same_v<RES, ValueObject> ) {
                return mpFunc( rContext, util::get_value<T1>( rParams[0] ) );
            } else if constexpr( not std::is_same_v<RES, void> ) {
                auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
                return ValueObject( mpFunc( rContext, util::get_value<T1>( rParams[0] ) ), cfg );
            } else {
                mpFunc( rContext, util::get_value<T1>( rParams[0] ) );
                return {};
            }
        }
    }

    int ParamCount() const final
    {
        return 1;
    }
};

// DEPRECATED! Please, use LibraryFunction instead!
template< typename F, typename T1, typename T2, typename RES = void>
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction2 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction2( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction2() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 2u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 2)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ) ), cfg );
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ) );
            return {};
        }
    }

    int ParamCount() const final
    {
        return 2;
    }
};

// DEPRECATED! Please, use LibraryFunction instead!
template< typename F, typename T1, typename T2, typename T3, typename RES = void>
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction3 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction3( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction3() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 3u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 3)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ) ), cfg );
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ) );
            return {};
        }
    }

    int ParamCount() const final
    {
        return 3;
    }
};

// DEPRECATED! Please, use LibraryFunction instead!
template< typename F, typename T1, typename T2, typename T3, typename T4, typename RES = void>
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction4 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction4( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction4() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 4u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 4)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ) ), cfg );
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ) );
            return {};
        }
    }

    int ParamCount() const final
    {
        return 4;
    }
};

// DEPRECATED! Please, use LibraryFunction instead!
template< typename F, typename T1, typename T2, typename T3, typename T4, typename T5, typename RES = void>
class [[deprecated( "Please, use the new and generic LibraryFunction<> instead!" )]] LibraryFunction5 : public FunctionBase
{
    F *mpFunc;
public:
    LibraryFunction5( F *f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction5() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( 5u != rParams.size() ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters (must be 5)!" );
        }
        if constexpr( std::is_same_v<RES, ValueObject> ) {
            return mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ), util::get_value<T5>( rParams[4] ) );
        } else if constexpr( not std::is_same_v<RES, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable, rContext.GetTypeSystem() ); // return values are unshared by default.
            return ValueObject( mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ), util::get_value<T5>( rParams[4] ) ), cfg );
        } else {
            mpFunc( util::get_value<T1>( rParams[0] ), util::get_value<T2>( rParams[1] ), util::get_value<T3>( rParams[2] ), util::get_value<T4>( rParams[3] ), util::get_value<T5>( rParams[4] ) );
            return {};
        }
    }

    int ParamCount() const final
    {
        return 5;
    }
};


} // namespace teascript

