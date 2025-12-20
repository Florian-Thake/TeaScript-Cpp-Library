/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Types.hpp"
#include "TypeInfo.hpp"
#include "IntegerSequence.hpp"
#include "Error.hpp"

#include <bitset>
#include <map>
#include <memory>


namespace teascript {

// the primitive types are always there, even without lookup in TypeSystem.
static TypeInfo const TypeNaV = MakeTypeInfo<NotAValue>("NaV");
static TypeInfo const TypeBool = MakeTypeInfo<Bool>("Bool");
static TypeInfo const TypeString = MakeTypeInfo<String>("String");
static TypeInfo const TypeU8 = MakeTypeInfo<U8>( "u8" );
static TypeInfo const TypeLongLong = MakeTypeInfo<I64>("i64");
static TypeInfo const TypeU64 = MakeTypeInfo<U64>( "u64" );
static TypeInfo const TypeDouble = MakeTypeInfo<F64>("f64");
static TypeInfo const TypeTypeInfo = MakeTypeInfo<TypeInfo>("TypeInfo");
static TypeInfo const TypePassthrough = MakeTypeInfo<Passthrough>( "Passthrough" );
static TypeInfo const TypeIntegerSequence = MakeTypeInfo<IntegerSequence>( "IntegerSequence" );
static TypeInfo const TypeBuffer = MakeTypeInfo<Buffer>( "Buffer" );
static TypeInfo const TypeError = MakeTypeInfo<Error>( "Error" );

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
        mTypes.insert( std::make_pair( TypeU8.ToTypeIndex(), TypePtr( &TypeU8 ) ) );
        mTypes.insert( std::make_pair( TypeLongLong.ToTypeIndex(), TypePtr( &TypeLongLong ) ) );
        mTypes.insert( std::make_pair( TypeU64.ToTypeIndex(), TypePtr( &TypeU64 ) ) );
        mTypes.insert( std::make_pair( TypeDouble.ToTypeIndex(), TypePtr( &TypeDouble ) ) );
        mTypes.insert( std::make_pair( TypeTypeInfo.ToTypeIndex(), TypePtr( &TypeTypeInfo ) ) );
        mTypes.insert( std::make_pair( TypePassthrough.ToTypeIndex(), TypePtr( &TypePassthrough ) ) );
        mTypes.insert( std::make_pair( TypeIntegerSequence.ToTypeIndex(), TypePtr( &TypeIntegerSequence ) ) );
        mTypes.insert( std::make_pair( TypeBuffer.ToTypeIndex(), TypePtr( &TypeBuffer ) ) );
        mTypes.insert( std::make_pair( TypeError.ToTypeIndex(), TypePtr( &TypeError ) ) );
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
