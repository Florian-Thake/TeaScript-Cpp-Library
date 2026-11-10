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


// DEPRECATED: Please, use only the new form for args. This define will be removed!
// define TEASCRIPT_ENGINE_USE_LEGACY_ARGS to 1 for use script arguments in the legacy form "arg1", "arg2", ..., "arg<N>"
// instead of a tuple "args[argN]" with all arguments as elements.
#if !defined(TEASCRIPT_ENGINE_USE_LEGACY_ARGS)
# define TEASCRIPT_ENGINE_USE_LEGACY_ARGS       0
#endif


// define TEASCRIPT_ENGINE_USE_LOCAL_SCOPE_FOR_MAINSCRIPT to 0 for use old behavior where the script runs in the current scope (which usually is the 'global' scope).
// NOTE: As long as TEASCRIPT_ENGINE_USE_LEGACY_ARGS exists, this define is dependent from it. If legacy args are used, the default for this define will change to 0.
// IMPORTANT: This define is only for transition. In future releases it will be removed. Then a local scope for the main script is always on and there isn't a compile switch for it anymore!
#if !defined(TEASCRIPT_ENGINE_USE_LOCAL_SCOPE_FOR_MAINSCRIPT)
# if TEASCRIPT_ENGINE_USE_LEGACY_ARGS == 0
#  define TEASCRIPT_ENGINE_USE_LOCAL_SCOPE_FOR_MAINSCRIPT    1
# else
#  define TEASCRIPT_ENGINE_USE_LOCAL_SCOPE_FOR_MAINSCRIPT    0
# endif
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
///
/// \note see class EngineBase for some more handy convenience functions.
///
/// If you want use the same engine for multiple runs you may either want
/// a) keep the same context so that next runs will see all changes from the
///    last runs. (You can change the script args while the rest of the context
///    stays unchanged.)
/// b) Always use a fresh context via ResetStateFor( Reset_All ) between each run.
/// c) Keep your base environment present for all scripts by doing something like:
///    1) Create Engine (will bootstrap Core Library).
///    2) Issue AddVar, RegisterUserCallback, RunPreamble* for build your custom
///       environment.
///    3) Execute the main script. <---<---<---<---<---<---<---<---<---<---<----<
///    4) ResetStateFor( Reset_LocalScopes ) (optionally Reset_ScriptArgs)      |
///    5) Loop to 3)     --->--->--->--->--->--->--->--->--->--->--->--->--->---^
///    6) or finally end.
/// d) A variation of c)
///
/// \note Depending on the mode and the error, check the GetLocalScopeCount() on error
///       and consider to call ResetStateFor( Reset_All ) or ResetStateFor( Reset_LocalScopes )
///       if an error occurred during the run before you execute any next run.
class Engine : public EngineBase
{
public:
    enum class eMode
    {
        Compile,
        Eval,
    };

    enum eStateResetWhat
    {
        Reset_DontTouchContext = 0,    /// machine + parser reset only, context won't be modified.
        
        //Reset_Modules          = 0x01, /// Modules only (reserved, no effect until module system is provided!)
        //Reset_Preamble         = 0x02, /// Preamble only (reserved, not yet implemented)
        Reset_ScriptArgs       = 0x04, /// Script args only
        Reset_LocalScopes      = 0x08, /// all scopes except the top level 'global' scope will be removed.
        
        Reset_All              = 0xff, /// Modules + ScriptArgs + Local Scopes + CoreLibrary (complete rebuild of the Context)

        // convenience
        Reset_ScriptState  = Reset_ScriptArgs | Reset_LocalScopes /*| Reset_Modules*/,
    };

    enum class eScopeBehavior
    {
        ScriptRunsInCurrent,  /// no extra scope will be added, script runs in the current scope (usually the 'global' scope).
        ScriptRunsInLocal,    /// the main script does not run in the 'global' scope. At least one local scope is added for the script.
#if TEASCRIPT_ENGINE_USE_LOCAL_SCOPE_FOR_MAINSCRIPT
        ScriptRunsIn_Default = ScriptRunsInLocal,
#else
        ScriptRunsIn_Default = ScriptRunsInCurrent,
#endif
    };
protected:
    eMode             mMode          = eMode::Compile;    // default is compile
    eOptimize         mOptLevel      = eOptimize::O0;     // actually default is O0, will be changed to O1 later!
    eScopeBehavior    mScopeBehavior = eScopeBehavior::ScriptRunsIn_Default;
    bool              mBuildDefaultArgs = true;           // whether empty default args shall be added if none are passed as arguments.
    config::eConfig   mCoreConfig;
    Context           mContext;
    struct BuildTools;  // separated in an extra struct and forward declare for safe includes of Parser, Compiler and StackMachine!
    std::shared_ptr<BuildTools> mBuildTools;


