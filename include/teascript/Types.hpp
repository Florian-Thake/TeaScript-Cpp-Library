/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <string>
#include <vector>


namespace teascript {


// Primitive Types of TeaScript
struct NotAValue {}; /// The Not A Value type.
struct Number {};    /// tag for number, acts 'like a concept'
struct Const {};     /// tag for const, acts 'like a concept'
struct Passthrough {}; /// tag for Passthrough data.

using Bool = bool;                      /// The Boolean type.
using Byte = unsigned char;             /// 8 bit byte.
using U8   = Byte;                      /// 8 bit unsigned integer
using I64  = signed long long int;      /// 64 bit signed integer
using U64  = unsigned long long int;    /// 64 bit unsigned integer
using F64  = double;                    /// 64 bit floating point
using String = std::string;             /// The String type.
using Buffer = std::vector<Byte>;       /// The Buffer type (for raw binary data).

using Integer = I64;                    /// Default type for integers
using Decimal = F64;                    /// Default type for decimal numbers


namespace util {

/// our type list. Just takes and store every type. empty lists not allowed.
template<typename T0, typename ...TN>
struct type_list
{
    static constexpr size_t size = (sizeof...(TN)) + 1;
};

/// tests if a type is present in a list of types / type_list.
template<typename ...>
struct is_one_of
{
    static constexpr bool value = false;
};

template<typename T, typename K0, typename ...KN>
struct is_one_of<T, K0, KN...>
{
    static constexpr bool value = std::is_same_v<T, K0> || is_one_of<T, KN...>::value;
};

template<typename T, typename K0, typename ...KN>
struct is_one_of<T, type_list<K0, KN...> >
{
    static constexpr bool value = is_one_of<T, K0, KN...>::value;
};

/// true if T is present in the type_list or list of types K0...KN, false otherwise.
template<typename T, typename K0, typename ...KN>
inline constexpr bool is_one_of_v = is_one_of<T, K0, KN...>::value;


/// picks the index of the type inside a type_list. The type _must_ be present!
template< typename, typename >
struct index_of; // error

template< typename T, typename ...TN>
struct index_of< T, type_list<T, TN...> >
{
    static constexpr size_t value = 0;
};

template< typename T, typename TX, typename ...TN>
struct index_of< T, type_list<TX, TN...> >
{
    static constexpr size_t value = 1 + index_of<T, type_list<TN...> >::value;
};

/// These are the valid types for arithmetic operations. We only use 'real' numbers and dissallow all character like types and bool.
using ArithmenticTypes = type_list< std::int8_t, U8, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, I64, U64, float, F64 >;

template< typename T >
inline constexpr bool is_arithmetic_v = is_one_of_v<T, ArithmenticTypes>;

} // namespace util


/// The concept for the valid arithmetic types in TeaScript.
template< typename T>
concept ArithmeticNumber = util::is_arithmetic_v<T>;


/// Concept for types which can be registered. We want only plain pure types.
template< typename T>
concept RegisterableType = !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_void_v<T>;


} // namespace teascript
