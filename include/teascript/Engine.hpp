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
#include "ConfigEnums.hpp"
#include "UserCallbackFunc.hpp"


#include <fstream>

// define TEASCRIPT_ENGINE_USE_WEB_PREVIEW to 1 for enabling and auto loading the web preview module for the default Engine.
// You must add <TeaScriptRoot>/modules/include/ to your include dirs and <TeaScriptRoot>/modules/source/WebPreview.cpp to your project.
#if !defined(TEASCRIPT_ENGINE_USE_WEB_PREVIEW)
# define TEASCRIPT_ENGINE_USE_WEB_PREVIEW       0
#endif


// define TEASCRIPT_ENGINE_USE_LEGACY_ARGS to 1 for use script arguments in the legacy form "arg1", "arg2", ..., "arg<N>"
// instead of a tuple "args[argN]" with all arguments as elements.
#if !defined(TEASCRIPT_ENGINE_USE_LEGACY_ARGS)
# define TEASCRIPT_ENGINE_USE_LEGACY_ARGS       0
#endif


#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW
# if __has_include( "teascript/modules/WebPreview.hpp" )
#  include "teascript/modules/WebPreview.hpp"
# else
#  error You must add <TeaScriptRoot>/modules/include/ to your include directories
# endif
#endif

namespace teascript {

// forward declarations...
namespace StackVM {
class Program;
using ProgramPtr = std::shared_ptr<Program>;
} // namespace StackVM


// header only and non-header only compile mode handling.
#if !defined(TEASCRIPT_DISABLE_HEADER_ONLY)
# define TEASCRIPT_DISABLE_HEADER_ONLY          0
#endif
// depending on the mode we need to declare member functions inline or not for resolve linker issues.
#if !defined( TEASCRIPT_COMPILE_MODE_INLINE )
# if TEASCRIPT_DISABLE_HEADER_ONLY
#  define TEASCRIPT_COMPILE_MODE_INLINE
# else
#  define TEASCRIPT_COMPILE_MODE_INLINE    inline 
# endif
#endif


/// The TeaScript standard engine.
/// This is a single-thread engine. You can use an instance of this class in
/// one thread. If you use the same(!) instance in more than one thread,
/// the instance is not thrad-safe by design.
/// However, in a multi-threaded environment it is safe to use one distinct
/// instance per each thread.
/// Furthermore, each instance has its own distinct context, which is not
/// shared between other instances/engines. You should not share values
/// between different Context/Engine instances, unless you take care of
/// thread safety by yourself.
/// \note see class EngineBase for some more handy convenience functions.
class Engine : public EngineBase
{
public:
    enum class eMode
    {
        Compile,
        Eval,
    };
protected:
    eMode             mMode        = eMode::Compile;    // default is compile
    eOptimize         mOptLevel    = eOptimize::O0;     // actually default is O0, will be changed to O1 later!
    config::eConfig   mCoreConfig;
    Context           mContext;
    struct BuildTools;  // separated in an extra struct and forward declare for safe includes of Parser, Compiler and StackMachine!
    std::shared_ptr<BuildTools> mBuildTools;


    /// Constructs the engine without bootstrapping the Core Library if \param bootstrap is false.
    /// If \param bootstrap is true it will bootstrap the Core Library with specified config from \param config.
    /// \note This constructor is useful for derived classes which don't want the default bootstrapping, e.g.
    ///       using another CoreLibrary or a derived class. Don't forget to override ResetState() in such a case.
    TEASCRIPT_COMPILE_MODE_INLINE Engine( bool const bootstrap, config::eConfig const config, eMode const mode = eMode::Compile, eOptimize const opt_level = eOptimize::O0 );

    /// Adds the given ValuObject \param val to the current scope as name \param rName.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE void AddValueObject( std::string const &rName, ValueObject val ) override;

