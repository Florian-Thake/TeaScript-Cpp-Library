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
    std::optional<teascript::Integer> mExitCode;

    /// Evaluates the given \param rContent as TeaScript.
    /// Evaluation usually invokes parsing and then the (recursive or iterative) evaluation of the produced AST.
    /// \param rContent The content to be evaluated.
    /// \param rName An arbitrary user defined name for referring to the content.
    /// \returns the result as ValueObject.
    virtual ValueObject EvaluateContent( Content const &rContent, std::string const &rName ) = 0;

    /// Adds the given ValuObject \param val to the current scope as name \praam rName.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
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

    /// Returns whether the last executed script/code ended with exit code.
    /// This usually happens when one of the exit()-like functions is called.
    inline
    bool HasExitCode() const noexcept
    {
        return mExitCode.has_value();
    }

    /// Returns the set exit code of last executed script/code or EXIT_FAILURE if there is none.
    inline
    teascript::Integer GetExitCode() const noexcept
    {
        return mExitCode.value_or( static_cast<teascript::Integer>(EXIT_FAILURE) );
    }

    /// Returns the stored variable with name \param rName starting search in the current scope up to toplevel scope.
    /// \throw May throw exception::unknown_identifier or a different excection based on exception::eval_eror/runtime_error.
    virtual ValueObject GetVar( std::string const &rName ) const = 0;

    /// Executes the script referenced by file path \param path with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as "arg1", "arg2", ..., "arg<N>". Additionally an "argN" variable indicating the parameter amount.
    /// The user might be responsible to remove prior used arg<N> variables. Be aware of conflicts. A ResetState() will handle that.
    /// \note It is implementation defined whether the content of the file or a further cached object (of a different form) will be used.
    /// \note Further it is implementation defined whether EvaluateContent() will be called or another way is used to execute the script.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/eval_error/runtime_error.
    virtual ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<std::string> const &args = {} ) = 0;

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

    /// Registers the given callback function \param rCallback as name \param rName in the current scope.
    /// The callback function is then invocable from TeaScript code by using its name and the call operator (pair of round brackets.)
    /// Pro tip: Use std::bind or a capturing lmabda to bring any arbitrary context with the callback.
    /// \note Actually the callback can be called with any amount of parameters. The callback is responsible to handle that.
    /// \warning EXPERIMENTAL: This interface and the general working and mechanics of user callbacks is experimental and may change often or be even removed.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    virtual void RegisterUserCallback( std::string const &rName, CallbackFunc const &rCallback ) = 0;

    /// Adds the given value as a mutable Bool with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddBoolVar( std::string const &rName, teascript::Bool const b )
    {
        AddValueObject( rName, ValueObject( b, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::Integer const i )
    {
        AddValueObject( rName, ValueObject( i, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, int const i )
    {
        AddVar( rName, static_cast<teascript::Integer>(i) );
    }

    /// Adds the given value as a mutable Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, unsigned int const i )
    {
        AddVar( rName, static_cast<teascript::Integer>(i) );
    }

    /// Adds the given value as a mutable Decimal with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::Decimal const d )
    {
        AddValueObject( rName, ValueObject( d, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddVar( std::string const &rName, teascript::String const &s )
    {
        AddValueObject( rName, ValueObject( s, ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Adds the given value as a mutable String with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    template<size_t N>
    inline
    void AddVar( std::string const &rName, char const (&s)[N] )
    {
        AddVar( rName, teascript::String( s, N ) );
    }


    /// Adds arbitrary data as std::any for passthrough with name \param rName to the current scope.
    /// Passthrough data can only be assigned to variables and used as function parameters. The user is responsible for the contained data stays valid.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    /// EXPERIMENTAL: This is an experimental inferface.
    inline
    void AddPassthroughData( std::string const &rName, std::any &&any )
    {
        AddValueObject( rName, ValueObject( Passthrough{}, std::move( any ), ValueConfig( eShared::ValueShared, eConst::ValueMutable ) ) );
    }

    /// Retrieves the passthrough data with name \praram rName as its concrete type. 
    /// This is a convenience function for GetVar(rName); some_cast<T>(var.GetPassthroughData());
    /// \throws exception::bad_value_cast or std::bad_any_cast if it is not passthrough data or the concrete type does not match.
    /// EXPERIMENTAL: This is an experimental interface.
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

    /// Adds the given value as a const Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, int const i )
    {
        AddConst( rName, static_cast<teascript::Integer>(i) );
    }

    /// Adds the given value as a const Integer with name \param rName to the current scope.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    inline
    void AddConst( std::string const &rName, unsigned int const i )
    {
        AddConst( rName, static_cast<teascript::Integer>(i) );
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
    template<size_t N>
    inline
    void AddConst( std::string const &rName, char const (&s)[N] )
    {
        AddConst( rName, teascript::String( s, N ) );
    }


    /// Adds arbitrary const data as std::any for passthrough with name \param rName to the current scope.
    /// Passthrough data can only be assigned to variables and used as function parameters. The user is responsible for the contained data stays valid.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    /// EXPERIMENTAL: This is an experimental inferface.
    inline
    void AddConstPassthroughData( std::string const &rName, std::any &&any )
    {
        AddValueObject( rName, ValueObject( Passthrough{}, std::move( any ), ValueConfig( eShared::ValueShared, eConst::ValueConst ) ) );
    }

};

} // namespace teascript
