/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
 */
#pragma once

#include <typeindex>
#include <bitset>
#include <map>
#include <memory>


namespace teascript {


// Primitive Types of TeaScript
struct NotAValue {}; /// The Not A Value type.
struct Number {};    /// tag for number, acts 'like a concept'
struct Const {};     /// tag for const, acts 'like a concept'
struct Passthrough {}; /// tag for Passthrough data.

using Bool = bool;                      /// The Boolean type.
using I64  = signed long long int;      /// 64 bit signed integer
using U64  = unsigned long long int;    /// 64 bit unsigned integer
using F64  = double;                    /// 64 bit floating point
using String = std::string;             /// The String type.

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
using ArithmenticTypes = type_list< std::int8_t, std::uint8_t, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, I64, U64, float, F64 >;

template< typename T >
inline constexpr bool is_arithmetic_v = is_one_of_v<T, ArithmenticTypes>;

} // namespace util

/// The concept for the valid arithmetic types in TeaScript.
template< typename T>
concept ArithmeticNumber = util::is_arithmetic_v<T>;

/// The type info class for all types represented in a ValueObject.
class TypeInfo
{
    std::string const mName;
    std::type_info const &mTypeInfo; // This will probably be splitted in inner and outer for e.g. shared_ptr<T> and others.
    size_t const mSize;
    bool const mIsArithmetic;
    bool const mIsNaV;
public:
    template< typename T >
    TypeInfo( T const *, std::string const &rName )
        : mName(rName)
        , mTypeInfo(typeid(T))
        , mSize( sizeof(T) )
        , mIsArithmetic( util::is_arithmetic_v<T> )
        , mIsNaV( typeid(T)==typeid(NotAValue) )
    {

    }

    inline std::type_index ToTypeIndex() const noexcept
    {
        return std::type_index( mTypeInfo );
    }

    inline bool IsSame( TypeInfo const &rOther ) const noexcept
    {
        return mTypeInfo == rOther.mTypeInfo;
    }

    inline bool IsSame( std::type_info const &rOther ) const noexcept
    {
        return mTypeInfo == rOther;
    }

    template< typename T>
    inline bool IsSame() const noexcept
    {
        return IsSame( typeid(T) );
    }

    inline std::string const &GetName() const noexcept
    {
        return mName;
    }

    inline size_t GetSize() const noexcept
    {
        return mSize;
    }

    inline bool IsNaV() const noexcept
    {
        return mIsNaV;
    }

    inline bool IsArithmetic() const noexcept
    {
        return mIsArithmetic;
    }

};


template< typename T >
inline
TypeInfo MakeTypeInfo( std::string const &rName, T const * dummy = nullptr ) noexcept
{
    return TypeInfo( dummy, rName );
}


// the primitive types are always there, even without lookup in TypeSystem.
static TypeInfo const TypeNaV = MakeTypeInfo<NotAValue>("NaV");
static TypeInfo const TypeBool = MakeTypeInfo<Bool>("Bool");
static TypeInfo const TypeString = MakeTypeInfo<String>("String");
static TypeInfo const TypeLongLong = MakeTypeInfo<I64>("i64");
static TypeInfo const TypeDouble = MakeTypeInfo<F64>("f64");
static TypeInfo const TypeTypeInfo = MakeTypeInfo<TypeInfo>("TypeInfo");
static TypeInfo const TypePassthrough = MakeTypeInfo<Passthrough>( "Passthrough" );

/// helper class for store TypeInfo instances either as unique_ptr or a as raw pointer (for static instances).
class TypePtr
{
    std::unique_ptr< TypeInfo const > mUPtr;
    TypeInfo const *mPtr;
public:
    inline
    TypePtr() noexcept
        : mUPtr()
        , mPtr( nullptr )
    {
    }

    inline
    TypePtr( std::unique_ptr< TypeInfo const > u ) noexcept
        : mUPtr( std::move(u) )
        , mPtr(mUPtr.get())
    {
    }

    inline
    TypePtr( TypeInfo const *p  ) noexcept
        : mUPtr()
        , mPtr( p )
    {
    }

    TypeInfo const *GetPtr() const noexcept
    {
        return mPtr;
    }
};

/// Properties of a type, which might be changeable.
class TypeProperties
{
    std::bitset<64> mProps;
public:
    inline TypeProperties() noexcept
        : mProps( 0 )
    {
    }

    inline explicit TypeProperties( bool const is_const, bool const type_allocated = false ) noexcept
        : TypeProperties()
    {
        if( is_const ) {
            MakeConst();
        }
        if( type_allocated ) {
            SetTypeAllocated( true );
        }
    }

    bool IsConst() const noexcept
    {
        return mProps.test( 0 );
    }

    bool IsMutable() const noexcept
    {
        return not IsConst();
    }

    void MakeConst() noexcept
    {
        mProps.set( 0 );
    }

    void MakeMutable() noexcept
    {
        mProps.reset( 0 );
    }

    bool IsTypeAllocated() const noexcept
    {
        return mProps.test( 63 );
    }

    void SetTypeAllocated( bool const set ) noexcept
    {
        mProps.set( 63, set );
    }
};

/// Concept for types which can be registered. We want only plain pure types.
template< typename T>
concept RegisterableType = !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_void_v<T>;


/// Helper function to create a new TypeInfo in a unique pointer for a given type.
template< RegisterableType T>
inline
auto MakeUniqueTypeInfo( std::string const &rName ) -> std::unique_ptr<TypeInfo const>
{
    return std::make_unique< TypeInfo const >( MakeTypeInfo< T >(rName) );
}


/// TypeSystem class to store all registered types.
class TypeSystem
{
    std::map<std::type_index, TypePtr const> mTypes;

public:
    TypeSystem()
    {
        mTypes.insert( std::make_pair( TypeNaV.ToTypeIndex(), TypePtr( &TypeNaV ) ) );
        mTypes.insert( std::make_pair( TypeBool.ToTypeIndex(), TypePtr( &TypeBool ) ) );
        mTypes.insert( std::make_pair( TypeString.ToTypeIndex(), TypePtr( &TypeString ) ) );
        mTypes.insert( std::make_pair( TypeLongLong.ToTypeIndex(), TypePtr( &TypeLongLong ) ) );
        mTypes.insert( std::make_pair( TypeDouble.ToTypeIndex(), TypePtr( &TypeDouble ) ) );
        mTypes.insert( std::make_pair( TypeTypeInfo.ToTypeIndex(), TypePtr( &TypeTypeInfo ) ) );
        mTypes.insert( std::make_pair( TypePassthrough.ToTypeIndex(), TypePtr( &TypePassthrough ) ) );
    }

    template< RegisterableType T>
    void RegisterType( std::string const &rName )
    {
        if( mTypes.contains( std::type_index( typeid(T) ) ) ) {
            return;
        }
        auto uptr = MakeUniqueTypeInfo<T>(rName);
        mTypes.insert( std::make_pair( std::type_index( typeid(T) ), TypePtr( std::move( uptr ) ) ) );
    }

    template< RegisterableType T>
    TypeInfo const * Find() const
    {
        return Find( std::type_index( typeid(T) ) );
    }

    
    TypeInfo const * Find( std::type_index const &idx ) const
    {
        auto res = mTypes.find( idx );
        if( res != mTypes.end() ) {
            return res->second.GetPtr();
        }
        return nullptr;
    }
};



} // namespace teascript