    /// Evaluates/Executes the given content as TeaScript.
    /// Depending on the mode the content will be either parsed and evaluated or parsed, compiled and executed.
    /// \param rContent The content to be evaluated.
    /// \param rName An arbitrary user defined name for referring to the content.
    /// \returns the result as ValueObject.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject EvaluateContent( Content const &rContent, std::string const &rName ) override;

public:
    /// The default Constructor constructs the engine with everything loaded and bootstrapped.
    Engine() : Engine( config::full() )
    {
    }

    /// Constructs the engine with the specified config. Use the helper funcions from config namespace to simplify the configuration.
    TEASCRIPT_COMPILE_MODE_INLINE explicit Engine( config::eConfig const config, eMode const mode = eMode::Compile );

    /// Convenience constructor for specifying the loading level and the opt-out feature mask separately.
    TEASCRIPT_COMPILE_MODE_INLINE Engine( config::eConfig const level, unsigned int const opt_out )
        : Engine( config::build( level, opt_out ) )
    {
    }

    /// copy and assignment is deleted.
    Engine( Engine const & ) = delete;
    /// copy and assignment is deleted.
    Engine &operator=( Engine const & ) = delete;


    /// Resets the state of the context (+ parser and machine). Will do a fresh bootstrap of the CoreLibrary with the current saved configuration.
    /// \note This should be done usually prior each execution of a script to not interfer with old variables/modified environment.
    TEASCRIPT_COMPILE_MODE_INLINE void ResetState() override;

    /// enables or disables debug mode (default: off). This will also set the optimization level to Debug.
    /// \note enabled debug mode will preserve the source code for the ASTNodes. Thus, the parsing will take slightly longer and the ASTNodes use more memory.
    TEASCRIPT_COMPILE_MODE_INLINE void SetDebugMode( bool const enabled ) noexcept;

    /// Returns the stored variable with name \param rName starting search in the current scope up to toplevel scope.
    /// \throw May throw exception::unknown_identifier or a different excection based on exception::eval_eror/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject GetVar( std::string const &rName ) const override;

    /// Invokes the TeaScript function with name rName with parameters in rParams. (see EngineBase for the nice convenience function CallFuncEx!)
    /// \returns the ValueObject result from the called fuction.
    /// \throw May throw exception::unknown_identifier or a different excection based on exception::eval_eror/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject CallFunc( std::string const &rName, std::vector<ValueObject> &rParams ) override;


    /// Registers the given callback function \param rCallback as name \param rName in the current scope.
    /// The callback function is then invocable from TeaScript code by using its name and the call operator (pair of round brackets.)
    /// Pro tip: Use std::bind or a capturing lmabda to bring any arbitrary context with the callback.
    /// \note Actually the callback can be called with any amount of parameters. The callback is responsible to handle that.
    /// \warning EXPERIMENTAL: This interface and the general working and mechanics of user callbacks is experimental and may change.
    /// \throw May throw exception::redefinition_of_variable or a different excection based on exception::eval_eror/runtime_error.
    void RegisterUserCallback( std::string const &rName, CallbackFunc const &rCallback ) override
    {
        auto func = std::make_shared<UserCallbackFunc>( rCallback );
        ValueObject  val{std::move(func), ValueConfig( ValueShared, ValueMutable, mContext.GetTypeSystem() )};
        AddValueObject( rName, std::move( val ) );
    }

    /// Executes the script referenced with file path \param path with the (optional) script parameters \param args.
    /// \see the other overload and in class EngineBase for more details.
    ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<std::string> const &args = {} ) override
    {
        std::vector<ValueObject> val_args;
        for( auto const &s : args ) {
            val_args.emplace_back( ValueObject( s, ValueConfig{ValueShared, ValueMutable} ) );
        }
        return ExecuteScript( path, val_args );
    }

