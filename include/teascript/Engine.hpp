/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "EngineBase.hpp"
#include "Context.hpp"
#include "Parser.hpp"
#include "CoreLibrary.hpp"
#include "UserCallbackFunc.hpp"

#include <fstream>


namespace teascript {

/// The TeaScript standard engine.
/// This is a single-thread engine and this class is _not_ thread-safe.
/// You cannot use the same instance in more than 1 thread the same time.
/// However, it is safe to use one distinct instance per thread.
class Engine : public EngineBase
{
protected:
    config::eConfig  mCoreConfig;
    Context          mContext;
    Parser           mParser;

    /// Constructs the engine without bootstrapping the Core Library if \param bootstrap is false.
    /// If \param bootstrap is true it will bootstrap the Core Library with specified config from \param config.
    /// \note This constructor is useful for derived classes which don't want the default bootstrapping, e.g.
    ///       using another CoreLibrary or a derived class. Don't forget to override ResetState() in such a case.
    Engine( bool const bootstrap, config::eConfig const config )
        : EngineBase()
        , mCoreConfig( config )
        , mContext()
        , mParser()
    {
        if( bootstrap ) {
            CoreLibrary().Bootstrap( mContext, mCoreConfig );
        }
    }

    /// Adds the given ValuObject \param val to the current scope as name \praam rName.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    void AddValueObject( std::string const &rName, ValueObject val ) override
    {
        (void)mContext.AddValueObject( rName, val );
    }

    /// Evaluates the given \param rContent as TeaScript.
    /// This implementation invokes parsing and then evaluates the produced AST recursively.
    /// \param rContent The content to be evaluated.
    /// \param rName An arbitrary user defined name for referring to the content.
    /// \returns the result as ValueObject.
    ValueObject EvaluateContent( Content const &rContent, std::string const &rName ) override
    {
        try {
            auto ast = mParser.Parse( rContent, rName );
            return ast->Eval( mContext );
        } catch( teascript::control::Exit_Script const &ex ) {
            if( nullptr != ex.GetResult().GetValuePtr<teascript::Integer>() ) {
                mExitCode = ex.GetResult().GetAsInteger();
            }
            return ex.GetResult();
        } catch( teascript::control::ControlBase const & ) {
            throw exception::runtime_error( "A TeaScript control flow exception escaped. Check for wrong named loop labels!" );
        }
    }

public:
    /// The default Constructor constructs the engine with everything loaded and bootstrapped.
    Engine() : Engine( config::full() )
    {
    }

    /// Constructs the engine with the specified config. Use the helper funcions from config namespace to simplify the configuration.
    explicit Engine( config::eConfig const config )
        : EngineBase()
        , mCoreConfig( config )
        , mContext()
    {
        CoreLibrary().Bootstrap( mContext, mCoreConfig );
    }

    /// Convenience constructor for specifying the loading level and the opt-out feature mask separately.
    Engine( config::eConfig const level, unsigned int const opt_out )
        : Engine( config::build( level, opt_out ) )
    {
    }

    /// copy and assignment is deleted.
    Engine( Engine const & ) = delete;
    /// copy and assignment is deleted.
    Engine &operator=( Engine const & ) = delete;


    /// Resets the state of parser and context. Will do a fresh bootstrap of the CoreLibrary with the current saved configuration.
    void ResetState() override
    {
        mParser.ClearState();
        CoreLibrary().Bootstrap( mContext, mCoreConfig );
    }

    /// enables or disables debug mode (default: off).
    /// \note enabled debug mode will preserve the source code for the ASTNodes. Thus, the parsing will take slightly longer and the ASTNodes use more memory.
    void SetDebugMode( bool const enabled ) noexcept
    {
        mParser.SetDebug( enabled );
        mContext.is_debug = enabled;
    }

    /// Activates the old behavior of function parameters are mutable by default. 
    /// This was changed in 0.12.0 where copy assigned parameters are now const by default and only shared assigned parameters are still mutable by default.
    // \warning This function is only temporarily available (for transition) and will be marked deprecated in 0.13.0 and removed in 0.14.0.
    /// Please modify your script code accordingly (or decide to use an inofficial legacy dialect \see Dialect.hpp.)
    /// DEPRECATED: Please, change your script code to explicit mutable parameters with 'def' keyword.
    [[deprecated( "Please, change your script code to explicit mutable parameters with 'def' keyword." )]]
    void ActivateDeprecatedDefaultMutableParameters() noexcept
    {
        mContext.dialect.parameters_are_default_const = false;
        mParser.OverwriteDialect( mContext.dialect );
    }

    /// Returns the stored variable with name \param rName starting search in the current scope up to toplevel scope.
    /// \throw May throw exception::unknown_identifier or a different excection based on exception::eval_eror/runtime_error.
    ValueObject GetVar( std::string const &rName ) const override
    {
        return mContext.FindValueObject( rName );
    }


    /// Registers the given callback function \param rCallback as name \param rName in the current scope.
    /// The callback function is then invocable from TeaScript code by using its name and the call operator (pair of round brackets.)
    /// Pro tip: Use std::bind or a capturing lmabda to bring any arbitrary context with the callback.
    /// \note Actually the callback can be called with any amount of parameters. The callback is responsible to handle that.
    /// \warning EXPERIMENTAL: This interface and the general working and mechanics of user callbacks is experimental and may change often or be even removed.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    void RegisterUserCallback( std::string const &rName, CallbackFunc const &rCallback ) override
    {
        auto func = std::make_shared<UserCallbackFunc>( rCallback );
        ValueObject  val{std::move(func), ValueConfig( ValueShared, ValueMutable, mContext.GetTypeSystem() )};
        AddValueObject( rName, std::move( val ) );
    }

    /// Executes the script referenced with file path \param path with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as "arg1", "arg2", ..., "arg<N>". Additionally an "argN" variable indicating the parameter amount.
    /// The user might be responsible to remove prior used arg<N> variables. Be aware of conflicts. A ResetState() will handle that.
    /// \note \see EngineBase::ExecuteScript for further important details.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/eval_error/runtime_error.
    ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<std::string> const &args ) override
    {
        // build utf-8 filename again... *grrr*
#if defined(_WIN32)
        auto const tmp_u8 = path.generic_u8string();
        auto const filename = std::string( tmp_u8.begin(), tmp_u8.end() ); // must make a copy... :-(
#else
        // NOTE: On Windows it will be converted to native code page! Could be an issue when used as TeaScript string!
        auto const filename = path.generic_string();
#endif
        std::ifstream file( path, std::ios::binary | std::ios::ate ); // ate jumps to end.
        if( file ) {
            auto size = file.tellg();
            file.seekg( 0 );
            std::vector<char> buf( static_cast<size_t>(size) + 1 ); // ensure zero terminating!
            file.read( buf.data(), size );

            if( not args.empty() ) {
                mContext.SetScriptArgs( args );
            }

            Content  content{buf.data(), buf.size()};
            return EvaluateContent( content, filename ); // TODO: absolute path or filename?
        }

        throw exception::load_file_error( filename );
    }
};

} // namespace teascript

