/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
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

    //using T1 = std::conditional_t< std::is_unsigned_v<R1>, U64, R1>;
    using T1 = std::conditional_t< std::is_floating_point_v<R1>, R1, T>;
    using U1 = std::conditional_t< std::is_floating_point_v<R1>, R1, U>;
public:
    using Type1 = T1;
    using Type2 = U1;
    using Res   = R1;
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

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res plus( T1 const op1, T2 const op2 ) noexcept
    {
        return static_cast<Res>(op1 + op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res minus( T1 const op1, T2 const op2 ) noexcept
    {
        return static_cast<Res>(op1 - op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res multiply( T1 const op1, T2 const op2 ) noexcept
    {
        return static_cast<Res>(op1 * op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res divide( T1 const op1, T2 const op2 ) noexcept( std::is_floating_point_v<T2> )
    {
        if constexpr( std::is_integral_v<T2> ) {
            if( op2 == 0 ) [[unlikely]] {
                throw exception::division_by_zero();
            }
        }
        return static_cast<Res>(op1 / op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res modulo( T1 const op1, T2 const op2 )
    {
        if constexpr( std::is_integral_v<Res> ) {
            if( op2 == 0 ) [[unlikely]] {
                throw exception::division_by_zero();
            }
            return static_cast<Res>(op1 % op2);
        } else {
            throw exception::modulo_with_floatingpoint();
        }
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res bit_and( T1 op1, T2  op2 ) noexcept
    {
        return static_cast<Res>(op1 & op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res bit_or( T1  op1, T2  op2 ) noexcept
    {
        return static_cast<Res>(op1 | op2);
    }

    template< ArithmeticNumber Res, ArithmeticNumber T1, ArithmeticNumber T2 >
    inline static Res bit_xor( T1  op1, T2  op2 ) noexcept
    {
        return static_cast<Res>(op1 ^ op2);
    }

    template< ArithmeticNumber T >
    inline static auto EvalUnaryOp( T const op1, std::string const &op ) noexcept -> T
    {
        if( op == "-" ) [[likely]] {
            return unary_minus( op1 );
        } else /* if( op == "+" ) */ {
            return unary_plus( op1 );
        }
        // throw exception::eval_error( "Internal Error! Unknown Unary Operator!!" );
    }

    //TODO: Split in divide/modulo and rest, where rest can be noexcept then.
    template< ArithmeticNumber T, ArithmeticNumber U >
    inline static auto EvalBinaryOp( T const op1, U const op2, std::string const &op ) -> typename PromoteForCalc<T, U>::Res
    {
        using CalcType1 = typename PromoteForCalc<T, U>::Type1;
        using CalcType2 = typename PromoteForCalc<T, U>::Type2;
        using ResultType = typename PromoteForCalc<T, U>::Res;
        switch( op[0] ) {
        case '+':
            return plus<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case '-':
            return minus<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case '*':
            return multiply<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case '/':
            return divide<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case 'm': // "mod"
            return modulo<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        default:
            //TODO: remove this throw? Must be handled outside
            throw exception::eval_error( "Internal Error! Unknown Binary Operator!!" );
        }
    }

    // bit ops, only for integral!
    template< ArithmeticNumber T, ArithmeticNumber U > requires( not std::is_floating_point_v<T> &&  not std::is_floating_point_v<U> )
    inline static auto EvalBitOp( T const op1, U const op2, std::string const &op ) -> typename PromoteForCalc<T, U>::Res
    {
        using CalcType1 = typename PromoteForCalc<T, U>::Type1;
        using CalcType2 = typename PromoteForCalc<T, U>::Type2;
        using ResultType = typename PromoteForCalc<T, U>::Res;
        switch( op[0] ) {
        case 'a': // (bit) and
            return bit_and<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case 'o': // (bit) or
            return bit_or<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        case 'x': // (bit) xor
            return bit_xor<ResultType>( static_cast<CalcType1>(op1), static_cast<CalcType2>(op2) );
        default:
            //TODO: remove this throw? Must be handled outside
            throw exception::eval_error( "Internal Error! Unknown Binary Operator!!" );
        }
    }
};

/// Applies unary or binary arithmetic operators to one / two given ValueObject(s) by extracting the arithemetic umderlying type or try conversion to long long
/// and returns a new ValueObject with the result.
class ArithmeticFactory
{
    template< ArithmeticNumber T>
    inline static ValueObject ApplyBinaryOpRHS( T const lhs, ValueObject const &o2, std::string const &op )
    {
        switch( o2.InternalType() ) {
        case ValueObject::TypeU8:
            return ValueObject{ArithmeticHelper::EvalBinaryOp( lhs, o2.GetValue<U8>(), op )};
        case ValueObject::TypeI64:
            return ValueObject{ArithmeticHelper::EvalBinaryOp( lhs, o2.GetValue<I64>(), op )};
        case ValueObject::TypeU64:
            return ValueObject{ArithmeticHelper::EvalBinaryOp( lhs, o2.GetValue<U64>(), op )};
        case ValueObject::TypeF64:
            return ValueObject{ArithmeticHelper::EvalBinaryOp( lhs, o2.GetValue<F64>(), op )};
        default:
            return ValueObject{ArithmeticHelper::EvalBinaryOp( lhs, o2.GetAsInteger(), op )};
        }
    }

    // bit ops, only for integral!
    template< ArithmeticNumber T>
    inline static ValueObject ApplyBitRHS( T const lhs, ValueObject const &o2, std::string const &op )
    {
        switch( o2.InternalType() ) {
        case ValueObject::TypeU8:
            return ValueObject{ArithmeticHelper::EvalBitOp( lhs, o2.GetValue<U8>(), op )};
        case ValueObject::TypeI64:
            return ValueObject{ArithmeticHelper::EvalBitOp( lhs, o2.GetValue<I64>(), op )};
        case ValueObject::TypeU64:
            return ValueObject{ArithmeticHelper::EvalBitOp( lhs, o2.GetValue<U64>(), op )};
        default:
            return ValueObject{ArithmeticHelper::EvalBitOp( lhs, o2.GetAsInteger(), op )};
        }
    }

    template< ArithmeticNumber T, ArithmeticNumber U >
    inline static T DoConvert( U const v )
    {
        if constexpr( std::is_floating_point_v<T> || std::is_same_v<T, U> ) {
        } else if constexpr( std::is_floating_point_v<U> ) {
            if( static_cast<U>(std::numeric_limits<T>::max()) < v || static_cast<U>(std::numeric_limits<T>::min()) > v ) {
                throw exception::integer_overflow( v, T{} );
            }
        } else if constexpr( std::is_signed_v<T> && std::is_signed_v<U> ) {
            if( std::numeric_limits<T>::max() < v || std::numeric_limits<T>::min() > v ) {
                throw exception::integer_overflow( v, T{} );
            }
        } else if constexpr( std::is_unsigned_v<T> ) {
            // for the time being allow all values, e.g., we want -1 as u8 == 0xff
        } else { // T is signed, U is unsigned
            if( (static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::max())) < v ) {
                throw exception::integer_overflow( v, T{} );
            }
        }
        return static_cast<T>(v);
    }

    template< ArithmeticNumber T, ArithmeticNumber U > requires( not std::is_floating_point_v<T> && not std::is_floating_point_v<U>)
    inline static std::strong_ordering DoCompare( T const v1, U const u1 ) noexcept
    {
        if constexpr( std::is_same_v<T, U> ) {
            return v1 <=> u1;
        } else if constexpr( std::is_signed_v<T> && std::is_signed_v<U> ) {
            return DoCompare( static_cast<I64>(v1), static_cast<I64>(u1) );
        } else if constexpr( std::is_unsigned_v<T> && std::is_unsigned_v<U> ) {
            return DoCompare( static_cast<U64>(v1), static_cast<U64>(u1) );
        } else if constexpr( std::is_signed_v<T> ) {
            if( v1 < 0 ) {
                return std::strong_ordering::less;
            } else {
                return DoCompare( static_cast<U64>(v1), static_cast<U64>(u1) );
            }
        } else { // std::is_signed_v<U>
            if( u1 < 0 ) {
                return std::strong_ordering::greater;
            } else {
                return DoCompare( static_cast<U64>(v1), static_cast<U64>(u1) );
            }
        }
    }

    template< ArithmeticNumber T >
    inline static std::strong_ordering DoCompareRHS( T const v1, ValueObject const &o2 )
    {
        switch( o2.InternalType() ) {
        case ValueObject::TypeU8:
            return DoCompare( v1, o2.GetValue<U8>() );
        case ValueObject::TypeI64:
            return DoCompare( v1, o2.GetValue<I64>() );
        case ValueObject::TypeU64:
            return DoCompare( v1, o2.GetValue<U64>() );
        //case ValueObject::TypeF64:
        //    return DoCompare( v1, o2.GetValue<F64>() );
        default:
            return DoCompare( v1, o2.GetAsInteger() );
        }
    }

public:
    inline static ValueObject ApplyBinaryOp( ValueObject const &o1, ValueObject const &o2, std::string const &op )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return ApplyBinaryOpRHS( o1.GetValue<U8>(), o2, op );
        case ValueObject::TypeI64:
            return ApplyBinaryOpRHS( o1.GetValue<I64>(), o2, op );
        case ValueObject::TypeU64:
            return ApplyBinaryOpRHS( o1.GetValue<U64>(), o2, op );
        case ValueObject::TypeF64:
            return ApplyBinaryOpRHS( o1.GetValue<F64>(), o2, op );
        default:
            return ApplyBinaryOpRHS( o1.GetAsInteger(), o2, op );
        }
    }

    // bit ops, only for integral!
    inline static ValueObject ApplyBitOp( ValueObject const &o1, ValueObject const &o2, std::string const &op )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return ApplyBitRHS( o1.GetValue<U8>(), o2, op );
        case ValueObject::TypeI64:
            return ApplyBitRHS( o1.GetValue<I64>(), o2, op );
        case ValueObject::TypeU64:
            return ApplyBitRHS( o1.GetValue<U64>(), o2, op );
        default:
            return ApplyBitRHS( o1.GetAsInteger(), o2, op );
        }
    }

    inline static ValueObject ApplyBitshift( ValueObject const &o1, ValueObject const &o2, bool const lsh )
    {
        auto const rhs = ConvertRaw<U8>( o2 );
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            if( rhs >= 8 ) {
                throw exception::out_of_range( "Bitshift value is too big for operand!" );
            }
            return lsh ? ValueObject(static_cast<U8>(o1.GetValue<U8>() << rhs)) : ValueObject(static_cast<U8>(o1.GetValue<U8>() >> rhs));
        case ValueObject::TypeU64:
            if( rhs >= 64 ) {
                throw exception::out_of_range( "Bitshift value is too big for operand!" );
            }
            return lsh ? ValueObject( static_cast<U64>(o1.GetValue<U64>() << rhs) ) : ValueObject( static_cast<U64>(o1.GetValue<U64>() >> rhs) );
        case ValueObject::TypeI64: // Thanks to C++20 we can bitshift signed types!
            if( rhs >= 64 ) {
                throw exception::out_of_range( "Bitshift value is too big for operand!" );
            }
            return lsh ? ValueObject( static_cast<I64>(o1.GetValue<I64>() << rhs) ) : ValueObject( static_cast<I64>(o1.GetValue<I64>() >> rhs) );
        default:
            throw exception::type_mismatch( "Bitshift is only possible for U8, U64 and I64!" );
        }
    }

    inline static ValueObject ApplyUnaryOp( ValueObject const &o1, std::string const &op )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return ValueObject{ArithmeticHelper::EvalUnaryOp( o1.GetValue<U8>(), op )};
        case ValueObject::TypeI64:
            return ValueObject{ArithmeticHelper::EvalUnaryOp( o1.GetValue<I64>(), op )};
        case ValueObject::TypeU64:
            return ValueObject{ArithmeticHelper::EvalUnaryOp( o1.GetValue<U64>(), op )};
        case ValueObject::TypeF64:
            return ValueObject{ArithmeticHelper::EvalUnaryOp( o1.GetValue<F64>(), op )};
        default:
            return ValueObject{ArithmeticHelper::EvalUnaryOp( o1.GetAsInteger(), op )};
        }
    }

    inline static ValueObject ApplyBitNot( ValueObject const &o1 )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return ValueObject{std::bit_not<U8>{}(o1.GetValue<U8>())};
        case ValueObject::TypeI64:
            return ValueObject{std::bit_not<I64>{}(o1.GetValue<I64>())};
        case ValueObject::TypeU64:
            return ValueObject{std::bit_not<U64>{}(o1.GetValue<U64>())};
        default:
            return ValueObject{std::bit_not<I64>{}(o1.GetAsInteger())};
        }
    }

    template< ArithmeticNumber T>
    inline static T ConvertRaw( ValueObject const &o1 )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return DoConvert<T>( o1.GetValue<U8>() );
        case ValueObject::TypeI64:
            return DoConvert<T>( o1.GetValue<I64>() );
        case ValueObject::TypeU64:
            return DoConvert<T>( o1.GetValue<U64>() );
        case ValueObject::TypeF64:
            return DoConvert<T>( o1.GetValue<F64>() );
        default:
            return DoConvert<T>( o1.GetAsInteger() );
        }
    }

    template< ArithmeticNumber T>
    inline static ValueObject Convert( ValueObject const &o1 )
    {
        return ValueObject{ConvertRaw<T>( o1 )};
    }

    
    inline static std::strong_ordering Compare( ValueObject const &o1, ValueObject const &o2 )
    {
        switch( o1.InternalType() ) {
        case ValueObject::TypeU8:
            return DoCompareRHS( o1.GetValue<U8>(), o2 );
        case ValueObject::TypeI64:
            return DoCompareRHS( o1.GetValue<I64>(), o2 );
        case ValueObject::TypeU64:
            return DoCompareRHS( o1.GetValue<U64>(), o2 );
        //case ValueObject::TypeF64:
        //    return DoCompareRHS( o1.GetValue<F64>(), o2 );
        default:
            return DoCompareRHS( o1.GetAsInteger(), o2 );
        }
    }
};


} // namespace util

} // namespace teascript