    /// Constructs the engine without bootstrapping the Core Library if \param bootstrap is false.
    /// If \param bootstrap is true it will bootstrap the Core Library with specified config from \param config.
    /// \note This constructor is useful for derived classes which don't want the default bootstrapping, e.g.
    ///       using another CoreLibrary or a derived class. Don't forget to override BootstrapCoreLibrary() in such a case.
    TEASCRIPT_COMPILE_MODE_INLINE Engine( bool const bootstrap, config::eConfig const config, eMode const mode = eMode::Compile, eOptimize const opt_level = eOptimize::O0 );

    /// Constructs the engine without bootstrapping the Core Library if \param bootstrap is false.
    /// If \param bootstrap is true it will bootstrap the Core Library with specified settings from \param rSettings.
    /// \note This constructor is useful for derived classes which don't want the default bootstrapping, e.g.
    ///       using another CoreLibrary or a derived class. Don't forget to override BootstrapCoreLibrary() in such a case.
    TEASCRIPT_COMPILE_MODE_INLINE Engine( bool const bootstrap, Settings &&rSettings, eMode const mode = eMode::Compile );


    /// Will Bootstrap the CoreLibrary (+ possibly other configured default modules) into a new, freshly built context.
    /// \note override this to load your own C++ modules. Will be called by the engine during bootstrap / ResetStateFor( Reset_All ).
    TEASCRIPT_COMPILE_MODE_INLINE virtual void BootstrapCoreLibrary();

    /// Adds the given ValuObject \param val to the current scope as name \param rName.
    /// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error.
    TEASCRIPT_COMPILE_MODE_INLINE void AddValueObject( std::string const &rName, ValueObject val ) override;

    /// Evaluates/Executes the given content as TeaScript.
    /// Depending on the mode the content will be either parsed and evaluated or parsed, compiled and executed.
    /// \param rContent The content to be evaluated.
    /// \param rName An arbitrary user defined name for referring to the content.
    /// \returns the result as ValueObject.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject EvaluateContent( Content const &rContent, std::string const &rName ) override;

    /// Ensures the current set eScopeBehavior is followed, e.g., adds a new scope if behavior is ScriptRunsInLocal and there is no local scope yet.
    TEASCRIPT_COMPILE_MODE_INLINE void FollowCurrentScopeBehavior();

public:
    /// The default Constructor constructs the engine with everything loaded and bootstrapped.
    Engine() : Engine( config::full() )
    {
    }

    /// Constructs the engine with the specified settings.
    TEASCRIPT_COMPILE_MODE_INLINE explicit Engine( Settings && rSettings, eMode const mode = eMode::Compile );

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


    /// \returns the current settings
    TEASCRIPT_COMPILE_MODE_INLINE Settings const &GetSettings() const;

    /// Resets the state, e.g. clears all variables/functions and do a fresh bootstrap of the CoreLibrary
    /// \note This should be done usually prior each execution of a script to not interfer with old variables/modified environment.
    void ResetState() override
    {
        ResetStateFor( Reset_All );
    }

    /// Resets a specific part of the context (+ parser and machine).
    /// \param what specifies via an or combination of eStateResetWhat which part of the context shall be reset (script args, local scopes, core library, etc.)
    /// \note if the core library shall be freshly bootstrapped it will affect all other part of the context as well. The context will be completely exchanged to a new one in that case.
    /// \note This should be done usually prior each execution of a script to not interfer with old variables/modified environment.
    TEASCRIPT_COMPILE_MODE_INLINE void ResetStateFor( unsigned int const what );


