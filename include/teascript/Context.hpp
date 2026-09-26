/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Collection.hpp"
#include "ValueObject.hpp"
#include "Type.hpp"
#include "Exception.hpp"
#include "Print.hpp"
#include "Util.hpp"
#include "UtilInternal.hpp"
#include "Settings.hpp"

#include <vector>
#include <queue>
#include <utility> // std::pair
#include <string>
#include <memory>


// define this to disable the Context drop in replacement and use the old implementation.
// NOTE: This is only for transition and will be removed next release!
//#define TEASCRIPT_DISABLE_NEW_CONTEXT    1


#if !TEASCRIPT_DISABLE_NEW_CONTEXT
# define TEASCRIPT_USE_NEW_CONTEXT         1
#else
# define TEASCRIPT_USE_NEW_CONTEXT         0
#endif


namespace teascript {

#if TEASCRIPT_USE_NEW_CONTEXT
/// The context for TeaScript script/code execution.
/// \warning This class and especially the class layout and all(!) data members are subject of heavy changes and are _not_ stable!
class Context
{
public:
    using ObjectType = ValueObject;

    // we keep this for compatibility (e.g., for CoreLibrary::Bootstrap) but this is not the internal used container anymore.
    using VariableCollection = Collection<ObjectType, std::string, col_policy::eOrder::Unordered >; // first in, last out and quick access.

    using ParameterList = std::vector<ObjectType>;  // FIFO, for consuming parameters of function calls.

    using IndexType = std::uint32_t;
    
    // carries all information for one named variable of one scope.
    struct Binding
    {
        ObjectType  mValue;
        IndexType   mScopeIndex;
        // not sure why clang need this help for construct this simple struct...
#if defined( __clang__ )
        inline Binding( ObjectType const & val, IndexType const idx ) : mValue(val), mScopeIndex(idx) {}
#endif
    };

    struct StorageEntry
    {
        std::vector<Binding> mValues; // LIFO, back is always the visible value which shadows all other in the list.
    };

    using VariableStorage = std::unordered_map<std::string, StorageEntry>;

    // since our storage is node based and we don't delete entries for bindings, the pointer to the entries are staying stable.
    using StorageHandle      = StorageEntry *;
    using ConstStorageHandle = StorageEntry const *;

private:
    struct Scope
    {
        std::vector<StorageHandle> mBindings; // all variables of this scope (back() during cleanup), LIFO
        
        ParameterList    mCurrentParamList;
        ParameterList::iterator mCurrentParamIt;
        SourceLocation   mCurrentLoc;

        Scope()
            : mCurrentParamIt( mCurrentParamList.end() )
        {
        }

        Scope( Scope && rOther) noexcept // for move in std::vector! important, otherwise Cleanup will be called if scope vector grows and reallocates!
            : mBindings(std::move(rOther.mBindings))
            , mCurrentParamList(std::move(rOther.mCurrentParamList))
            , mCurrentParamIt(std::move(rOther.mCurrentParamIt))
            , mCurrentLoc(std::move(rOther.mCurrentLoc))
        { 
        }

        void Cleanup()
        {
            while( not mBindings.empty() ) {
                if( auto h = mBindings.back(); h ) h->mValues.pop_back();
                //mBindings.back()->mValues.pop_back();
                mBindings.pop_back();
            }

            mCurrentParamList.clear();
            mCurrentParamIt = mCurrentParamList.end();
            mCurrentLoc = SourceLocation();
        }

        ~Scope()
        {
            Cleanup();
        }
    };

    // We need to ensure this for Sopes are moved within the vector on reallocations!
    static_assert(std::is_nothrow_move_constructible_v<Scope>);


    bool mBootstrapped = true;

    Settings mSettings;
    
    TypeSystem mTypeSystem; // Better be a shared ptr?

    VariableStorage mVariableStorage;

    std::vector<Scope> mScopes;
    IndexType          mScopeIndex = 0;

    Scope &GetCurrentScope()
    {
        assert( mScopeIndex < mScopes.size() );
        return mScopes[mScopeIndex];
    }

    Scope const &GetCurrentScope() const
    {
        assert( mScopeIndex < mScopes.size() );
        return mScopes[mScopeIndex];
    }

    void CheckName( std::string const &rName, SourceLocation const &rLoc = {} ) const
    {
        if( mBootstrapped && rName.starts_with( '_' ) ) {
            throw exception::internal_name( rLoc );
        }
    }

public:
    Context()
    {
        mScopes.emplace_back(); // global / top level scope is always present
    }

