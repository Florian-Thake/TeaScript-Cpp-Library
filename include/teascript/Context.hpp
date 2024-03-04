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
#include "Print.hpp"
#include "Util.hpp"
#include "UtilInternal.hpp"
#include "Dialect.hpp"

#include <vector>
#include <queue>
#include <utility> // std::pair
#include <string>
#include <memory>


#ifndef TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
# define TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE     1
#endif


#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
# include "Collection.hpp"
#else
# include <unordered_map>
#endif


namespace teascript {

/// The context for TeaScript script/code execution.
/// \warning This class and especially the class layout and all(!) data members are subject of heavy changes and are _not_ stable!
class Context
{
public:
    using ObjectType = ValueObject;

    //TODO [ITEM 96] Refactor the internal storage layout and lookup for variables and scopes.

#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
    using VariableCollection = Collection<ObjectType, std::string, col_policy::eOrder::Unordered >; // first in, last out and quick access.

    // leftover, is still needed for 'bulk add'
    using VariableStorage = VariableCollection::StorageType;
#else
    using KeyValue        = std::pair< std::string, ObjectType >;
    using VariableStorage = std::vector< KeyValue >; // first in, last out
    using VariableLookup  = std::unordered_map< std::string, ObjectType >; // for quick access.
#endif

    using ParameterList = std::queue<ObjectType>;  // FIFO, for consuming parameters of function calls.

    class Scope
    {
    public:
        Scope() = default;
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        VariableCollection  mVariableCollection;
#else
        VariableStorage  mVariableStorage;
        VariableLookup   mVariableLookup;
#endif

        ParameterList    mCurrentParamList;
        SourceLocation   mCurrentLoc;

        void Cleanup()
        {
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
            //TODO: (future) Need to lookup registered Destructor functions and call them!
            mVariableCollection.Clear();
#endif
            {
                ParameterList  empty;
                mCurrentParamList.swap( empty );
            }
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
#else
            mVariableLookup.clear();
            //TODO: (future) Need to lookup registered Destructor functions and call them!
            while( !mVariableStorage.empty() ) {
                mVariableStorage.pop_back();
            }
#endif      
        }

        ~Scope()
        {
            Cleanup();
        }
    };

private:
    bool mBootstrapped = true;

    TypeSystem mTypeSystem; // Better be a shared ptr?
    //TODO: THREAD Have a shared global scope (optionally) for multi-threaded environments?
    //      then the local scopes could use "per thread" storage? or alternatively 
    //      mGlobaleScopes is shared_ptr and Context is "per thread" ... or ....
    Scope mGlobalScope;

    std::vector<Scope> mLocalScopes;

    Scope &GetCurrentScope()
    {
        return mLocalScopes.empty() ? mGlobalScope : mLocalScopes.back();
    }

    Scope const &GetCurrentScope() const
    {
        return mLocalScopes.empty() ? mGlobalScope : mLocalScopes.back();
    }

    void CheckName( std::string const &rName, SourceLocation const &rLoc = {} ) const
    {
        if( mBootstrapped && rName.starts_with( '_' ) ) {
            throw exception::internal_name( rLoc );
        }
    }

    // note: only for transition for have a private non deprecated variant which can be called without error.
    //       this function is for INTERNAL only!
    void BulkAdd_Impl( VariableStorage const &init )
    {
        if( mBootstrapped ) {
            return;
        }
        for( auto const &kv : init ) {
            if( kv.second.IsShared() ) {
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
                // NOTE: new AppendKeyValue should be same complexity as unconditional insert of the old code (only one lookup during emplace)
                mGlobalScope.mVariableCollection.AppendKeyValue( kv.first, kv.second );
#else
                mGlobalScope.mVariableStorage.push_back( kv );
                mGlobalScope.mVariableLookup.insert( kv );
#endif
            }
        }
    }

public:
    Dialect  dialect; // TeaScipt language behavior. (default is TeaScript standard language)  NOTE: The existence/public existence may change in future!

    bool is_debug = false; // from and for parser. TODO: ASTNodeFactory (integrate Parser in Context?? no, better try to keep Parser and Context unrelated!!)

    Context() = default;
    Context( Context && ) = default;
    Context &operator=( Context && ) = default;