    /// \returns the current local scope count. 0 means the 'global' scope is the current top level scope.
    /// \note After ResetStateFor( Reset_LocalScopes ) this function will return 0.
    TEASCRIPT_COMPILE_MODE_INLINE size_t GetLocalScopeCount() const noexcept;

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
        ValueObject  val{std::move(func), ValueConfig( ValueShared, ValueMutable )};
        AddValueObject( rName, std::move( val ) );
    }

    // pull in the other overload with std::vector<std::string> args.
    using EngineBase::ExecuteScript;

    /// Executes the script referenced with file path \param path with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as a tuple "args[idx]". Additionally an "argN" variable indicating the parameter amount.
    /// If the args is empty any existing args from a prior execute will stay present, otherwise the new provided args will be used.
    /// Use ResetStateFor( Reset_ScriptArgs ) before this call if you need empty args.
    /// \note The ValueObjects for the parameters must be in shared state (created with ValueShared or a MakeShared() call issued).
    /// \note The engine will always provide at least an empty args Tuple with argN==0 if an Execute* function with args param is used.
    /// \note \see EngineBase::ExecuteScript for further important details.
    /// \throw May throw exception::load_file_error or any exception based on exception::parsing_error/compile_error/eval_error/runtime_error/bad_value_cast.
    TEASCRIPT_COMPILE_MODE_INLINE ValueObject ExecuteScript( std::filesystem::path const &path, std::vector<ValueObject> const &args ) override;


    /// Executes the given \param program in the TeaStackVM with the (optional) script parameters \param args.
    /// The script parameters will be available for the script as a tuple "args[idx]". Additionally an "argN" variable indicating the parameter amount.
    /// If the args is empty any existing args from a prior execute will stay present, otherwise the new provided args will be used.
    /// Use ResetStateFor( Reset_ScriptArgs ) before this call if you need empty args.
    /// \note The ValueObjects for the parameters must be in shared state (created with ValueShared or a MakeShared() call issued).
    /// \note The engine will always provide at least an empty args Tuple with argN==0 if an Execute* function with args param is used.
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
    /// \note You must ensure the view ends with a '\0' or '\n' (included in the length())! Please, keep in mind std::string_view is not zero terminated by default!
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

private:
    // small value restore helper class. preliminary, will be moved to some util header.
    template<typename T>
    class ValueRestore
    {
        T const old;
        T &member;
    public:
        ValueRestore( T &mem, T const new_value )
            : old( mem )
            , member( mem )
        {
            member = new_value;
        }
        ~ValueRestore()
        {
            member = old;
        }
        ValueRestore( ValueRestore const && ) = delete;
        ValueRestore &operator=( ValueRestore const && ) = delete;
    };

public:

    /// Runs script \param path as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see ExecuteScript for general information.
    virtual ValueObject RunPreambleScript( std::filesystem::path const &path )
    {
        auto restore_scope_behavior = ValueRestore( mScopeBehavior, eScopeBehavior::ScriptRunsInCurrent );
        auto restore_build_default_args = ValueRestore( mBuildDefaultArgs, false );
        return ExecuteScript( path );
    }

    /// Runs program \param program as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see ExecuteScript for general information.
    virtual ValueObject RunPreambleProgram( StackVM::ProgramPtr const &program )
    {
        auto restore_scope_behavior = ValueRestore( mScopeBehavior, eScopeBehavior::ScriptRunsInCurrent );
        auto restore_build_default_args = ValueRestore( mBuildDefaultArgs, false );
        return ExecuteProgram( program );
    }

    /// Runs content \param rContent as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see EvaluateContent for general information.
    virtual ValueObject RunPreambleContent( Content const &rContent, std::string const &name = "_PREAMBLE_CODE_" )
    {
        auto restore_scope_behavior = ValueRestore( mScopeBehavior, eScopeBehavior::ScriptRunsInCurrent );
        auto restore_build_default_args = ValueRestore( mBuildDefaultArgs, false ); // not needed here currently, but just for the case.
        return EvaluateContent( rContent, name );
    }

    /// Runs code \param code as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see EvaluateContent for general information.
    ValueObject RunPreambleCode( std::string const &code, std::string const &name = "_PREAMBLE_CODE_" )
    {
        return RunPreambleContent( code, name );
    }

    /// Runs code \param code as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \note You must ensure the view ends with a '\0' or '\n' (included in the length())! Please, keep in mind std::string_view is not zero terminated by default!
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see EvaluateContent for general information.
    ValueObject RunPreambleCode( std::string_view const code, std::string const &name = "_PREAMBLE_CODE_" )
    {
        return RunPreambleContent( code, name );
    }

    /// Runs code \param code as a preamble in the current (usually 'global') context for manipulate / preparing the context for the main script.
    /// \param name is arbitrary user defined name for referring to the code.
    /// \note if you want change the script environment between runs, don't forget to call ResetStateFor( Reset_LocalScopes ) to ensure this function will affect the 'global' scope!
    /// \see EvaluateContent for general information.
    template< size_t N >
    ValueObject RunPreambleCode( char const (&code)[N], std::string const &name = "_PREAMBLE_CODE_" )
    {
        return RunPreambleContent( code, name );
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


