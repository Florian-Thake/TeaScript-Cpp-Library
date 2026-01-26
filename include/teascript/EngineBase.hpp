/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "ValueObject.hpp"
#include "Content.hpp"
#include "FunctionBase.hpp"


namespace teascript {

/// Base class for all TeaScript engines.
class EngineBase
{
protected:
    /// Evaluates the given \param rContent as TeaScript.
    /// Evaluation usually invokes either parsing and then the recursive evaluation of the produced AST,
    /// or a compilation of the produced AST followed by an execution of the binary program in the TeaStackVM.
    /// \param rContent The content to be evaluated.
    /// \param rName An arbitrary user defined name for referring to the content.
    /// \returns the result as ValueObject.
    virtual ValueObject EvaluateContent( Content const &rContent, std::string const &rName ) = 0;

    /// Adds the given ValuObject \param val to the current scope as name \praam rName.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    virtual void AddValueObject( std::string const &rName, ValueObject val ) = 0;

public:
    /// Default constructor.
    EngineBase()
    {
    }

    /// Virtual Destructor
    virtual ~EngineBase()
    {
    }

    /// Resets the state, e.g. clears all variables/functions and do a fresh bootstrap of the CoreLibrary
    virtual void ResetState() = 0;

    /// Returns the stored variable with name \param rName starting search in the current scope up to toplevel scope.
    /// \throw May throw exception::unknown_identifier or a different exception based on exception::eval_eror/runtime_error.
    virtual ValueObject GetVar( std::string const &rName ) const = 0;

    /// Executes the script referenced by file path \param path with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as a tuple "args[idx]". Additionally an "argN" variable indicating the parameter amount.
    /// The user might be responsible to remove prior used arg variables. Be aware of conflicts. A ResetState() will handle that.
    /// \note The legacy form of the arg variables "arg1", "arg2", ... is available via the compile setting TEASCRIPT_ENGINE_USE_LEGACY_ARGS=1
    /// \note It is implementation defined whether the content of the file or a further cached object (of a different form) will be used.
    /// \note Further it is implementation defined whether EvaluateContent() will be called or another way is used to execute the script.
    /// \note This function stays virtual for backward compatibility reasons.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/eval_error/runtime_error.
    virtual ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<std::string> const &args = {} )
    {
        std::vector<ValueObject> val_args;
        for( auto const &s : args ) {
            val_args.emplace_back( ValueObject( s, ValueConfig{ValueShared, ValueMutable} ) );
        }
        return ExecuteScript( path, val_args );
    }

