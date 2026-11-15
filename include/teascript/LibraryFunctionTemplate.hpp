/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "FunctionBase.hpp"
#include "Context.hpp"

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
class LibraryFunction< RET( TS... ) > : public FunctionBase
{
public:
    using FuncPtrType = RET( * )(TS...);
    using ReturnType = RET;
    using ArgumentTuple = std::tuple< TS... >;
    static constexpr bool HasContextParam = util::check_context_param<ArgumentTuple>::value;
    static constexpr int  ArgN = static_cast<int>(std::tuple_size_v<ArgumentTuple>) - static_cast<int>(HasContextParam);
private:
    FuncPtrType mpFunc;

public:
    LibraryFunction( FuncPtrType f )
        : FunctionBase()
        , mpFunc( f )
    {
    }

    virtual ~LibraryFunction() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        if( rParams.size() != static_cast<size_t>(ArgN) ) {
            throw exception::eval_error( rLoc, "Func Call: Wrong amount of passed parameters! Expected: " + std::to_string( ArgN ) );
        }

        if constexpr( std::is_same_v<ReturnType, ValueObject> ) {
            if constexpr( HasContextParam ) {
                return util::call_context_helper< FuncPtrType, TS...>( mpFunc, rContext, rParams, std::make_index_sequence<ArgN>{} );
            } else {
                return util::call_helper< FuncPtrType, TS...>( mpFunc, rParams, std::make_index_sequence<ArgN>{} );
            }
        } else if constexpr( not std::is_same_v<ReturnType, void> ) {
            auto const cfg = ValueConfig( ValueUnshared, ValueMutable ); // return values are unshared by default.
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


} // namespace teascript