    /// Executes the script referenced with file path \param path with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as a tuple "args[idx]". Additionally an "argN" variable indicating the parameter amount.
    /// The user might be responsible to remove prior used arg variables. Be aware of conflicts. A ResetState() will handle that.
    /// The ValueObjects for the parameters must be in shared state (created with ValueShared or a MakeShared() call issued).
    /// \note The legacy form of the arg variables "arg1", "arg2", ... is available via the compile setting TEASCRIPT_ENGINE_USE_LEGACY_ARGS=1
    /// \note \see EngineBase::ExecuteScript for further important details.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/compile_error/eval_error/runtime_error/bad_value_cast.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<ValueObject> const &args );


    /// Executes the given \param program in the TeaStackVM with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as a tuple "args[idx]". Additionally an "argN" variable indicating the parameter amount.
    /// The user might be responsible to remove prior used arg variables. Be aware of conflicts. A ResetState() will handle that.
    /// The ValueObjects for the parameters must be in shared state (created with ValueShared or a MakeShared() call issued).
    /// \note The legacy form of the arg variables "arg1", "arg2", ... is available via the compile setting TEASCRIPT_ENGINE_USE_LEGACY_ARGS=1
    /// \note \see EngineBase::ExecuteScript for further important details.
    /// \throw May throw an exception based on exception::eval_error/runtime_error/bad_value_cast.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject ExecuteProgram( StackVM::ProgramPtr const &program, std::vector<ValueObject> const &args = {} );


    /// Compiles the given \param rContent to a binary program for the TeaStackVM with the optimization level \param opt_level.
    /// \throw May throw an exception based on exception::compile_error/eval_error/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE StackVM::ProgramPtr CompileContent( Content const &rContent, eOptimize const opt_level = eOptimize::O0, std::string const &rName = "_USER_CODE_" );

    /// Compiles the script referenced with file path \param path to a binary program for the TeaStackVM with the optimization level \param opt_level.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/compile_error/eval_error/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE StackVM::ProgramPtr CompileScript( std::filesystem::path const &path, eOptimize const opt_level = eOptimize::O0 );

    /// Compiles the TeaScript code in \param code to a binary program for the TeaStackVM with the optimization level \param opt_level.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw an exception based on exception::parsing_error/compile_error/eval_error/runtime_error.
    StackVM::ProgramPtr CompileCode( std::string const &code, eOptimize const opt_level = eOptimize::O0, std::string const &name = "_USER_CODE_" )
    {
        return CompileContent( code, opt_level, name );
    }

    /// Compiles the TeaScript code in \param code to a binary program for the TeaStackVM with the optimization level \param opt_level.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw an exception based on exception::parsing_error/compile_error/eval_error/runtime_error.
    StackVM::ProgramPtr CompileCode( std::string_view const &code, eOptimize const opt_level = eOptimize::O0, std::string const &name = "_USER_CODE_" )
    {
        return CompileContent( code, opt_level, name );
    }

    /// Compiles the TeaScript code in \param code to a binary program for the TeaStackVM with the optimization level \param opt_level.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \throw May throw an exception based on exception::parsing_error/compile_error/eval_error/runtime_error.
    template< size_t N >
    StackVM::ProgramPtr CompileCode( char const (&code)[N], eOptimize const opt_level = eOptimize::O0, std::string const &name = "_USER_CODE_" )
    {
        return CompileContent( code, opt_level, name );
    }
        
};

} // namespace teascript


// check for broken header only / not header only configurations.
#if (0==TEASCRIPT_DISABLE_HEADER_ONLY) && defined(TEASCRIPT_INCLUDE_DEFINITIONS)
# error header only config broken, TEASCRIPT_DISABLE_HEADER_ONLY is 0 but TEASCRIPT_INCLUDE_DEFINITIONS is defined.
#endif

#if (!TEASCRIPT_DISABLE_HEADER_ONLY)  || TEASCRIPT_INCLUDE_DEFINITIONS
# define TEASCRIPT_ENGINE_IMPL        1    /* just a guard */
# include "Engine_Impl.ipp"
# undef TEASCRIPT_ENGINE_IMPL
#endif


