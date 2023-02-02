/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
 */
#pragma once

#include "ValueObject.hpp"
#include "Type.hpp"
#include "Exception.hpp"
#include <type_traits>


namespace teascript {

namespace util {


/// Computes the used type for calculation and the resulting type at compile time for 2 given types T and U.
template< ArithmeticNumber T, ArithmeticNumber U >
struct PromoteForCalc
{
private:
    // first pick the biggest, or the possibly unsigned one.
    using R4 = std::conditional_t< (sizeof( T ) < sizeof( U )) || (sizeof( T ) == sizeof( U ) && std::is_signed_v<T>), U, T >;
    // keep unsigned (the biggest) since they can overflow. but signed will be 64
    using R3 = std::conditional_t< std::is_unsigned_v<R4>, R4, I64>;
    // floating points always win
    using R2 = std::conditional_t<std::is_same_v<T, float> || std::is_same_v<U, float>, float, R3>;
    using R1 = std::conditional_t<std::is_same_v<T, F64> || std::is_same_v<U, F64>, F64, R2>;

    using T1 = std::conditional_t< std::is_unsigned_v<R1>, U64, R1>;
public:
    using Type =  T1;
    using Res  =  R1;
};


//TODO: change op from string to Operator enum!
//TODO: overflow check for signed operation if ResType is smaller.
//TODO: optionally pre-check overflow.
//TODO: Don't throw here but use error_code and/or ValueObject w. some Error class/exception and throw later in ASTNode!!!!
/// Applies the arithmetic operations to the operands and do checks and conversions depending on the types.
struct ArithmeticHelper
{
    template< ArithmeticNumber T >
    inline static T unary_plus( T const op1 ) noexcept
    {
        return +op1;  // well, for the sake of completeness... ;-)
    }

    template< ArithmeticNumber T >
    inline static T unary_minus( T const op1 ) noexcept
    {
        if constexpr( std::is_unsigned_v<T> ) {
            return static_cast<T>( - static_cast<std::make_signed_t<T>>(op1) );
        } else {
            return static_cast<T>(-op1);
        }
    }

    template< ArithmeticNumber Res, ArithmeticNumber T >
    inline static Res plus( T const op1, T const op2 ) noexcept
    {
        return static_cast<Res>(op1 + op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T >
    inline static Res minus( T const op1, T const op2 ) noexcept
    {
        return static_cast<Res>(op1 - op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T >
    inline static Res multiply( T const op1, T const op2 ) noexcept
    {
        return static_cast<Res>(op1 * op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T >
    inline static Res divide( T const op1, T const op2 ) noexcept( std::is_floating_point_v<T> )
    {
        if constexpr( std::is_integral_v<T> ) {
            if( op2 == 0 ) {
                throw exception::division_by_zero();
            }
        }
        return static_cast<Res>(op1 / op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T >
    inline static Res modulo( T const op1, T const op2 )
    {
        if constexpr( std::is_integral_v<T> ) {
            if( op2 == 0 ) {
                throw exception::division_by_zero();
            }
            return static_cast<Res>(op1 % op2);
        } else {
            throw exception::modulo_with_floatingpoint();
        }
    }

    template< ArithmeticNumber T >
    inline static auto EvalUnOp( T const op1, std::string const &op ) noexcept -> T
    {
        if( op == "-" ) {
            return unary_minus( op1 );
        } else /* if( op == "+" ) */ {
            return unary_plus( op1 );
        }
        // throw exception::eval_error( "Internal Error! Unknown Unary Operator!!" );
    }

    //TODO: Split in divide/modulo and rest, where rest can be noexcept then.
    template< ArithmeticNumber T, ArithmeticNumber U >
    inline static auto EvalBinOp( T const op1, U const op2, std::string const &op ) -> typename PromoteForCalc<T, U>::Res
    {
        using CalcType = typename PromoteForCalc<T, U>::Type;
        using ResultType = typename PromoteForCalc<T, U>::Res;
        if( op == "+" ) {
            return plus<ResultType>( static_cast<CalcType>(op1), static_cast<CalcType>(op2) );
        } else if( op == "-" ) {
            return minus<ResultType>( static_cast<CalcType>(op1), static_cast<CalcType>(op2) );
        } else if( op == "*" ) {
            return multiply<ResultType>( static_cast<CalcType>(op1), static_cast<CalcType>(op2) );
        } else if( op == "/" ) {
            return divide<ResultType>( static_cast<CalcType>(op1), static_cast<CalcType>(op2) );
        } else if( op == "mod" ) {
            return modulo<ResultType>( static_cast<CalcType>(op1), static_cast<CalcType>(op2) );
        }

        //TODO: remove this throw? Must be handled outside
        throw exception::eval_error( "Internal Error! Unknown Binary Operator!!" );
    }
};

/// Applies unary or binary arithmetic operators to one / two given ValueObject(s) by extracting the arithemetic umderlying type or try conversion to long long
/// and returns a new ValueObject with the result.
class ArtithmeticFactory
{
    template< ArithmeticNumber T>
    inline static ValueObject ApplyRHS( T const lhs, ValueObject const &o2, std::string const & op )
    {
        //TODO: use TypeInfo or Variant Index!
        if( o2.GetValuePtr<I64>() ) {
            return ValueObject{ ArithmeticHelper::EvalBinOp( lhs, o2.GetValue<I64>(), op ) };
        } else if( o2.GetValuePtr<F64>() ) {
            return ValueObject{ ArithmeticHelper::EvalBinOp( lhs, o2.GetValue<F64>(), op ) };
        } else {
            return ValueObject{ArithmeticHelper::EvalBinOp( lhs, o2.GetAsInteger(), op )};
        }
    }
public:
    inline static ValueObject ApplyBinOp( ValueObject const &o1, ValueObject const &o2, std::string const &op )
    {
        //TODO: use TypeInfo or Variant Index!
        if( o1.GetValuePtr<I64>() ) {
            return ApplyRHS( o1.GetValue<I64>(), o2, op );
        } else if( o1.GetValuePtr<F64>() ) {
            return ApplyRHS( o1.GetValue<F64>(), o2, op );
        } else {
            return ApplyRHS( o1.GetAsInteger(), o2, op);
        }
    }

    inline static ValueObject ApplyUnOp( ValueObject const &o1, std::string const &op )
    {
        //TODO: use TypeInfo or Variant Index!
        if( o1.GetValuePtr<I64>() ) {
            return ValueObject{ArithmeticHelper::EvalUnOp( o1.GetValue<I64>(), op )};
        } else if( o1.GetValuePtr<F64>() ) {
            return ValueObject{ArithmeticHelper::EvalUnOp( o1.GetValue<F64>(), op )};
        } else {
            return ValueObject{ArithmeticHelper::EvalUnOp( o1.GetAsInteger(), op )};
        }
    }
};


} // namespace util

} // namespace teascript

