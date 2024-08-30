/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <cassert>

#include "Exception.hpp"

// define this for disable using of Boost header only libraries.
// NOTE: currently disabled by default due to performance penalty which needs to be investigated.
#define TEASCRIPT_DISABLE_BOOST     1

#if __has_include( "boost/unordered_map.hpp") && !defined( TEASCRIPT_DISABLE_BOOST )
# define TEASCRIPT_USE_BOOST_CONTAINERS     1
#else
# define TEASCRIPT_USE_BOOST_CONTAINERS     0
#endif

#if TEASCRIPT_USE_BOOST_CONTAINERS
# include "boost/unordered_map.hpp"
# include "boost/container/map.hpp"
#endif

namespace teascript {


namespace col_policy {
enum class eOrder
{
    Ordered,
    Unordered
};
} // namespace col_policy


/// Container class with stable storage order, implements LIFO and provides
/// access by index as well as by a key (optionally). The complexity is comparable
/// with std::vector but removing elements others than the last adds some 
/// extra complexity on top if keys are used due to maintaining the access by key.
template< typename V, typename K = std::string, col_policy::eOrder order = col_policy::eOrder::Ordered >
class Collection
{
    static_assert( order == col_policy::eOrder::Ordered || order == col_policy::eOrder::Unordered, "Must be Ordered or Unordered!" );

public:
    using ValueType = V;
    using KeyType = K;

    using KeyValue = std::pair< KeyType, ValueType >;

    using StorageType = std::vector< KeyValue >; // first in, last out

#if TEASCRIPT_USE_BOOST_CONTAINERS
    using LookupType = std::conditional_t< order == col_policy::eOrder::Ordered,
                                           boost::container::map< KeyType, std::size_t >,
                                           boost::unordered_map<KeyType, std::size_t > >; // for quick access.
#else
    using LookupType = std::conditional_t< order == col_policy::eOrder::Ordered, 
                                           std::map< KeyType, std::size_t >, 
                                           std::unordered_map<KeyType, std::size_t > >; // for quick access.
#endif

    /// npos for indicating an invalid index
    inline static constexpr std::size_t npos = static_cast<std::size_t>(-1);

protected:
    StorageType  mStorage;
    LookupType   mLookup;

public:
    Collection() = default;
    Collection( Collection const & ) = default;
    Collection &operator=( Collection const & ) = default;
    Collection( Collection && ) = default;
    Collection &operator=( Collection  && ) = default;

    ~Collection()
    {
        Clear();
    }

    void Clear()
    {
        mLookup.clear();
        while( !mStorage.empty() ) {
            mStorage.pop_back();
        }
    }

    inline
    void Reserve( std::size_t const size )
    {
        mStorage.reserve( size );
    }

    inline
    std::size_t Size() const noexcept
    {
        return mStorage.size();
    }

    inline
    bool IsEmpty() const noexcept
    {
        return mStorage.empty();
    }

    inline
    bool ContainsIdx( std::size_t const idx ) const noexcept // convenience
    {
        return idx < Size();
    }

    inline
    bool ContainsKey( KeyType const &rKey ) const noexcept
    {
        return mLookup.find( rKey ) != mLookup.end();
    }

    // note: only clang wants these 'typename's

    inline
    typename StorageType::iterator begin() noexcept
    {
        return mStorage.begin();
    }

    inline
    typename StorageType::iterator end() noexcept
    {
        return mStorage.end();
    }

    inline
    typename StorageType::const_iterator begin() const noexcept
    {
        return mStorage.begin();
    }

    inline
    typename StorageType::const_iterator end() const noexcept
    {
        return mStorage.end();
    }

    std::size_t IndexOfKey( KeyType const &rKey ) const noexcept
    {
        auto it = mLookup.find( rKey );
        if( it == mLookup.end() ) {
            return npos;
        }
        assert( ContainsIdx( it->second ) );
        return it->second;
    }

    KeyType KeyOfIndex( std::size_t const idx ) const
    {
        if( not ContainsIdx( idx ) ) {
            throw exception::out_of_range( "Collection: Invalid index!" );
        }

        return mStorage[idx].first; // NOTE: With this we cannot distinguish between empty key in mLookup or default KeyType() without entry in mLookup !!!
    }

    void AppendValue( ValueType const &rVal )
    {
        mStorage.emplace_back( KeyType(), rVal );
    }

    bool AppendKeyValue( KeyType const &rKey, ValueType const &rVal )
    {
        if( not mLookup.emplace( rKey, mStorage.size() ).second ) {
            return false;
        }
        mStorage.emplace_back( rKey, rVal );
        return true;
    }

    inline
    ValueType const &GetValueByIdx_Unchecked( std::size_t const idx ) const noexcept
    {
        return mStorage[idx].second;
    }

    inline
    ValueType &GetValueByIdx_Unchecked( std::size_t const idx ) noexcept
    {
        return mStorage[idx].second;
    }

    ValueType const &GetValueByIdx( std::size_t const idx ) const
    {
        if( ContainsIdx( idx ) ) {
            return GetValueByIdx_Unchecked( idx );
        }
        throw exception::out_of_range( "Collection: Invalid index!" );
    }

    ValueType & GetValueByIdx( std::size_t const idx )
    {
        // one of the rare unevil const_cast: first make this const to can re-use the const code, then remove the const again from result.
        return const_cast<ValueType &>(const_cast<Collection const &>(*this).GetValueByIdx( idx ));
    }