    explicit Context( TypeSystem &&rMovedSys, bool const booting = false )
        : mBootstrapped( not booting )
        , mTypeSystem( std::move( rMovedSys ) )
    {
    }

#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
    [[deprecated( "Please, use a different constructor instead." )]]
#endif
    Context( VariableStorage const &init, TypeSystem && rMovedSys, bool const booting = false )
        : mBootstrapped( not booting )
        , mTypeSystem( std::move(rMovedSys) )

    {
        BulkAdd_Impl( init ); // must call not deprecated variant for avoid errors
    }

    ~Context()
    {
        while( !mLocalScopes.empty() ) {
            mLocalScopes.pop_back();
        }
    }

    void SetBootstrapDone()
    {
        mBootstrapped = true;
    }

#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
#if 0  // was for testing
    void Reserve( std::size_t num )
    {
        if( mBootstrapped ) {
            return;
        }
        mGlobalScope.mVariableCollection.Reserve( num );
    }
#endif

    /// moves the variable collection into the global scope, all prior vars will be lost.
    /// only do something during bootstrapping, otherwise a no-op. 
    void InjectVars( VariableCollection && col )
    {
        if( mBootstrapped ) {
            return;
        }
        mGlobalScope.mVariableCollection = std::move( col );
    }
#endif


#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
    [[deprecated( "Please, use InjectVars() instead." )]]
#endif
    void BulkAdd( VariableStorage const &init )
    {
        BulkAdd_Impl( init );
    }

    void SetScriptArgs( std::vector<std::string> const &rArgs )
    {
        //TODO: add arg0 as 'main script name' ?!
        int i = 0;
        for( auto const &arg : rArgs ) {
            //TODO: check if it is a pure int, double or bool and then create a specialized ValueObject ?
            AddValueObject( "arg" + std::to_string( ++i ), ValueObject( arg, true ) );
        }
        AddValueObject( "argN", ValueObject( static_cast<long long>(i), true ) );
    }

    TypeSystem &GetTypeSystem()
    {
        return mTypeSystem;
    }

    TypeSystem const &GetTypeSystem() const
    {
        return mTypeSystem;
    }