    Context( Context && ) = default;
    // IMPORTANT: rOther is not useable after move because there is no global scope anymore!
    Context &operator=( Context &&rOther )
    {
        // first clear possible old!!
        while( !mScopes.empty() ) {
            mScopes.pop_back();
        }
        mBootstrapped = rOther.mBootstrapped;
        mSettings = std::move( rOther.mSettings );
        mTypeSystem = std::move( rOther.mTypeSystem );
        mVariableStorage = std::move( rOther.mVariableStorage );
        mScopes = std::move( rOther.mScopes );
        mScopeIndex = rOther.mScopeIndex;
        //rOther.mScopeIndex = 0; //does not help, 0 also does not exist anymore. but we don't want alloc a new scope here...
        return *this;
    }

    explicit Context( Settings &&rSettings, bool const booting = false )
        : mBootstrapped( not booting )
        , mSettings( std::move( rSettings ) )
    {
        mScopes.emplace_back(); // global / top level scope is always present
    }

    explicit Context( TypeSystem &&rMovedSys, Settings &&rSettings = Settings(), bool const booting = false)
        : mBootstrapped( not booting )
        , mSettings( std::move( rSettings ) )
        , mTypeSystem( std::move( rMovedSys ) )
    {
        mScopes.emplace_back(); // global / top level scope is always present
    }

    ~Context()
    {
        while( !mScopes.empty() ) {
            mScopes.pop_back();
        }
    }

    Settings const &GetSettings() const
    {
        return mSettings;
    }

    eOptimize SwitchOptimizationLevel( eOptimize const opt_level )
    {
        eOptimize const old = mSettings.GetOptimizationLevel();
        mSettings.SetOptimizationLevel( opt_level );
        return old;
    }


    void SetBootstrapDone()
    {
        mBootstrapped = true;
    }


    /// \note this function is for INTERNAL use only!
    void InjectVars( VariableCollection &&col )
    {
        if( mBootstrapped ) {
            return;
        }
        //TODO: check, just preliminary
        mVariableStorage.reserve( mVariableStorage.size() + col.Size() );
        auto &scope = GetCurrentScope();
        for( auto &kv : col ) {
            auto h = GetHandle( kv.first );
            assert( h->mValues.empty() );
            h->mValues.emplace_back( kv.second, mScopeIndex );
            scope.mBindings.emplace_back( h );
        }
    }

    /// this function will either add a tuple args[argN] with rArgs as elements to the current scope
    /// or legacy arg variables "arg1", "arg2", ... An argN variable is added in both cases.
    void SetScriptArgs( std::vector<std::string> const &rArgs, bool const legacy = false )
    {
        std::vector<ValueObject> val_args;
        for( auto const &s : rArgs ) {
            val_args.emplace_back( ValueObject( s, ValueConfig{eShared::ValueShared, eConst::ValueMutable} ) );
        }
        SetScriptArgs( val_args, legacy );
    }

    /// this function will either add a tuple args[argN] with rArgs as elements to the current scope
    /// or legacy arg variables "arg1", "arg2", ... An argN variable is added in both cases.
    void SetScriptArgs( std::vector<ValueObject> const &rArgs, bool const legacy = false )
    {
        if( legacy ) {
            int i = 0;
            for( auto &arg : rArgs ) {
                AddValueObject( "arg" + std::to_string( ++i ), arg );
            }
        } else {
            Tuple args;
            for( auto &arg : rArgs ) {
                args.AppendValue( arg );
            }
            AddValueObject( "args", ValueObject( std::move( args ), ValueConfig( ValueShared, ValueMutable, mTypeSystem ) ) );
        }
        AddValueObject( "argN", ValueObject( static_cast<long long>(rArgs.size()), true ) );
    }

    TypeSystem &GetTypeSystem()
    {
        return mTypeSystem;
    }

    TypeSystem const &GetTypeSystem() const
    {
        return mTypeSystem;
    }

    /// always returns a valid handle for given name, either a new created one or an already existing.
    /// \note The handle is only valid as long as this context lives!
    /// IMPORTANT: This function is INTERNAL and EXPERIMENTAL for now.
    StorageHandle GetHandle( std::string const &rName )
    {
        auto [it, _] = mVariableStorage.try_emplace( rName );
        return &it->second;
    }