    /// Executes the script referenced by file path \param path with the (optional) script parameters \param args.
    /// This variant uses full blown ValueObjects as script parameters.
    /// \see other ExecuteScript overload for more details.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/eval_error/runtime_error.
    virtual ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<ValueObject> const &args ) = 0;

    /// Execute given TeaScript code and returns the result. \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw any exception based on exception::parsing_error/eval_error/runtime_error.
    ValueObject ExecuteCode( std::string const &code, std::string const &name = "_USER_CODE_" )
    {
        return EvaluateContent( code, name );
    }

    /// Execute given TeaScript code and returns the result. \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw any exception based on exception::parsing_error/eval_error/runtime_error.
    ValueObject ExecuteCode( std::string_view const &code, std::string const &name = "_USER_CODE_" )
    {
        return EvaluateContent( code, name );
    }

    /// Execute given TeaScript code and returns the result. \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw any exception based on exception::parsing_error/eval_error/runtime_error.
    template< size_t N >
    ValueObject ExecuteCode( char const (&code)[N], std::string const &name = "_USER_CODE_" )
    {
        return EvaluateContent( code, name );
    }

    /// Invokes the TeaScript function with name rName with parameters in rParams.
    /// \returns the ValueObject result from the called fuction.
    /// \throw May throw exception::unknown_identifier or a different exception based on exception::eval_eror/runtime_error.
    virtual ValueObject CallFunc( std::string const &rName, std::vector<ValueObject> &rParams ) = 0;

    /// Invokes the TeaScript function with name rName with the additional parametes, which will be converted to ValueObjects.
    /// \returns the ValueObject result from the called fuction.
    /// \throw May throw exception::unknown_identifier or a different exception based on exception::eval_eror/runtime_error.
    /// \note Every parameter t must be a type which can be passed to a public ValueObject consructor.
    template< typename ...T> requires ((not std::is_same_v<T, ValueObject> && not std::is_same_v<T, std::vector<ValueObject>> && std::is_constructible_v<ValueObject, T, ValueConfig>) && ...)
    ValueObject CallFuncEx( std::string const &rName, T ... t ) /* different name for overload resolution in derived classes! */
    {
        std::vector<ValueObject> params{ValueObject( std::forward<T>( t ), ValueConfig{ValueShared,ValueMutable} )...};
        return CallFunc( rName, params );
    }

    /// Registers the given callback function \param rCallback as name \param rName in the current scope.
    /// The callback function is then invocable from TeaScript code by using its name and the call operator (pair of round brackets.)
    /// Pro tip: Use std::bind or a capturing lmabda to bring any arbitrary context with the callback.
    /// \note Actually the callback can be called with any amount of parameters. The callback is responsible to handle that.
    /// \warning EXPERIMENTAL: This interface and the general working and mechanics of user callbacks is experimental and may change.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    virtual void RegisterUserCallback( std::string const &rName, CallbackFunc const &rCallback ) = 0;

    /// Adds the given value as a mutable Bool with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddBoolVar( std::string const &rName, teascript::Bool const b )
    {
        AddValueObject( rName, ValueObject( b, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::Integer const i )
    {
        AddValueObject( rName, ValueObject( i, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable Integer with name \param rName to the current scope. NOTE: will change to I32 if this type is added!
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, int const i )
    {
        AddVar( rName, static_cast<teascript::Integer>(i) );
    }

    /// Adds the given value as a mutable U64 with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::U64 const u )
    {
        AddValueObject( rName, ValueObject( u, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable U64 with name \param rName to the current scope. NOTE: will change to U32 if this type is added!
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, unsigned int const u )
    {
        AddVar( rName, static_cast<teascript::U64>(u) );
    }

    /// Adds the given value as a mutable U8 with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::U8 const u )
    {
        AddValueObject( rName, ValueObject( u, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable Decimal with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::Decimal const d )
    {
        AddValueObject( rName, ValueObject( d, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::String const &s )
    {
        AddValueObject( rName, ValueObject( s, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::String  &&s )
    {
        AddValueObject( rName, ValueObject( std::move(s), ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable String with name \param rName to the current scope. The char array \param s must be zero terminated.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    template<size_t N>
    inline
    void AddVar( std::string const &rName, char const (&s)[N] )
    {
        AddVar( rName, teascript::String( s, N - 1 ) ); // - 1 for not add the \0 as content. String will be null terminated anyway.
    }

    /// Adds the given value as a mutable Buffer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::Buffer &&rBuffer )
    {
        AddValueObject( rName, ValueObject( std::move( rBuffer ), ValueConfig( ValueShared, ValueMutable ) ) );
    }

    /// Adds the given value (mutable or const) with name \param rName to the current scope. The ValueObejct must be a shared one!
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddSharedValueObject( std::string const &rName, teascript::ValueObject const &rValueObject )
    {
        if( rValueObject.InternalType() == ValueObject::TypeNaV ) {
            throw exception::runtime_error( "teascript::EngingeBase::AddSharedValueObject(): NaV not allowed!" );
        }
        if( not rValueObject.IsShared() ) {
            throw exception::runtime_error( "teascript::EngingeBase::AddSharedValueObject(): value must be shared!" );
        }
        AddValueObject( rName, rValueObject );
    }


    /// Adds arbitrary data as std::any for passthrough with name \param rName to the current scope.
    /// Passthrough data can only be assigned to variables and used as function parameters. The user is responsible for the contained data stays valid.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddPassthroughData( std::string const &rName, std::any &&any )
    {
        AddValueObject( rName, ValueObject( Passthrough{}, std::move( any ), ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Retrieves the passthrough data with name \praram rName as its concrete type. 
    /// This is a convenience function for GetVar(rName); some_cast<T>(var.GetPassthroughData());
    /// \throws exception::bad_value_cast or std::bad_any_cast if it is not passthrough data or the concrete type does not match.
    template< typename T >
    inline
    T & GetPassthroughData( std::string const &rName ) const
    {
        ValueObject val = GetVar( rName );
        return std::any_cast<T>(val.GetPassthroughData());
    }


    /// Adds the given value as a const Bool with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddBoolConst( std::string const &rName, teascript::Bool const b )
    {
        AddValueObject( rName, ValueObject( b, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::Integer const i )
    {
        AddValueObject( rName, ValueObject( i, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const Integer with name \param rName to the current scope. NOTE: will change to I32 if this type is added!
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, int const i )
    {
        AddConst( rName, static_cast<teascript::Integer>(i) );
    }

    /// Adds the given value as a const U64 with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::U64 const u )
    {
        AddValueObject( rName, ValueObject( u, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const U64 with name \param rName to the current scope. NOTE: will change to U32 if this type is added!
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, unsigned int const i )
    {
        AddConst( rName, static_cast<teascript::U64>(i) );
    }

     /// Adds the given value as a const U8 with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::U8 const u )
    {
        AddValueObject( rName, ValueObject( u, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const Decimal with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::Decimal const d )
    {
        AddValueObject( rName, ValueObject( d, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::String const &s )
    {
        AddValueObject( rName, ValueObject( s, ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::String  &&s )
    {
        AddValueObject( rName, ValueObject( std::move(s), ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

    /// Adds the given value as a const String with name \param rName to the current scope. The char array \param s must be zero terminated.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    template<size_t N>
    inline
    void AddConst( std::string const &rName, char const (&s)[N] )
    {
        AddConst( rName, teascript::String( s, N - 1 ) ); // - 1 for not add the \0 as content. String will be null terminated anyway.
    }

    /// Adds the given value as a mutable Buffer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, teascript::Buffer &&rBuffer )
    {
        AddValueObject( rName, ValueObject( std::move( rBuffer ), ValueConfig( ValueShared, ValueConst ) ) );
    }


    /// Adds arbitrary const data as std::any for passthrough with name \param rName to the current scope.
    /// Passthrough data can only be assigned to variables and used as function parameters. The user is responsible for the contained data stays valid.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    inline
    void AddConstPassthroughData( std::string const &rName, std::any &&any )
    {
        AddValueObject( rName, ValueObject( Passthrough{}, std::move( any ), ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

};

} // namespace teascript
