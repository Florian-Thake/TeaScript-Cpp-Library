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

#include <string>
#include <vector>
#include <map>
#include <cassert>

#include "Exception.hpp"


namespace teascript {

// TODO: Make this a replacement for ValueObject storage in Context ?
/// Container class with stable storage order, implements LIFO and provides
/// access by index as well as by a key (optionally). The complexity is comparable
/// with std::vector but removing elements others than the last adds some 
/// extra complexity on top if keys are used due to maintaining the access by key.
template< typename V, typename K = std::string >
class Collection
{
public:
    using ValueType = V;
    using KeyType = K;

    using KeyValue = std::pair< KeyType, ValueType >;

    using StorageType = std::vector< KeyValue >; // first in, last out

    using LookupType = std::map< KeyType, std::size_t >; // for quick access.

protected:
    StorageType  mStorage;
    LookupType   mLookup;

public:
    Collection() = default;
    ~Collection()
    {
        Clear();
    }

    void Clear()
    {
        mLookup.clear();
        //TODO: (future) Need to lookup registered Destructor functions and call them!
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
            return static_cast<std::size_t>(-1); //npos
        }
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
        if( mLookup.find( rKey ) != mLookup.end() ) {
            return false;
        }
        mStorage.emplace_back( rKey, rVal );
        mLookup.emplace( std::make_pair( rKey, mStorage.size()-1u ) );
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