    ValueType const &GetValueByKey( KeyType const &rKey ) const
    {
        auto it = mLookup.find( rKey );
        if( it != mLookup.end() ) {
            assert( ContainsIdx( it->second ) );
            return GetValueByIdx_Unchecked( it->second );
        }
        throw exception::out_of_range( "Collection: Invalid key! Key not found!" );
    }

    ValueType & GetValueByKey( KeyType const &rKey )
    {
        // one of the rare unevil const_cast: first make this const to can re-use the const code, then remove the const again from result.
        return const_cast<ValueType &>(const_cast<Collection const &>(*this).GetValueByKey( rKey ));
    }

    /// Subscript operator [] for index based access. As for std::vector it is undefined behavior if idx is out of range.
    inline
    ValueType const &operator[]( std::size_t const idx ) const noexcept
    {
        return mStorage[idx].second;
    }

    /// Subscript operator [] for index based access. As for std::vector it is undefined behavior if idx is out of range.
    inline
    ValueType &operator[]( std::size_t const idx ) noexcept
    {
        return mStorage[idx].second;
    }

    /// Subscript operator [] for key based access. Unlike std::map this operator will _not_ create a missing key / value!
    /// IMPORTANT: A call of this operator for an unpresent key will result in undefined behavior!
    inline
    ValueType const &operator[]( KeyType const &rKey ) const noexcept
    {
        return GetValueByIdx_Unchecked( mLookup.find( rKey )->second );
    }

    /// Subscript operator [] for key based access. Unlike std::map this operator will _not_ create a missing key / value!
    /// IMPORTANT: A call of this operator for an unpresent key will result in undefined behavior!
    inline
    ValueType &operator[]( KeyType const &rKey ) noexcept
    {
        return GetValueByIdx_Unchecked( mLookup.find( rKey )->second );
    }

protected:
    bool RemoveValue( std::size_t const idx, typename LookupType::iterator const &map_it ) noexcept
    {
        if( map_it != mLookup.end() ) {
            mLookup.erase( map_it );
        }

        mStorage.erase( mStorage.begin() + idx );

        // if it was the last element, there cannot be indices which needs to be adjusted. bail out early.
        if( mStorage.size() == idx ) {
            return true;
        }

        // we must adjust all indices which are behind the removed object.
        for( auto it = mLookup.begin(); it != mLookup.end(); ++it ) {
            if( it->second > idx ) {
                --it->second;
            }
        }

        return true;

    }

public:
    bool RemoveValueByIdx( std::size_t const idx ) noexcept
    {
        if( not ContainsIdx( idx ) ) {
            return false;
        }

        KeyType const key = mStorage[idx].first;
        return RemoveValue( idx, mLookup.find( key ) );
        
    }

    bool RemoveValueByKey( KeyType const &rKey ) noexcept
    {
        auto it = mLookup.find( rKey );
        if( it == mLookup.end() ) {
            return false;
        }
        assert( ContainsIdx( it->second ) );
        return RemoveValue( it->second, it );
    }

    /// This function might be useful when speed is preferred a lot over memory consumption.
    /// The key \param rKey will be removed from lookup but the value in the storage will only be replaced by the given \param rVal.
    /// With that the storage will stay stable and a lookup data update is not needed. But the storage never shrinks, only grows!
    /// \return the original value or std::nullopt.
    /// \note when iterating or access by index the placeholders are present/visible. The user of this class must handle it.
    std::optional<ValueType> RemveValueByKeyWithPlaceholder( KeyType const rKey, ValueType const &rVal ) noexcept
    {
        auto it = mLookup.find( rKey );
        if( it == mLookup.end() ) {
            return std::nullopt;
        }
        assert( ContainsIdx( it->second ) );
        auto val = mStorage[it->second].second;

        mStorage[it->second].second = rVal;
        mStorage[it->second].first = KeyType();
        mLookup.erase( it );

        return val;
    }

    void InsertValue( std::size_t const idx, ValueType const &rVal )
    {
        if( idx > Size() ) {
            throw exception::out_of_range();
        }
        if( idx == Size() ) {
            AppendValue( rVal );
            return;
        }

        mStorage.insert( mStorage.begin() + idx, std::make_pair( KeyType(), rVal ) );
        // we must adjust all indices which are behind or equal the idx
        for( auto it = mLookup.begin(); it != mLookup.end(); ++it ) {
            if( it->second >= idx ) {
                ++it->second;
            }
        }
    }

    void InsertKeyValue( std::size_t const idx, KeyType const &rKey, ValueType const &rVal )
    {
        if( idx > Size() ) {
            throw exception::out_of_range();
        }
        if( idx == Size() ) {
            AppendKeyValue( rKey, rVal );
            return;
        }
        mStorage.insert( mStorage.begin() + idx, std::make_pair( rKey, rVal ) );
        // we must adjust all indices which are behind or equal the idx
        for( auto it = mLookup.begin(); it != mLookup.end(); ++it ) {
            if( it->second >= idx ) {
                ++it->second;
            }
        }
        mLookup.emplace( std::make_pair( rKey, idx ) );
    }

    void SwapByIdx( std::size_t const idx1, std::size_t const idx2 )
    {
        if( not ContainsIdx( idx1 ) || not ContainsIdx( idx2 ) ) {
            throw exception::out_of_range();
        }

        std::swap( mStorage[idx1], mStorage[idx2] );

        // adjust all indices for idx1 and idx2 as well (bruteforce for now)
        for( auto it = mLookup.begin(); it != mLookup.end(); ++it ) {
            if( it->second == idx1 ) {
                it->second = idx2;
            } else if( it->second == idx2 ) {
                it->second = idx1;
            }
        }
    }
};


} // nmespace teascript