    ObjectType FindValueObject( std::string const &rName, SourceLocation const &rLoc = {}, long long *pScopeLevel = nullptr ) const
    {
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        // all internal names can only occur in the global scope!
        if( not rName.starts_with( "_" ) ) {
            for( auto it = mLocalScopes.rbegin(); it != mLocalScopes.rend(); ++it ) {
                if( auto idx = it->mVariableCollection.IndexOfKey( rName ); idx != VariableCollection::npos ) {
                    if( pScopeLevel ) {
                        *pScopeLevel = std::distance( mLocalScopes.rbegin(), it ) + 1LL;
                    }
                    return it->mVariableCollection.GetValueByIdx_Unchecked( idx );
                }
            }
        }

        if( auto idx = mGlobalScope.mVariableCollection.IndexOfKey( rName ); idx != VariableCollection::npos ) {
            if( pScopeLevel ) {
                *pScopeLevel = static_cast<long long>(mLocalScopes.size()) + 1LL;
            }
            return mGlobalScope.mVariableCollection.GetValueByIdx_Unchecked( idx );
        }
#else
        for( auto it = mLocalScopes.rbegin(); it != mLocalScopes.rend(); ++it ) {
            if( auto map_it = it->mVariableLookup.find( rName ); map_it != it->mVariableLookup.end() ) {
                if( pScopeLevel ) {
                    *pScopeLevel = std::distance( mLocalScopes.rbegin(), it ) + 1LL;
                }
                return map_it->second;
            }
        }
        if( auto map_it = mGlobalScope.mVariableLookup.find( rName ); map_it != mGlobalScope.mVariableLookup.end() ) {
            if( pScopeLevel ) {
                *pScopeLevel = static_cast<long long>(mLocalScopes.size()) + 1LL;
            }
            return map_it->second;
        }
#endif
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType AddValueObject( std::string const &rName, ValueObject const &rValue, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        // only search in the most recent scope...
        Scope &scope = GetCurrentScope();
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        if( rValue.IsShared() ) { //TODO: maybe this can be relaxed when the lookup does not safe a copy but an index to the storage or sth. similar (but shared assign is special then!)
            if( not scope.mVariableCollection.AppendKeyValue( rName, rValue ) ) {
                throw exception::redefinition_of_variable( rLoc, rName );
            }
            // new object is always last position.
            return scope.mVariableCollection[scope.mVariableCollection.Size() - 1u];
        }
#else
        if( auto map_it = scope.mVariableLookup.find( rName ); map_it != scope.mVariableLookup.end() ) {
            throw exception::redefinition_of_variable( rLoc, rName );
        }
        if( rValue.IsShared() ) { //TODO: maybe this can be relaxed when the lookup does not safe a copy but an index to the storage or sth. similar (but shared assign is special then!)
            scope.mVariableStorage.push_back( std::make_pair( rName, rValue ) );
            scope.mVariableLookup.insert( std::make_pair( rName, rValue ) );
            return scope.mVariableStorage.back().second;
        }
#endif
        throw exception::runtime_error( rLoc, "Internal Error! ValueObject must be shared for add it!" );
    }

    ObjectType RemoveValueObject( std::string const &rName, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        // for now only in the current scope.
        //TODO: check if outer scopes shall be considered as well!
        Scope &scope = GetCurrentScope();
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        auto res = scope.mVariableCollection.RemveValueByKeyWithPlaceholder( rName, ValueObject() );
        if( res.has_value() ) {
            return res.value();
        }
#else
        if( scope.mVariableLookup.erase(rName) != 0 ) {
            auto it = std::find_if( scope.mVariableStorage.begin(), scope.mVariableStorage.end(), [&](auto const &e) {
                return e.first == rName;
            } );
            auto res = it->second;
            scope.mVariableStorage.erase( it );
            return res;
        }
#endif
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType SetValue( std::string const &rName, ValueObject const &rValue, bool const shared, SourceLocation const &rLoc = {} )
    {
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        for( auto it = mLocalScopes.rbegin(); it != mLocalScopes.rend(); ++it ) {
            if( auto idx = it->mVariableCollection.IndexOfKey( rName ); idx != VariableCollection::npos ) {
                if( shared && rValue.IsShared() ) {
                    it->mVariableCollection.GetValueByIdx_Unchecked( idx ).SharedAssignValue( rValue, rLoc );
                } else {
                    it->mVariableCollection.GetValueByIdx_Unchecked( idx ).AssignValue( rValue, rLoc );
                }
                return it->mVariableCollection.GetValueByIdx_Unchecked( idx );
            }
        }

        if( auto idx = mGlobalScope.mVariableCollection.IndexOfKey( rName ); idx != VariableCollection::npos ) {
            if( shared && rValue.IsShared() ) {
                mGlobalScope.mVariableCollection.GetValueByIdx_Unchecked( idx ).SharedAssignValue( rValue, rLoc );
            } else {
                mGlobalScope.mVariableCollection.GetValueByIdx_Unchecked( idx ).AssignValue( rValue, rLoc );
            }
            return mGlobalScope.mVariableCollection.GetValueByIdx_Unchecked( idx );
        }
#else
        for( auto it = mLocalScopes.rbegin(); it != mLocalScopes.rend(); ++it ) {
            if( auto map_it = it->mVariableLookup.find( rName ); map_it != it->mVariableLookup.end() ) {
                if( shared && rValue.IsShared() ) {
                    map_it->second.SharedAssignValue( rValue, rLoc );
                    auto storage_it = std::find_if( it->mVariableStorage.begin(), it->mVariableStorage.end(), [&]( auto const &e ) {
                        return e.first == rName;
                    } );
                    storage_it->second = map_it->second;
                } else {
                    map_it->second.AssignValue( rValue, rLoc );
                }
                return map_it->second;
            }
        }
        if( auto map_it = mGlobalScope.mVariableLookup.find( rName ); map_it != mGlobalScope.mVariableLookup.end() ) {
            if( shared && rValue.IsShared() ) {
                map_it->second.SharedAssignValue( rValue, rLoc );
                auto storage_it = std::find_if( mGlobalScope.mVariableStorage.begin(), mGlobalScope.mVariableStorage.end(), [&]( auto const &e ) {
                    return e.first == rName;
                } );
                storage_it->second = map_it->second;
            } else {
                map_it->second.AssignValue( rValue, rLoc );
            }
            return map_it->second;
        }
#endif

        throw exception::unknown_identifier( rLoc, rName );
    }

    void EnterScope()
    {
        mLocalScopes.emplace_back();
    }

    void ExitScope()
    {
        if( mLocalScopes.empty() ) {
            throw exception::runtime_error( "Internal Error! ExitScope() with empty local scopes!" );
        }
        mLocalScopes.pop_back();
    }

    void SetParamList( std::vector<ValueObject> const &paramlist )
    {
        //TODO [ITEM 97] Optimise setting of parameter list
        GetCurrentScope().mCurrentParamList = ParameterList{ std::deque<ObjectType>{ paramlist.begin(), paramlist.end() } };
    }

    size_t CurrentParamCount() const
    {
        return GetCurrentScope().mCurrentParamList.size();
    }

    ValueObject ConsumeParam()
    {
        Scope &scope = GetCurrentScope();
        if( scope.mCurrentParamList.empty() ) {
            throw exception::runtime_error( "Internal Error! ConsumeParam() scope.mCurrentParamList.empty()!" );
        }
        auto val = std::move(scope.mCurrentParamList.front());
        scope.mCurrentParamList.pop();
        return val;
    }

    void SetSourceLocation( SourceLocation const &rLoc )
    {
        GetCurrentScope().mCurrentLoc = rLoc;
    }

    SourceLocation const &GetCurrentSourceLocation() const noexcept
    {
        return GetCurrentScope().mCurrentLoc;
    }

    void Dump( std::string_view  const search = {} )
    {
        // only global scope for now
#if TEASCRIPT_USE_COLLECTION_VARIABLE_STORAGE
        for( auto const &kv : mGlobalScope.mVariableCollection ) {
            if( kv.first.empty() ) { // placeholder, skip it.
                continue;
            }
#else
        for( auto const &kv : mGlobalScope.mVariableStorage ) {
#endif
            if( not search.empty() ) {
                if( std::string::npos == kv.first.find( search ) ) {
                    continue;
                }
            }
            if( FunctionPtr const *p_func = kv.second.GetValuePtr< FunctionPtr >(); p_func != nullptr ) {
                TEASCRIPT_PRINT( "{}{} : <function>\n", kv.first, (*p_func)->ParameterInfoStr() );
            } else {
                std::string valstr = kv.second.PrintValue();
                if( kv.second.GetTypeInfo()->IsSame( TypeString ) ) {
                    valstr.erase( 0, 1 ); // cut "
                    valstr.erase( valstr.size() - 1 ); // cut "
                    auto size = util::utf8_string_length( valstr );
                    util::prepare_string_for_print( valstr, 40 );
                    valstr += " (" + std::to_string( size ) + " glyphs)";
                }
                //               name (TypeName, const/mutable, address, schare count): value
                TEASCRIPT_PRINT( "{} ({}, {}, {:#x}, sc:{}) : {}\n", kv.first, kv.second.GetTypeInfo()->GetName(),
                                 (kv.second.IsConst() ? "const" : "mutable"),
                                 kv.second.GetInternalID(), kv.second.ShareCount(),
                                 valstr );
            }
        }
    }
};


/// Helper class for easy and exception safe manage new scopes.
class ScopedNewScope
{
    Context *mpContext;
public:
    ScopedNewScope( Context &rContext ) : mpContext( &rContext )
    {
        mpContext->EnterScope();
    }

    ScopedNewScope( Context &rContext, std::vector<ValueObject> const &rParamList, SourceLocation const &rLoc ) : ScopedNewScope( rContext )
    {
        mpContext->SetParamList( rParamList );
        mpContext->SetSourceLocation( rLoc );
    }

    ~ScopedNewScope()
    {
        Exit();
    }

    void Reset()
    {
        mpContext = nullptr;
    }

    void Exit()
    {
        if( mpContext ) {
            mpContext->ExitScope();
            Reset();
        }
    }
};

} // namespace teascript