    /// returns a handle for given name if exists, otherwise nullptr.
    /// \note The handle is only valid as long as this context lives!
    /// IMPORTANT: This function is INTERNAL and EXPERIMENTAL for now.
    ConstStorageHandle FindHandle( std::string const &rName ) const
    {
        auto it = mVariableStorage.find( rName );
        if( it == mVariableStorage.end() ) {
            return nullptr;
        }
        return &it->second;
    }

    /// returns a handle for given name if exists, otherwise nullptr.
    /// \note The handle is only valid as long as this context lives!
    /// IMPORTANT: This function is INTERNAL and EXPERIMENTAL for now.
    StorageHandle FindHandle( std::string const &rName )
    {
        // one of the rare unevil const_cast: first make this const for reuse the const overload, then remove the const again from the result.
        return const_cast<StorageHandle>(const_cast<Context const *>(this)->FindHandle( rName ));
    }

    ObjectType FindValueObject( std::string const &rName, SourceLocation const &rLoc = {}, long long *pScopeLevel = nullptr ) const
    {
        auto h = FindHandle( rName );
        if( h && not h->mValues.empty() ) {
            if( pScopeLevel ) {
                *pScopeLevel = 1LL + mScopeIndex - h->mValues.back().mScopeIndex;
            }
            return h->mValues.back().mValue;
        }
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType AddValueObject( std::string const &rName, ValueObject const &rValue, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        if( rValue.IsShared() ) {
            auto h = GetHandle( rName );
            if( not h->mValues.empty() && h->mValues.back().mScopeIndex == mScopeIndex ) {
                throw exception::redefinition_of_variable( rLoc, rName );
            }
            h->mValues.emplace_back( rValue, mScopeIndex );
            GetCurrentScope().mBindings.emplace_back( h );
            return h->mValues.back().mValue;
        }
        throw exception::runtime_error( rLoc, "ValueObject must be shared for add it!" );
    }

    ObjectType RemoveValueObject( std::string const &rName, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        // remove is possible only in the current scope.
        auto h = FindHandle( rName );
        if( h && not h->mValues.empty() ) {
            if( h->mValues.back().mScopeIndex == mScopeIndex ) {
                auto val = std::move(h->mValues.back().mValue);
                auto &scope = GetCurrentScope();
                auto it = std::find( scope.mBindings.begin(), scope.mBindings.end(), h );
                assert( it != scope.mBindings.end() );
                *it = nullptr;
                //GetCurrentScope().mBindings.erase( std::remove( GetCurrentScope().mBindings.begin(), GetCurrentScope().mBindings.end(), h ), GetCurrentScope().mBindings.end() );
                h->mValues.pop_back();
                return val;
            }
        }
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType SetValue( std::string const &rName, ValueObject const &rValue, bool const shared, SourceLocation const &rLoc = {} )
    {
        auto h = FindHandle( rName );
        if( h && not h->mValues.empty() ) {
            if( shared && rValue.IsShared() ) {
                h->mValues.back().mValue.SharedAssignValue( rValue, rLoc );
            } else {
                h->mValues.back().mValue.AssignValue( rValue, rLoc );
            }
            return h->mValues.back().mValue;
        }
        throw exception::unknown_identifier( rLoc, rName );
    }

    void EnterScope()
    {
        ++mScopeIndex;
        if( mScopes.size() == mScopeIndex ) {
            mScopes.emplace_back();
        }
    }

    void ExitScope()
    {
        if( LocalScopeCount() > 0 ) {
            GetCurrentScope().Cleanup();
            --mScopeIndex;
        } else {
            throw exception::runtime_error( "Internal Error! ExitScope() with empty local scopes!" );
        }
    }

    size_t LocalScopeCount() const
    {
        return mScopeIndex;
    }

    size_t TotalCurrentScopeBindings() const
    {
        auto &scope = GetCurrentScope();
        auto removed = std::count( scope.mBindings.begin(), scope.mBindings.end(), nullptr );
        return scope.mBindings.size() - removed;
        //return GetCurrentScope().mBindings.size();
    }

    size_t TotalVariableSlots() const
    {
        return mVariableStorage.size();
    }

    size_t HighestScopeWatermark() const
    {
        return mScopes.size();
    }

    void SetParamList( std::vector<ValueObject> &paramlist )
    {
        GetCurrentScope().mCurrentParamList = std::move( paramlist );
        GetCurrentScope().mCurrentParamIt   = GetCurrentScope().mCurrentParamList.begin();
    }

    size_t CurrentParamCount() const
    {
        return GetCurrentScope().mCurrentParamList.end() - GetCurrentScope().mCurrentParamIt;
    }

    ValueObject ConsumeParam()
    {
        Scope &scope = GetCurrentScope();
        if( scope.mCurrentParamIt == scope.mCurrentParamList.end() ) {
            throw exception::runtime_error( "Internal Error! ConsumeParam() scope.mCurrentParamIt == scope.mCurrentParamList.end()!" );
        }
        auto val = std::move( *scope.mCurrentParamIt );
        ++scope.mCurrentParamIt;
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

    /// Dumps all visible(!) variables and functions of all existing scopes in alphabetical order.
    /// \note this function is not thread safe. Call this only when the program is suspended or halted.
    void Dump( std::string_view  const search = {} )
    {
        std::vector< VariableStorage::value_type const * > sorted;
        sorted.reserve( mVariableStorage.size() );
        for( auto const &val : mVariableStorage ) {
            sorted.push_back( &val );
        }
        std::sort( sorted.begin(), sorted.end(), []( auto const *v1, auto const *v2 ) {
            return v1->first < v2->first;
        } );

        for( auto const v : sorted ) {
            auto const &[name, values] = *v;
            if( values.mValues.empty() ) {
                continue;
            }
            if( not search.empty() ) {
                if( std::string::npos == name.find( search ) ) {
                    continue;
                }
            }

            auto const &visible_value = values.mValues.back().mValue;

            DumpOne( name, visible_value );
        }
    }

private:
    void DumpOne( std::string const &rName, ValueObject const &rValue )
    {
        if( FunctionPtr const *p_func = rValue.GetValuePtr< FunctionPtr >(); p_func != nullptr ) {
            TEASCRIPT_PRINT( "{}{} : <function>\n", rName, (*p_func)->ParameterInfoStr() );
        } else {
            std::string valstr = rValue.PrintValue();
            if( rValue.GetTypeInfo()->IsSame( TypeString ) ) {
                valstr.erase( 0, 1 ); // cut "
                valstr.erase( valstr.size() - 1 ); // cut "
                auto size = util::utf8_string_length( valstr );
                util::prepare_string_for_print( valstr, 40 );
                valstr += " (" + std::to_string( size ) + " glyphs)";
            }
            //               name (TypeName, const/mutable, address, schare count): value
            TEASCRIPT_PRINT( "{} ({}, {}, {:#x}, sc:{}) : {}\n", rName, rValue.GetTypeInfo()->GetName(),
                             (rValue.IsConst() ? "const" : "mutable"),
                             rValue.GetInternalID(), rValue.ShareCount(),
                             valstr );
        }
    }

};

#else
/// The context for TeaScript script/code execution.
/// \warning This class and especially the class layout and all(!) data members are subject of heavy changes and are _not_ stable!
class Context
{
public:
    using ObjectType = ValueObject;

    //TODO [ITEM 96] Refactor the internal storage layout and lookup for variables and scopes.

    using VariableCollection = Collection<ObjectType, std::string, col_policy::eOrder::Unordered >; // first in, last out and quick access.

    using ParameterList = std::queue<ObjectType>;  // FIFO, for consuming parameters of function calls.

    class Scope
    {
    public:
        Scope() = default;

        VariableCollection  mVariableCollection;

        ParameterList    mCurrentParamList;
        SourceLocation   mCurrentLoc;

        void Cleanup()
        {
            //TODO: (future) Need to lookup registered Destructor functions and call them!
            mVariableCollection.Clear();

            {
                ParameterList  empty;
                mCurrentParamList.swap( empty );
            }
        }

        ~Scope()
        {
            Cleanup();
        }
    };

private:
    bool mBootstrapped = true;

    Settings mSettings;

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

public:
    Context() = default;
    Context( Context && ) = default;
    Context &operator=( Context && ) = default;

    explicit Context( Settings &&rSettings, bool const booting = false )
        : mBootstrapped( not booting )
        , mSettings( std::move( rSettings ) )
    {
    }

    explicit Context( TypeSystem &&rMovedSys, Settings && rSettings = Settings(), bool const booting = false)
        : mBootstrapped( not booting )
        , mSettings( std::move( rSettings ) )
        , mTypeSystem( std::move( rMovedSys ) )
    {
    }


    ~Context()
    {
        while( !mLocalScopes.empty() ) {
            mLocalScopes.pop_back();
        }
    }

    Settings const &GetSettings() const
    {
        return mSettings;
    }

    eOptimize SwitchOptimizationLevel( eOptimize const opt_level )
    {
        eOptimize const old = mSettings.GetOptimizationLevel();
        mSettings.SetOptimizationLevel( opt_level );
        return old;
    }


    void SetBootstrapDone()
    {
        mBootstrapped = true;
    }


    /// moves the variable collection into the global scope, all prior vars will be lost.
    /// only do something during bootstrapping, otherwise a no-op. 
    void InjectVars( VariableCollection && col )
    {
        if( mBootstrapped ) {
            return;
        }
        mGlobalScope.mVariableCollection = std::move( col );
    }

    /// this function will either add a tuple args[argN] with rArgs as elements to the current scope
    /// or legacy arg variables "arg1", "arg2", ... An argN variable is added in both cases.
    void SetScriptArgs( std::vector<std::string> const &rArgs, bool const legacy = false )
    {
        std::vector<ValueObject> val_args;
        for( auto const &s : rArgs ) {
            val_args.emplace_back( ValueObject( s, ValueConfig{eShared::ValueShared, eConst::ValueMutable} ) );
        }
        SetScriptArgs( val_args, legacy );
    }

    /// this function will either add a tuple args[argN] with rArgs as elements to the current scope
    /// or legacy arg variables "arg1", "arg2", ... An argN variable is added in both cases.
    void SetScriptArgs( std::vector<ValueObject> const &rArgs, bool const legacy = false )
    {
        //TODO: add arg0 as 'main script name' ?!
        if( legacy ) {
            int i = 0;
            for( auto &arg : rArgs ) {
                AddValueObject( "arg" + std::to_string( ++i ), arg );
            }
        } else {
            Tuple args;
            for( auto &arg : rArgs ) {
                args.AppendValue( arg );
            }
            AddValueObject( "args", ValueObject( std::move(args), ValueConfig( ValueShared,ValueMutable, mTypeSystem ) ) );
        }
        AddValueObject( "argN", ValueObject( static_cast<long long>(rArgs.size()), true ) );
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
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType AddValueObject( std::string const &rName, ValueObject const &rValue, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        // only search in the most recent scope...
        Scope &scope = GetCurrentScope();
        if( rValue.IsShared() ) { //TODO: maybe this can be relaxed when the lookup does not safe a copy but an index to the storage or sth. similar (but shared assign is special then!)
            if( not scope.mVariableCollection.AppendKeyValue( rName, rValue ) ) {
                throw exception::redefinition_of_variable( rLoc, rName );
            }
            // new object is always last position.
            return scope.mVariableCollection[scope.mVariableCollection.Size() - 1u];
        }
        throw exception::runtime_error( rLoc, "ValueObject must be shared for add it!" );
    }

    ObjectType RemoveValueObject( std::string const &rName, SourceLocation const &rLoc = {} )
    {
        CheckName( rName, rLoc );
        // for now only in the current scope.
        //TODO: check if outer scopes shall be considered as well!
        Scope &scope = GetCurrentScope();
        auto res = scope.mVariableCollection.RemoveValueByKeyWithPlaceholder( rName, ValueObject() );
        if( res.has_value() ) {
            return res.value();
        }
        throw exception::unknown_identifier( rLoc, rName );
    }

    ObjectType SetValue( std::string const &rName, ValueObject const &rValue, bool const shared, SourceLocation const &rLoc = {} )
    {
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

    size_t LocalScopeCount() const
    {
        return mLocalScopes.size();
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

    /// Dumps all variables and functions of all actually present scopes.
    /// \note if a program is suspended or halted there can be more than one scopes present, otherwise there is only the global scope.
    /// \note in case there are local scopes, you might see shadowed variables as well. The last printed one is the visible one.
    void Dump( std::string_view  const search = {} )
    {
        Dump( mGlobalScope.mVariableCollection, search );
        for( auto it = mLocalScopes.begin(); it != mLocalScopes.end(); ++it ) {
            Dump( it->mVariableCollection, search );
        }
    }

private:
    void Dump( VariableCollection const &col, std::string_view  const search )
    {
        for( auto const &kv : col ) {
            if( kv.first.empty() ) { // placeholder, skip it.
                continue;
            }
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

#endif // #if TEASCRIPT_USE_NEW_CONTEXT


/// Helper class for easy and exception safe manage new scopes.
class ScopedNewScope
{
    Context *mpContext;
public:
    ScopedNewScope( Context &rContext ) : mpContext( &rContext )
    {
        mpContext->EnterScope();
    }

    ScopedNewScope( Context &rContext, std::vector<ValueObject> &rParamList, SourceLocation const &rLoc ) : ScopedNewScope( rContext )
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

