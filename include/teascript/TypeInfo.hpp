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

#include <typeindex>
#include <memory>

namespace teascript {

/// The type info class for all types represented in a ValueObject.
class TypeInfo
{
    std::string const mName;
    std::type_info const &mTypeInfo; // This will probably be splitted in inner and outer for e.g. shared_ptr<T> and others.
    size_t const mSize;
    bool const mIsArithmetic;
    bool const mIsSigned;
    bool const mIsNaV;
public:
    template< typename T >
    TypeInfo( T const *, std::string const &rName )
        : mName( rName )
        , mTypeInfo( typeid(T) )
        , mSize( sizeof( T ) )
        , mIsArithmetic( util::is_arithmetic_v<T> )
        , mIsSigned( mIsArithmetic && std::is_signed_v<T> )
        , mIsNaV( typeid(T) == typeid(NotAValue) )
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

    inline bool IsSigned() const noexcept
    {
        return mIsSigned;
    }

};


template< typename T >
inline
TypeInfo MakeTypeInfo( std::string const &rName, T const *dummy = nullptr ) noexcept
{
    return TypeInfo( dummy, rName );
}

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
        : mUPtr( std::move( u ) )
        , mPtr( mUPtr.get() )
    {
    }

    inline
        TypePtr( TypeInfo const *p ) noexcept
        : mUPtr()
        , mPtr( p )
    {
    }

    TypeInfo const *GetPtr() const noexcept
    {
        return mPtr;
    }
};

} // namespace teascript
