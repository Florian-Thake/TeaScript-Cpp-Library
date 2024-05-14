/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#pragma once

#include <memory> // std::shared_ptr
#include <atomic> // std::atomic_bool

#include "teascript/Context.hpp"
#include "teascript/ConfigEnums.hpp" // could be forward declared but user needs it for the ::Build() function below anyway.
#include "teascript/Content.hpp"     // could be forward declared but user needs it for the ::Build() function below anyway.
#include "teascript/StackVMConstraints.hpp" // could be forward declared but user needs it for the ::RunFor() function below anyway.

namespace teascript {

// bunch of forward declarations...
namespace StackVM {
template<bool> class Machine;
class Program;
using ProgramPtr = std::shared_ptr<Program>;
} // namespace StackVM


/// The class CoroutineScriptEngine can be used for execute TeaScript code similar
/// like coroutines. The scripts can be suspended (by themselves, by constraints or 
/// by request) and they are able to yield values at any point and continue execution
/// from that position.
/// This class is thread-safe in the terms of the coroutine execution state.
/// However, the coroutine execution itself is single-threaded. Only one thread is allowed
/// to execute the coroutine of one distinct CoroutineScriptEngine instance.
/// \warning Furthermore, querying or modifying the context is not thread-safe. Only one
/// thread is allowed to use the Context the _same_ time!
/// \note The context is not shared. Each instance will use its own private context.
/// \note This class and its interface/layout are considered EXPERIMENTAL.
class CoroutineScriptEngine
{
protected:
    std::atomic_bool                         mRunning = false;
    Context                                  mContext;
    std::shared_ptr<StackVM::Machine<true>>  mMachine;

    // lil' helper for exception safe reset of running flag.
    struct ScopedRunning
    {
        std::atomic_bool &b;
        ScopedRunning( std::atomic_bool &b_ ) : b( b_ ) {}
        ~ScopedRunning() { b.store( false ); }
    };

public:
    /// Default constructor will bootstrap the full Core Library into the context. No coroutine loaded yet.
    CoroutineScriptEngine();
    /// Will use the given context as the context for the coroutine (see class teascript::ContextFactory for a handy helper.)
    /// \note any prior existing local scope will be removed from the context.
    explicit CoroutineScriptEngine( Context &&rContext );
    /// Will prepare to execute the given program as coroutine and bootstrap the full Core Library into the context.
    explicit CoroutineScriptEngine( StackVM::ProgramPtr const &coroutine );
    /// Will prepare to execute the given program as coroutine and use the given context as the context for the coroutine.
    /// \note any prior existing local scope will be removed from the context.
    CoroutineScriptEngine( StackVM::ProgramPtr const &coroutine, Context &&rContext );

    /// builds a coroutine program from given source. 
    /// \see also teascript::StackVM::Program::Load()
    /// \see also class teascript::Engine for more possibilities to compile a program.
    static StackVM::ProgramPtr Build( Content const &rContent, eOptimize const opt_level = eOptimize::O0, std::string const &name = "_USER_CORO_" );

    /// Will prepare to execute the given program as coroutine, the old coroutine will be removed.
    /// The current coroutine must not be running actually!
    void ChangeCoroutine( StackVM::ProgramPtr const &coroutine );

    /// Resets state and prepares actual set coroutine for execution. same as ChangeCoroutine( old_coroutine ).
    void Reset();

    /// \returns whether the coroutine is neither running, nor yet finished and no error occurred, so that in can be continued (e.g., for yielding more values)
    bool CanBeContinued() const;

    /// \returns whether the coroutine is completely finished (no more values can be yielded / no instruction left to be executed).
    /// \note depending on the coroutine code this state might be never reached!
    bool IsFinished() const;

    /// \returns whether the actual set coroutine is running, i.e. a thread is inside Run()/operator()/RunFor() or ChangeCoroutine().
    inline
    bool IsRunning() const
    {
        return mRunning.load();
    }


    /// \returns whether on this platform it is possible to send a suspend request to a running coroutine from another thread.
    /// \note: see class teascript::StackVM::Machine for details.
    bool IsSuspendRequestPossible() const;

    /// sends a suspend request to the (running) coroutine from (most likely) a different thread.
    /// \returns true if it make sense to wait for coroutine is suspeneded, false if a request could not be sent (error).
    /// \see class teascript::StackVM::Machine::Suspend() for details.
    bool Suspend() const;

    /// Runs the coroutine until yield, suspend, finished or error occurred.
    /// \returns the yielded value if any, or NaV (Not A Value).
    /// \throws exception::runtime_error or a derived class, or (rarely) only a std::exception based exception.
    /// \note A running coroutine can be suspended from another thread via Suspend() if IsSuspendRequestPossible() returns true.
    ValueObject Run();

    /// Runs the coroutine until yield, suspend, finished or error occurred.
    /// \returns the yielded value if any, or NaV (Not A Value).
    /// \throws exception::runtime_error or a derived class, or (rarely) only a std::exception based exception.
    /// \note A running coroutine can be suspended from another thread via Suspend() if IsSuspendRequestPossible() returns true.
    ValueObject operator()()
    {
        return Run();
    }

    /// Runs the coroutine until constraint reached or yield, suspend, finished or error occurred.
    /// \returns the yielded value if any, or NaV (Not A Value).
    /// \throws exception::runtime_error or a derived class, or (rarely) only a std::exception based exception.
    /// \note A running coroutine can be suspended from another thread via Suspend() if IsSuspendRequestPossible() returns true.
    ValueObject RunFor( StackVM::Constraints const &constraint );

    /// Adds given ValueObjects as a tuple "args[idx]". Additionally adding an "argN" variable indicating the parameter amount. The coroutine must be suspended.
    /// The ValueObjects for the parameters must be in shared state (created with ValueShared or a MakeShared() call issued).
    /// \note this function is _not_ thread safe. Only one thread is allowed to call this function the _same_ time and the coroutine must not be running!
    void SetInputParameters( std::vector<ValueObject> const &params );

    /// Adds given parameters as ValueObjects as a tuple "args[idx]". Additionally adding an "argN" variable indicating the parameter amount. The coroutine must be suspended.
    /// \note this function is _not_ thread safe. Only one thread is allowed to call this function the _same_ time and the coroutine must not be running!
    template< typename ...T> requires ( (not std::is_same_v<T, ValueObject> && not std::is_same_v<T, std::vector<ValueObject>> && std::is_constructible_v<ValueObject, T, ValueConfig>) && ...)
    void SetInputParameters( T ... t )
    {
        std::vector<ValueObject> params{ ValueObject( std::forward<T>( t ), ValueConfig{ValueShared,ValueMutable} )... };
        SetInputParameters( params );
    }
};

} // namespace teascript


#if !defined(TEASCRIPT_DISABLE_HEADER_ONLY)
# define TEASCRIPT_DISABLE_HEADER_ONLY          0
#endif

// check for broken header only / not header only configurations.
#if (0==TEASCRIPT_DISABLE_HEADER_ONLY) && defined(TEASCRIPT_INCLUDE_DEFINITIONS)
# error header only config broken, TEASCRIPT_DISABLE_HEADER_ONLY is 0 but TEASCRIPT_INCLUDE_DEFINITIONS is defined.
#endif

#if (!TEASCRIPT_DISABLE_HEADER_ONLY)  || TEASCRIPT_INCLUDE_DEFINITIONS
# define TEASCRIPT_COROUTINE_ENGINE_IMPL        1    /* just a guard */
# include "CoroutineScriptEngine_Impl.ipp"
# undef TEASCRIPT_COROUTINE_ENGINE_IMPL
#endif
