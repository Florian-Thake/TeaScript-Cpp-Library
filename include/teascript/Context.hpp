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
#include "Dialect.hpp"

#include <vector>
#include <queue>
#include <utility> // std::pair
#include <string>
#include <memory>



namespace teascript {

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

