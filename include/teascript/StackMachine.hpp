/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "StackVMProgram.hpp"
#include "StackVMConstraints.hpp"
#include "Context.hpp"
#include "CompiledFuncBase.hpp"  // cannot include CompiledFunc.hpp due to mutual dependency :-(
#include "ASTNode.hpp"         // for the StaticExec() functions (sharing same implementation for AST eval and StackMachine execute).
#include "version.h"

#include <functional>
#include <mutex>
#include <array>
#include <chrono>


// clang does not support stop_source/stop_token yet :-(  but we don't want re-invent the wheel only for clang actually!
#if !defined( __clang__ ) || __has_include( <stop_token> )
# define TEASCRIPT_SUSPEND_REQUEST_POSSIBLE    1
#else
# define TEASCRIPT_SUSPEND_REQUEST_POSSIBLE    0
#endif

#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
# include <stop_token>
#endif


// sets the granularity of the max time constraint (default 10).
// every this amount of instructions the actual time is queried for calculate if max time is reached.
// (only relevant if Constraints::Max_Time() is used for Exec/Continue.)
#if !defined( TEASCRIPT_CONSTRAINTS_MAXTIME_GRANULARITY )
# define TEASCRIPT_CONSTRAINTS_MAXTIME_GRANULARITY     10
#endif

// activates measuring the time of every executed instruction and keeps the last 256 in a circular buffer.
#if !defined( TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES )
# define TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES          0
#endif


namespace teascript {
class CompiledFunc;
namespace {
// need an extra function due to mutual dependency :-(
FunctionPtr CompiledFuncFactory( StackVM::ProgramPtr const &program, size_t const start );
}

namespace StackVM {

//                                                               current instr        number      jumped
using CurrentInstrCallback = std::function< void( StackVM::Instruction const &, size_t const, bool const ) >;

/// The TeaStackVM
/// Use thread_support = false for single thread usage. Then the mutex is a no-op and a stop_source/token is not used.
template< bool thread_support >
class Machine
{
public:
    enum class eState
    {
        Stopped,     // stop state, no program present
        Running,     // the execution is ongoing, instructions are actively processed.
        Suspended,   // the execution is actually suspended and can be continued.
        Finished,    // the execution finished normally.
        Halted,      // abnormal program end, HALT instruction was executed or an error occurred.
    };

    enum class eError
    {
        Halted,             // halt instruction was executed.
        Exception,          // exception was caught / would have been thrown.
        StackTooSmall,      // not enough stack elements
        UnknownInstruction, // unknown instruction
        IllegalJump,        // jump to an illegal address.
        NotImplemented,     // instruction (or the compilation) is not implemented (yet).
    };

    struct CallStackEntry
    {
        std::string name;    // a name, can be the function name if it is known.
        size_t      ret;     // ret address to the program before this entry.
        ProgramPtr  prog;    // the program where the code reside which is being executed. (must be always valid.)
        FunctionPtr func;    // function object which is being executed. NOTE: might be nullptr!

        // not sure why clang need this help for construct this simple struct...
#if defined( __clang__ )
    inline CallStackEntry( std::string const &n, size_t r, ProgramPtr const &p, FunctionPtr const &f ) : name(n), ret(r), prog(p), func(f) {}
#endif
    };

private:
    std::vector<ValueObject>    mStack;
    std::vector<CallStackEntry> mCallStack;
    std::optional<ValueObject>  mResult;
    std::optional<eError>       mError;
    std::exception_ptr          mException;

    ProgramPtr                  mProgram;

    std::size_t                 mCurrent = 0;

    // helper struct, 'BasicLocable' for scoped_lock if thread_support == false
    struct NoMutex
    {
        inline void lock() {}
        inline void unlock() {}
    };

    // either using a real mutex or a fake one, depending on thread_support is enabled.
    using Mutex = std::conditional_t<thread_support, std::mutex, NoMutex>;

    Mutex            mutable    mStateMutex;
 
    // actually, clang does not have this :-(
#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
    std::stop_source mutable    mStopSource;
    std::stop_token             mStopToken;
#endif

    eState                      mState = eState::Stopped;

    CurrentInstrCallback        mCurrentInstrCallback;

#if TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES
    std::array< std::pair< eTSVM_Instr, std::chrono::steady_clock::time_point >, 256 > mInstrTimesRingBuffer;
    unsigned char mInstrTimesIndex = 0;  // using 'defined overflow' to wrap around
#endif

public:
    /// Constructs the machine with initial stack size (note: the stack is actually an operand stack and therefore must not be big)
    explicit Machine( std::size_t const initial_stack_size = 64 )
    {
        mStack.reserve( initial_stack_size );
        mCallStack.reserve( 32 ); // hardcoded for now (this size might be reached for recursive calls only)
#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
        if constexpr( thread_support ) {
            mStopToken = mStopSource.get_token();
        }
#endif
    }

    /// \returns whether this instance has thread support.
    constexpr bool HasThreadSupport() const { return thread_support; }
    /// \returns whether this instance on this platform is able to issue suspend requests from another thread.
    constexpr bool SuspendRequestPossible() const { return HasThreadSupport() && 1 == TEASCRIPT_SUSPEND_REQUEST_POSSIBLE; }

    /// \returns the state of the machine.
    eState State() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        return mState;
    }

    // convenience state queries...
    inline bool IsFinished()  const { auto const guard = std::lock_guard( mStateMutex ); return mState == eState::Finished; }
    inline bool IsRunning()   const { auto const guard = std::lock_guard( mStateMutex ); return mState == eState::Running; }
    inline bool IsSuspended() const { auto const guard = std::lock_guard( mStateMutex ); return mState == eState::Suspended; }
    inline bool IsErroneousHalted() const { auto const guard = std::lock_guard( mStateMutex ); return mState == eState::Halted; }

    /// \returns whether there is a result present and can be obtained.
    bool HasResult() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        return (mState == eState::Finished || mState == eState::Suspended) && mResult.has_value();
    }

    /// \return the (eventually) result of the program.
    /// \warning: this function protects against races from Running state, but not against races using the same ValueObject instance!
    std::optional<ValueObject> GetResult() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState != eState::Finished && mState != eState::Suspended ) {
            throw exception::runtime_error( "TeaStackVM is not in finished/suspended state, cannot query result!" );
        }
        return mResult;
    }

    /// \return result of the program as a moved out object. After the call the result in the machine has a NaV!
    /// \throws if state is not finished or there is no result.
    /// \warning: this function protects against races from Running state, but not against races using the same ValueObject instance!
    ValueObject &&MoveResult()
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState != eState::Finished && mState != eState::Suspended ) {
            throw exception::runtime_error( "TeaStackVM is not in finished/suspended state, cannot query result!" );
        }
        if( not mResult.has_value() ) {
            throw exception::runtime_error( "TeaStackVM has not a result!" );
        }
        return std::move( mResult.value() );
    }

    /// \returns whether the machine stopped with an error.
    bool HasError() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        return mState == eState::Halted && mError.has_value();
    }

    /// \returns the present error when in Halted State. The machine must be in halted state.
    eError GetError() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState != eState::Halted ) {
            throw exception::runtime_error( "TeaStackVM is not in halted state, cannot query error!" );
        }
        assert( mError.has_value() ); // must be always true when in Halted state!
        return mError.value();
    }

    /// \throws a possible exception if machine is in halted state.
    void ThrowPossibleErrorException() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState != eState::Halted ) {
            return;
        }
        // do we have an exception object?
        if( mException ) {
            std::rethrow_exception( mException );
        }
        switch( mError.value() ) {
        case eError::Halted:
            throw exception::runtime_error( "Halt instruction was executed." );
        case eError::IllegalJump:
            throw exception::runtime_error( "Jumped to illegal address/position." );
        case eError::NotImplemented:
            throw exception::runtime_error( "Instruction not implemented!" );
        case eError::UnknownInstruction:
            throw exception::runtime_error( "Unknown instruction!" );
        case eError::StackTooSmall:
            throw exception::runtime_error( "Stack too small!" );
        default:
            break;
        }
    }

    /// \returns the (working) variable stack of the machine. This is an EXPERIMENTAL interface and should be only used for debugging!
    /// \warning if the machine change to running state, using the stack will result in data races with undefined behavior!
    std::vector<ValueObject> const &GetStack() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Running || mState == eState::Stopped ) {
            throw exception::runtime_error( "TeaStackVM must not be running or in stop state!" );
        }
        return mStack;
    }

    /// \returns the current call stack of the machine. This is an EXPERIMENTAL interface and should be only used for debugging!
    /// \warning if the machine change to running state, using the stack will result in data races with undefined behavior!
    std::vector<CallStackEntry> const &GetCallStack() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Running || mState == eState::Stopped ) {
            throw exception::runtime_error( "TeaStackVM must not be running or in stop state!" );
        }
        return mCallStack;
    }

    /// \returns the main program. The machine must not be running.
    /// \note the pointer might be a nullptr, e.g., in Stopped state.
    ProgramPtr const &GetMainProgram() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Running ) {
            throw exception::runtime_error( "TeaStackVM must not be running!" );
        }
        return mProgram;
    }

    /// \returns the actual active program of the current function in the call stack. The machine must not be running.
    /// \note in suspended or halted state this might be a subprogram/module and not the main program.
    /// \note the pointer might be a nullptr, e.g., in Stopped state.
    ProgramPtr const &GetCurrentProgram() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Running ) {
            throw exception::runtime_error( "TeaStackVM must not be running!" );
        }
        return not mCallStack.empty() ? mCallStack.back().prog : mProgram;
    }

    /// \returns the 'program counter' where the current program stopped execution. The machine must not be running.
    /// \note for Suspended state the program counter points to the instruction which will be executed next.
    /// \warning The index might be pointing outside the instruction array. This must be checked prior accessing the array.
    size_t GetCurrentInstructionIndex() const
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Running ) {
            throw exception::runtime_error( "TeaStackVM must not be running!" );
        }
        return mCurrent;
    }

    /// Resets the state from the last run for a new program can be exectuted. An eventually result and intermediate state will be cleared.
    /// The machine must not be running when call this function.
    void Reset()
    {
        auto const guard = std::lock_guard( mStateMutex );
        if( mState == eState::Stopped ) {
            return;
        }
        // reset cannot stop the machine from another thread!
        if( mState == eState::Running ) {
            throw exception::runtime_error( "TeaStackVM is running, cannot reset from another thread!" );
        }

        mProgram.reset();
        mStack.clear();
        mCallStack.clear();
        mResult = std::nullopt;
        mError  = std::nullopt;
        mException = nullptr;
        mCurrent = 0;
#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
        if constexpr( thread_support ) {
            mStopSource = std::stop_source();
            mStopToken = mStopSource.get_token();
        }
#endif
        mState = eState::Stopped;
    }


    /// sends a suspend request to the (running) machine from (most likely) a different thread.
    /// \note this function only works if thread_support==true and it has the required platform support (SuspendRequestPossible()==true). 
    /// \returns true if it make sense to wait for the machine reaches suspeneded state, false if a request could not be sent (error).
    /// if the calling thread is a different one, then it may wait until the state is not running anymore.
    /// if the thread is the same (nested call), then the caller must return and the callstack will
    /// leave the Exec() function when the machine is suspended.
    bool Suspend()
    {
#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
        if constexpr( thread_support ) {
            auto const guard = std::lock_guard( mStateMutex );
            if( mState != eState::Running ) {
                return true; // indicates a check for the State makes sense after the call.
            }
            return mStopSource.request_stop();
        } else {
            return false;
        }
#else
        return false;
#endif
    }

    /// Sets a callback function which will be called for each instruction prior its execution.
    void SetCurrentInstrCallback( CurrentInstrCallback callback )
    {
        auto const guard = std::lock_guard( mStateMutex );
        // cannot change callback when running!
        if( mState == eState::Running ) {
            throw exception::runtime_error( "TeaStackVM is running, cannot set callback!" );
        }
        mCurrentInstrCallback = std::move( callback );
    }
    
    /// Starts to execute a new program given by \param rProgram with context \param rContext.
    /// The machine must be in stopped state. This functions blocks until the program execuion is finished 
    /// or an error occured or the program is suspended.
    /// Use a constraint to specify conditions when the program shall be suspended by the machine.
    void Exec( ProgramPtr const &rProgram, Context &rContext, Constraints const &constraint = Constraints::None() )
    {
        {
            auto const  guard = std::lock_guard( mStateMutex );
            if( mState != eState::Stopped ) {
                throw exception::runtime_error( "TeaStackVM must be in Stopped state for Exec()!" );
            }
            if( not rProgram || rProgram->GetCompilerVersion() != version::combined_number() ) {
                throw exception::runtime_error( "StackVM::Machine::Exec(): invalid program!" );
            }

            mProgram = rProgram;
            mCallStack.emplace_back( "<main>", mProgram->GetInstructions().size(), mProgram, nullptr ); // return from main is end
            mState = eState::Running;
        }

#if TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES
        mInstrTimesIndex = 0;
        // 'NotImplemented' acts as a start marker, if index 0 has a different value, the ring buffer wrapped around already.
        mInstrTimesRingBuffer[mInstrTimesIndex++] = std::make_pair( eTSVM_Instr::NotImplemented, std::chrono::steady_clock::now() );
#endif
        
        Exec( rContext, constraint );
    }

    /// Continues a suspended program. The program must be in Suspended state.
    void Continue( Context &rContext, Constraints const &constraint = Constraints::None() )
    {
        {
            auto const  guard = std::lock_guard( mStateMutex );
            if( mState != eState::Suspended ) {
                throw exception::runtime_error( "TeaStackVM must be in Suspended state for Continue()!" );
            }
            mResult = std::nullopt;    // clear a possible yielded result
            mState  = eState::Running;
        }

        Exec( rContext, constraint );
    }

    /// Starts to execute a subroutine from a given program. 
    /// \note Actually the machine must be in Stopped state. This restriction might be lifted in future versions.
    void ExecSubroutine( ProgramPtr const &rProgram, size_t const start, Context &rContext, std::span<ValueObject> const &rParams, SourceLocation const &rLoc )
    {
        {
            auto const  guard = std::lock_guard( mStateMutex );
            if( mState != eState::Stopped ) { // TODO: might be lifted to Running if thread_id is same!
                throw exception::runtime_error( rLoc, "TeaStackVM must be in Stopped state for ExecSubroutine()!" );
            }
            if( not rProgram || rProgram->GetCompilerVersion() != version::combined_number() ) {
                throw exception::runtime_error( rLoc, "StackVM::Machine::ExecSubroutine(): invalid program!" );
            }
            if( start > rProgram->GetInstructions().size() ) {
                throw exception::runtime_error( rLoc, "StackVM::Machine::ExecSubroutine(): Illegal start address!" );
            }

            // setup the stack (actually we assume and mimic a fuctiom call. This might work with arbitrary other code as well but has a dirty stack then)
            mStack.emplace_back();  // one dummy function object.
            for( auto &v : rParams ) {
                mStack.emplace_back( v );
            }
            mStack.emplace_back( ValueObject( static_cast<U64>( rParams.size() ) ) );
            mProgram = rProgram;
            mCurrent = start;
            mCallStack.emplace_back( "<subroutine>", mProgram->GetInstructions().size(), mProgram, nullptr ); // return from main is end

            mState = eState::Running;
        }

        Exec( rContext, Constraints::None() );
    }

private:
    void Exec( Context &rContext, Constraints const &constraint )
    {
        auto const start = std::chrono::steady_clock::now();
        constexpr size_t max_time_granularity = TEASCRIPT_CONSTRAINTS_MAXTIME_GRANULARITY;
        unsigned long long  instr_count = 0; // current instr. count
        [[maybe_unused]] std::size_t  last = 0;   // last instruction before a jump
        bool run = true;
        bool jumped = false;

        auto stack_error = [&]( size_t const n ) -> bool {
            if( mStack.size() < n ) [[unlikely]] {
                run = false;
                mError = eError::StackTooSmall;
                return true;
            }
            return false;
        };

        auto const *program_instr = mCallStack.back().prog->GetInstructions().data();
        auto        program_size  = mCallStack.back().prog->GetInstructions().size();

        while( run && mCurrent < program_size ) {

#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE
            if constexpr( thread_support ) {
                if( mStopToken.stop_requested() ) {
                    run = false;
                    break;
                }
            }
#endif

            if( jumped ) {
                ++instr_count;
            }
            switch( constraint.Kind() ) {
            case Constraints::eKind::InstrCount:
                if( instr_count >= constraint.GetMaxInstr() ) {
                    run = false;
                    continue;
                }
                break;
            case Constraints::eKind::Timed:
                if( instr_count % max_time_granularity == 0 ) {
                    auto const  t  = std::chrono::steady_clock::now();
                    auto const  ms = std::chrono::duration_cast<std::chrono::milliseconds>(t - start);
                    if( ms >= constraint.GetMaxTime() ) {
                        run = false;
                        continue;
                    }
                }
                break;
            default:
                break;
            }

            auto const &current_instr = program_instr[mCurrent];
            if( mCurrentInstrCallback ) {
                mCurrentInstrCallback( current_instr, mCurrent, jumped );
            }
            jumped = false;

            switch( current_instr.instr ) {
            case eTSVM_Instr::HALT:
                run = false;
                mError = eError::Halted;
                continue;
            case eTSVM_Instr::ProgramEnd:
                assert( mCurrent == program_size - 1 );
                break;
            case eTSVM_Instr::NoOp: break;
            case eTSVM_Instr::NoOp_NaV: mStack.emplace_back(); break;
            case eTSVM_Instr::Debug:
                // in payload is the name of the variable/tuple element. (is a NoOp here, just for debugging)
                break;
                // These are all debug only instructions and act like a NoOp
            case eTSVM_Instr::ExprStart:
            case eTSVM_Instr::ExprEnd:
            case eTSVM_Instr::RepeatStart:
            case eTSVM_Instr::RepeatEnd:
            case eTSVM_Instr::If:
            case eTSVM_Instr::Else:
                break;
            case eTSVM_Instr::Push:
                mStack.push_back( current_instr.payload );
                break;
            case eTSVM_Instr::Pop:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                }
                mStack.pop_back();
                break;
            case eTSVM_Instr::Replace:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                }
                mStack.back() = current_instr.payload;
                break;
            case eTSVM_Instr::Load:
                try {
                    mStack.push_back( rContext.FindValueObject( current_instr.payload.template GetValue<std::string>() ) );
                } catch( ... ) {
                    HandleException( std::current_exception() );
                    run = false;
                    continue;
                }
                break;
            case eTSVM_Instr::Stor:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s = mStack.size();
                    auto const &val = mStack[s - 1];
                    auto const &id  = mStack[s - 2];
                    // TODO: Handle rContext.dialect.auto_define_unknown_identifiers on throw unknown_identifier
                    // TODO: can this be merged with ASTNode_Assign ?
                    try {
                        mStack[s - 2] = rContext.SetValue( id.GetValue<std::string>(), val, current_instr.payload.template GetValue<bool>() );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::DefVar:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s = mStack.size();
                    auto       &val = mStack[s - 1];
                    auto const &id  = mStack[s - 2];
                    // TODO: Handle rContext.dialect.auto_define_unknown_identifiers on throw unknown_identifier
                    // TODO: can this be merged with ASTNode_Assign ?
                    bool const shared = current_instr.payload.template GetValue<bool>();
                    if( !shared ) {
                        val.Detach( false ); // make copy (do this unconditional here for ensure the detached value is mutable!)
                    } else if( val.IsShared() && val.IsConst() ) {
                        // FIXME: For function calls we want to show the call side. But the assign is at callee location! (mChildren[1] is then FromParamList, does also not have proper SrcLoc)
                        // TODO: Add SourceLoc of mDebugInfo
                        mException = std::make_exception_ptr( exception::const_shared_assign() );
                        mError = eError::Exception;
                        run = false;
                        continue;
                    }
                    try {
                        mStack[s - 2] = rContext.AddValueObject( id.GetValue<std::string>(), val.MakeShared() );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            // TODO: Streamline DefVar, ConstVar and AutoVar!
            case eTSVM_Instr::ConstVar:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s = mStack.size();
                    auto       &val = mStack[s - 1];
                    auto const &id  = mStack[s - 2];
                    // TODO: Handle rContext.dialect.auto_define_unknown_identifiers on throw unknown_identifier
                    // TODO: can this be merged with ASTNode_Assign ?
                    bool const shared = current_instr.payload.template GetValue<bool>();
                    if( !shared && val.ShareCount() > 1 ) { // only make copy for values living on some store already.
                        val.Detach( true ); // make copy
                    }
                    try {
                        mStack[s - 2] = rContext.AddValueObject( id.GetValue<std::string>(), val.MakeShared().MakeConst() );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::AutoVar:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s = mStack.size();
                    auto       &val = mStack[s - 1];
                    auto const &id  = mStack[s - 2];
                    // TODO: Handle rContext.dialect.auto_define_unknown_identifiers on throw unknown_identifier
                    // TODO: can this be merged with ASTNode_Assign ?
                    bool const shared = current_instr.payload.template GetValue<bool>();
                    if( !shared && val.ShareCount() > 1 ) { // only make copy for values living on some store already.
                        val.Detach( true ); // make copy
                    }
                    try {
                        mStack[s - 2] = rContext.AddValueObject( id.GetValue<std::string>(), val.MakeShared() );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::UndefVar:
                try {
                    auto val = rContext.FindValueObject( current_instr.payload.template GetValue<std::string>() );
                    if( val.IsConst() ) { // TODO: Check if this shall be moved into RemoveValueObject
                        HandleException( std::make_exception_ptr( exception::eval_error( "Variable is const. Const variables cannot be undefined!" ) ) );
                        run = false;
                        continue;
                    }
                    (void)rContext.RemoveValueObject( current_instr.payload.template GetValue<std::string>() );
                    mStack.push_back( ValueObject( true ) );
                } catch( exception::unknown_identifier const & ) {
                    if( rContext.dialect.undefine_unknown_idenitifiers_allowed ) {
                        mStack.push_back( ValueObject( false ) );
                    } else {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                }
                break;
            case eTSVM_Instr::IsDef:
                try {
                    long long scope = 0;
                    (void)rContext.FindValueObject( current_instr.payload.template GetValue<std::string>(), {}, &scope );
                    mStack.push_back( ValueObject( scope ) );
                } catch( exception::unknown_identifier const & ) {
                    mStack.push_back( ValueObject( false ) );
                }
                break;
            case eTSVM_Instr::MakeTuple:
                {
                    auto count = current_instr.payload.template GetValue<U64>();
                    if( stack_error( count ) ) [[unlikely]] {
                        continue;
                    } else {
                        Tuple  tuple;
                        if( count > 1 ) {
                            tuple.Reserve( count );
                        }
                        for( auto idx = mStack.size() - count; idx != mStack.size(); ++idx ) {
                            tuple.AppendValue( mStack[idx].MakeShared() );
                        }
                        while( count > 1 ) {
                            mStack.pop_back();
                            --count;
                        }
                        if( 0 == count ) {
                            mStack.emplace_back();
                        }

                        mStack.back() = ValueObject( std::move( tuple ), ValueConfig{ValueShared,ValueMutable,rContext.GetTypeSystem()} );
                    }
                }
                break;
            case eTSVM_Instr::SetElement:
            case eTSVM_Instr::DefElement:
            case eTSVM_Instr::ConstElement:
                // tuple, name/idx, val
                if( stack_error( 3 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s    = mStack.size();
                    auto       &val = mStack[s - 1];
                    auto const &id  = mStack[s - 2];
                    auto       &obj = mStack[s - 3];

                    if( obj.IsConst() ) {
                        HandleException( std::make_exception_ptr( exception::eval_error( "Tuple is const. Elements cannot be added!" ) ) );
                        run = false;
                        continue;
                    }

                    auto       &tuple = obj.GetValue<Tuple>();
                    bool const shared = current_instr.payload.template GetValue<bool>();

                    size_t idx = static_cast<std::size_t>(-1);
                    if( current_instr.instr == eTSVM_Instr::SetElement ) {
                        idx = SetElement( tuple, id, val, shared );
                    } else {
                        idx = DefElement( tuple, id, val, shared, current_instr.instr == eTSVM_Instr::ConstElement );
                    }
                    if( static_cast<std::size_t>(-1) == idx ) {
                        run = false;
                        continue;
                    }
                    // save the result.
                    mStack[s - 3] = tuple.GetValueByIdx_Unchecked( idx );
                    // stack cleanup, only the new val stays.
                    mStack.pop_back();
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::IsDefElement:
            case eTSVM_Instr::UndefElement:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto &lhs = mStack.back();
                    if( current_instr.instr == eTSVM_Instr::UndefElement && lhs.IsConst() ) {
                        HandleException( std::make_exception_ptr( exception::eval_error( "Tuple is const. Elements cannot be removed!" ) ) );
                        run = false;
                        continue;
                    }
                    auto const &rhs   = current_instr.payload;
                    auto const &tuple = lhs.GetConstValue<Tuple>();

                    std::size_t  idx = static_cast<std::size_t>(-1);
                    if( rhs.GetTypeInfo()->IsSame( TypeString ) ) {
                        idx = tuple.IndexOfKey( rhs.template GetValue<String>() );
                    } else {
                        idx = static_cast<std::size_t>(rhs.GetAsInteger());
                    }
                    if( idx == static_cast<std::size_t>(-1) || not tuple.ContainsIdx( idx ) ) {
                        mStack.back() = ValueObject( false );
                    } else {
                        if( current_instr.instr == eTSVM_Instr::UndefElement ) {
                            lhs.GetMutableValue<Tuple>().RemoveValueByIdx(idx);
                        }
                        mStack.back() = ValueObject( true );
                    }
                }
                break;
            case eTSVM_Instr::SubscriptGet:
                // tuple/buffer, index values count
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto param_count = mStack.back().GetValue<U64>();
                    if( stack_error( param_count + 1 + 1 ) ) [[unlikely]] {
                        continue;
                    }
                    auto const s = mStack.size();
                    auto const &obj = mStack[s - (param_count + 1 + 1)];

                    auto span = std::span<ValueObject const >( &mStack[s - (param_count + 1)], param_count );
                    try {
                        mStack[s - (param_count + 1 + 1)] = ASTNode_Subscript_Operator::GetValueObject( obj, span );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    // cleanup the stack
                    mStack.pop_back(); // param count
                    while( param_count > 0 ) {
                        mStack.pop_back();
                        --param_count;
                    }
                }
                break;
            case eTSVM_Instr::SubscriptSet:
                // tuple/buffer, index values count, value
                if( stack_error( 3 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const s = mStack.size();
                    auto param_count = mStack[s-2].GetValue<U64>();
                    if( stack_error( param_count + 1 + 1 + 1 ) ) [[unlikely]] {
                        continue;
                    }
                    auto  &obj = mStack[s - (param_count + 1 + 1 + 1)];

                    auto span = std::span<ValueObject const >( &mStack[s - (param_count + 1 + 1)], param_count );
                    try {
                        mStack[s - (param_count + 1 + 1 + 1)] = ASTNode_Subscript_Operator::SetValueObject( obj, span, mStack[s-1], current_instr.payload.template GetValue<bool>() ); // TODO: add mDebugInfo SourceLocation!
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    // cleanup the stack
                    mStack.pop_back(); // value
                    mStack.pop_back(); // param count
                    while( param_count > 0 ) {
                        mStack.pop_back();
                        --param_count;
                    }
                }
                break;
            case eTSVM_Instr::UnaryOp:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const &rhs = mStack.back();

                    auto const op = static_cast<ASTNode_Unary_Operator::eOperation>(current_instr.payload.template GetValue<U64>());
                    try {
                        mStack.back() = ASTNode_Unary_Operator::StaticExec( op, rhs );
                    } catch(...){
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                }
                break;
            case eTSVM_Instr::BinaryOp:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s   = mStack.size();
                    auto const &rhs = mStack[s - 1];
                    auto const &lhs = mStack[s - 2];

                    auto const op = static_cast<ASTNode_Binary_Operator::eOperation>(current_instr.payload.template GetValue<U64>());
                    try {
                        mStack[s - 2] = ASTNode_Binary_Operator::StaticExec( op, lhs, rhs );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::IsType:
            case eTSVM_Instr::AsType:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s = mStack.size();
                    auto const &rhs = mStack[s - 1];
                    auto const &lhs = mStack[s - 2];
                    try {
                        if( current_instr.instr == eTSVM_Instr::IsType ) {
                            mStack[s - 2] = ASTNode_Is_Type::StaticExec( lhs, rhs );
                        } else {
                            mStack[s - 2] = ASTNode_As_Type::StaticExec( lhs, rhs );
                        }
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::BitOp:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s = mStack.size();
                    auto const &rhs = mStack[s - 1];
                    auto const &lhs = mStack[s - 2];

                    auto const op = static_cast<ASTNode_Bit_Operator::eBitOp>(current_instr.payload.template GetValue<U64>());
                    try {
                        mStack[s - 2] = ASTNode_Bit_Operator::StaticExec( op, lhs, rhs );
                    } catch( ... ) {
                        HandleException( std::current_exception() );
                        run = false;
                        continue;
                    }
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::DotOp:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s = mStack.size();
                    auto const &rhs = mStack[s - 1];
                    auto const &lhs = mStack[s - 2];
                    
                    if( lhs.InternalType() != ValueObject::TypeTuple ) {
                        HandleException( std::make_exception_ptr( exception::eval_error( "Dot Operator: LHS is not a Tuple/Record/Class/Module/Namespace!" ) ) );
                        run = false;
                        continue;
                    }
                    auto const &tuple = lhs.GetValue<Tuple>();
                    std::size_t  idx = static_cast<std::size_t>(-1);
                    if( rhs.GetTypeInfo()->IsSame( TypeString ) ) {
                        idx = tuple.IndexOfKey( rhs.template GetValue<String>() );
                    } else {
                        idx = static_cast<std::size_t>(rhs.GetAsInteger());
                    }
                    if( idx == static_cast<std::size_t>(-1) ) {
                        HandleException( std::make_exception_ptr( exception::unknown_identifier( rhs.GetValue<String>() ) ) );
                        run = false;
                        continue;
                    } else if( not tuple.ContainsIdx( idx ) ) {
                        HandleException( std::make_exception_ptr( exception::out_of_range() ) );
                        run = false;
                        continue;
                    }
                    auto obj = tuple.GetValueByIdx_Unchecked( idx );
                    if( lhs.IsConst() ) obj.MakeConst();
                    mStack[s - 2] = obj;
                    mStack.pop_back();
                }
                break;
            case eTSVM_Instr::EnterScope:
                rContext.EnterScope();
                break;
            case eTSVM_Instr::ExitScope:
                rContext.ExitScope();
                break;
            case eTSVM_Instr::Test:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const res = mStack.back().GetAsBool();
                    mStack.back() = ValueObject( res );
                }
                break;
            case eTSVM_Instr::JumpRel:
                last = mCurrent;
                mCurrent += current_instr.payload.template GetValue<Integer>();
                jumped = true;
                continue;
            case eTSVM_Instr::JumpRel_If:
            case eTSVM_Instr::JumpRel_IfNot:
            case eTSVM_Instr::TestAndJumpRel_If:
            case eTSVM_Instr::TestAndJumpRel_IfNot:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const res = mStack.back().GetAsBool();
                    if( eTSVM_Instr::TestAndJumpRel_If == current_instr.instr
                        || eTSVM_Instr::TestAndJumpRel_IfNot == current_instr.instr ) {
                        mStack.back() = ValueObject( res ); // save the result.
                    }
                    if( ((eTSVM_Instr::JumpRel_IfNot == current_instr.instr || eTSVM_Instr::TestAndJumpRel_IfNot == current_instr.instr) && not res) ||
                        ((eTSVM_Instr::JumpRel_If == current_instr.instr || eTSVM_Instr::TestAndJumpRel_If == current_instr.instr ) && res) ) {
                        // set to new instruction
                        last = mCurrent;
                        mCurrent += current_instr.payload.template GetValue<Integer>();
                        jumped = true;
                        continue;
                    }
                }
                break;
            case eTSVM_Instr::ForallHead:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s = mStack.size();

                    // get the sequence
                    auto const &seq_val = mStack[s - 1];
                    if( not seq_val.GetTypeInfo()->IsSame<IntegerSequence>() && not seq_val.GetTypeInfo()->IsSame<Tuple>() ) {
                        HandleException( std::make_exception_ptr( exception::eval_error( "Forall loop can actually only iterate over an IntegerSequence/Tuple!" ) ) );
                        run = false;
                        continue;
                    }

                    //FIXME: if seq_val is a sequence already we should use a reference for in later versions it will be possible to manipulate it elsewhere in the loop.
                    auto  seq = seq_val.GetTypeInfo()->IsSame<Tuple>()
                        ? IntegerSequence( 0LL, static_cast<Integer>(seq_val.GetValue<Tuple>().Size() - 1), 1LL )
                        : seq_val.GetValue<IntegerSequence>();
                    seq.Reset();

                    // create the index variable
                    // TODO: add mDebugInfo SourceLocation!
                    mStack[s - 2] = rContext.AddValueObject( mStack[s - 2].GetValue<std::string>(), ValueObject( seq.Current(), ValueConfig(ValueShared, ValueMutable)));

                    // store the seqeunce
                    mStack[s - 1] = ValueObject( std::move(seq), ValueConfig(ValueShared, ValueMutable));
                }
                break;
            case eTSVM_Instr::ForallNext:
                if( stack_error( 3 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const  s = mStack.size();
                    auto &seq = mStack[s - 2].GetValue<IntegerSequence>();
                    if( seq.Next() ) {
                        mStack[s - 3].AssignValue( seq.Current() );
                        mStack.pop_back(); // clear previous result
                    } else {
                        // forall is done, cleanup stack and set instruction to end of loop.
                        mStack[s - 3] = std::move( mStack.back() ); // carry result.
                        mStack.pop_back(); // clear working stack (idx and seq)
                        mStack.pop_back();
                        // jump
                        last = mCurrent;
                        mCurrent += current_instr.payload.template GetValue<Integer>();
                        jumped = true;
                        continue;
                    }
                }
                break;
            case eTSVM_Instr::CallFunc:
                if( stack_error( 2 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto param_count = mStack.back().GetValue<U64>();
                    if( stack_error( param_count + 1 + 1 ) ) [[unlikely]] {
                        continue;
                    }
                    auto func = mStack[ mStack.size() - (param_count + 1 + 1)].GetValueCopy<FunctionPtr>();
                    auto cfunc = std::dynamic_pointer_cast<CompiledFuncBase>(func);
                    if( cfunc ) {
                        mCallStack.emplace_back( current_instr.payload.template GetValue<std::string>(), mCurrent + 1, cfunc->GetProgram(), func );
                        program_instr = mCallStack.back().prog->GetInstructions().data();
                        program_size  = mCallStack.back().prog->GetInstructions().size();
                        last = mCurrent;
                        mCurrent = cfunc->GetStartAddress();
                        jumped = true;
                        continue;
                    } else {
                        mStack.pop_back();
                        std::vector< ValueObject > params; //TODO optimize this. Try to use a span over the stack elements!
                        params.reserve( param_count );
                        for( auto idx = 0ULL; idx < param_count; ++idx ) {
                            params.emplace_back( std::move( mStack[mStack.size() - param_count + idx] ) );
                        }
                        while( param_count > 0 ) {
                            mStack.pop_back();
                            --param_count;
                        }
                        try {
                            mStack.back() = func->Call( rContext, params, SourceLocation() );
                        } catch( control::Exit_Script &rExit ) { // for the case _Exit is used in not compild functions
                            mStack.back() = rExit.MoveResult();
                            HandleExit( rContext );
                            // jump to program end.
                            last = mCurrent;
                            program_instr = mCallStack[0].prog->GetInstructions().data();
                            program_size  = mCallStack[0].prog->GetInstructions().size();
                            mCurrent = mCallStack[0].ret;
                            jumped = true;
                            continue;
                        } catch( ... ) {
                            HandleException( std::current_exception() );
                            run = false;
                            continue;
                        }
                    }
                }
                break;
            case eTSVM_Instr::ParamList:
                break;
            case eTSVM_Instr::FuncDef:
                {
                    auto func = CompiledFuncFactory(mCallStack.back().prog, mCurrent+2);
                    ValueObject  val{std::move( func ), ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() )};
                    auto const &name = current_instr.payload.template GetValue<std::string>();
                    if( name != "<lambda>" ) {
                        try {
                            rContext.AddValueObject( name, val, SourceLocation() );
                        } catch( ... ) {
                            HandleException( std::current_exception() );
                            run = false;
                            continue;
                        }
                        mStack.emplace_back( ValueObject( true ) ); // make it usable in boolean expressions: use_xxx and (func test(a) {a*a})
                    } else {
                        mStack.emplace_back( std::move( val ) );
                    }
                }
                break;
            case eTSVM_Instr::Ret:
                if( mCallStack.empty() ) [[unlikely]] {
                    // TODO: Add SourceLoc of mDebugInfo
                    mException = std::make_exception_ptr( exception::runtime_error( "No ret address for return from function!" ) );
                    mError = eError::Exception;
                    run = false;
                    continue;
                } else {
                    last = mCurrent;
                    mCurrent = mCallStack.back().ret;
                    if( mCallStack.size() > 1 ) { // don't remove 'main' here
                        mCallStack.pop_back();
                    }
                    program_instr = mCallStack.back().prog->GetInstructions().data();
                    program_size  = mCallStack.back().prog->GetInstructions().size();
                    jumped = true;
                    continue;
                }
                break;
            case eTSVM_Instr::ParamSpec:
                if( stack_error( 1 ) ) [[unlikely]] {
                    continue;
                } else {
                    // we need a working copy of the param count for decrement!
                    mStack.emplace_back( mStack.back().GetValue<U64>() );
                }
                break;
            case eTSVM_Instr::ParamSpecClean:
                // at least working param count, orig param count and function object
                if( stack_error( 3 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const left_params = mStack.back().GetValue<U64>();
                    if( left_params != 0 ) [[unlikely]] {
                        //NOTE: we need SourceLoc of the caller! But the caller is different for each call.
                        //      Here we know that callstack is at least 2 (we and the caller). The ret address - 1 is the CallFunc instruction which might carry a source loc.
                        auto const &loc = mCallStack[mCallStack.size() - 2].prog->GetSourceLocationFor( mCallStack[mCallStack.size() - 1].ret - 1 );
                        HandleException( std::make_exception_ptr( exception::eval_error( loc, "Too many arguments for function call!" ) ) );
                        run = false;
                        continue;
                    } else {
                        auto orig_params = mStack[mStack.size() - 2].GetValue<U64>();
                        mStack.pop_back(); // working param count
                        mStack.pop_back(); // orig param count
                        while( orig_params > 0 ) {  // each param
                            mStack.pop_back();
                            --orig_params;
                        }
                        mStack.pop_back(); // function object
                    }
                }
                break;
            case eTSVM_Instr::FromParam:
                // current id, working param count, orig param count and (at least) one value
                if( stack_error( 4 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const pos = mStack.size() - 2;
                    auto param_count = mStack[pos].GetValue<U64>();
                    if( param_count < 1 ) [[unlikely]] { // huh?
                        //NOTE: we need SourceLoc of the caller! But the caller is different for each call.
                        //      Here we know that callstack is at least 2 (we and the caller). The ret address - 1 is the CallFunc instruction which might carry a source loc.
                        auto const &loc = mCallStack[mCallStack.size()-2].prog->GetSourceLocationFor( mCallStack[mCallStack.size()-1].ret - 1 );
                        HandleException( std::make_exception_ptr( exception::eval_error( loc, "Too less arguments for function call!" ) ) );
                        run = false;
                        continue;
                    } else if( stack_error( 3 + param_count ) ) [[unlikely]] {
                        continue;
                    } else {
                        // add current param to end (after the id)
                        mStack.push_back( std::move( mStack[pos - (1 + param_count)] ) );
                        // decrement param count by one and save it.
                        --param_count;
                        mStack[pos].AssignValue( param_count );
                    }
                }
                break;
            case eTSVM_Instr::FromParam_Or:
                // current id, working param count, orig param count
                if( stack_error( 3 ) ) [[unlikely]] {
                    continue;
                } else {
                    auto const pos = mStack.size() - 2;
                    auto param_count = mStack[pos].GetValue<U64>();
                    if( param_count > 0 ) { // there are still parameters given by the caller.
                        // add current param to end (after the id)
                        mStack.push_back( std::move( mStack[pos - (1 + param_count)] ) );
                        // decrement param count by one and save it.
                        --param_count;
                        mStack[pos].AssignValue( param_count );
                        // jump over the default param code
                        last = mCurrent;
                        mCurrent += current_instr.payload.template GetValue<Integer>();
                        jumped = true;
                        continue;
                    } // else: no parameter present, execute the param or instructions for default param.
                }
                break;
            case eTSVM_Instr::ExitProgram:
                HandleExit( rContext );
                // jump to program end.
                last = mCurrent;
                program_instr = mCallStack[0].prog->GetInstructions().data();
                program_size = mCallStack[0].prog->GetInstructions().size();
                mCurrent = mCallStack[0].ret;
                jumped = true;
                continue;
            case eTSVM_Instr::Suspend:
                if( constraint.Kind() == Constraints::eKind::AutoContinue ) {
                    ; // nop
                } else {
                    run = false;
                }
                break;
            case eTSVM_Instr::Yield:
                if( not mStack.empty() ) {
                    mResult = std::move( mStack.back() );
                    // dont't pop here for not break other inserted cleanup code! (every statement must have a result, actually!)
                }
                run = false;
                break;
            case eTSVM_Instr::NotImplemented:
                mError = eError::NotImplemented;
                run = false;
                break;
            default:
                mError = eError::UnknownInstruction;
                run = false;
            }

#if TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES
            mInstrTimesRingBuffer[mInstrTimesIndex++] = std::make_pair( current_instr.instr, std::chrono::steady_clock::now() );
#endif
            ++instr_count;
            ++mCurrent;
        }


        auto const guard = std::lock_guard( mStateMutex );

        if( mError.has_value() ) {
            mState = eState::Halted;
            return;
        } else if( not run ) {
            mState = eState::Suspended;
            return;
        }

        // == size --> reached program end
        if( not mCallStack.empty() && mCurrent > mCallStack.back().prog->GetInstructions().size() ) {
            mError = eError::IllegalJump;
            mState = eState::Halted;
            return;
        }

        if( mCallStack.size() == 1 ) {
            mCallStack.pop_back();
        } else {
            mException = std::make_exception_ptr( exception::runtime_error("CallStack != 1, Ret instruction missing?!") );
            mError = eError::Exception;
            mState = eState::Halted;
            return;
        }

        if( not mStack.empty() ) {
            mResult = std::move( mStack.back() );
            mStack.pop_back();
        }

        mState = eState::Finished;
    }


    void HandleExit( Context &rContext )
    {
        // save result and clean stack
        if( mStack.size() > 1 ) {
            mStack[0] = std::move( mStack[mStack.size() - 1] );
            while( mStack.size() > 1 ) {
                mStack.pop_back();
            }
        }
        // clean scope.
        while( rContext.LocalScopeCount() > 0 ) {
            rContext.ExitScope();
        }
        // clean call stack
        while( mCallStack.size() > 1 ) {
            mCallStack.pop_back();
        }
    }

    size_t SetElement( Tuple &rTuple, ValueObject const &rId, ValueObject &val, bool const shared )
    {
        std::size_t  idx = static_cast<std::size_t>(-1);
        if( rId.GetTypeInfo()->IsSame( TypeString ) ) {
            idx = rTuple.IndexOfKey( rId.GetValue<std::string>() );
        } else {
            idx = static_cast<std::size_t>(rId.GetAsInteger());
        }
        if( idx == static_cast<std::size_t>(-1) ) {
            HandleException( std::make_exception_ptr( exception::unknown_identifier( rId.GetValue<std::string>() ) ) );
            return idx;
        } else if( not rTuple.ContainsIdx( idx ) ) {
            HandleException( std::make_exception_ptr( exception::out_of_range() ) );
            return static_cast<std::size_t>(-1);
        }
        try {
            if( shared ) {
                rTuple.GetValueByIdx_Unchecked( idx ).SharedAssignValue( val );
            } else {
                rTuple.GetValueByIdx_Unchecked( idx ).AssignValue( val );
            }
        } catch( ... ) {
            HandleException( std::current_exception() );
            return static_cast<std::size_t>(-1);
        }
        return idx;
    }

    size_t DefElement( Tuple &rTuple, ValueObject const &rId, ValueObject &val, bool const shared, bool const as_const )
    {
        auto  idx = static_cast<std::size_t>(-1);
        if( not as_const ) {
            if( not shared ) {
                val.Detach( false ); // make copy (do this unconditional here for ensure the detached value is mutable!)
            } else if( val.IsShared() && val.IsConst() ) {
                HandleException( std::make_exception_ptr( exception::const_shared_assign() ) );// (TODO: see FIXME in ASTNode!)
                return idx;
            }
        } else {
            if( not shared && val.ShareCount() > 1 ) { // only make copy for values living on some store already.
                val.Detach( true ); // make copy
            }
        }
        if( rId.GetTypeInfo()->IsSame( TypeString ) ) {
            std::string const &identifier = rId.GetValue<std::string>();
            if( not rTuple.AppendKeyValue( identifier, as_const ? val.MakeShared().MakeConst() : val.MakeShared() ) ) {
                HandleException( std::make_exception_ptr( exception::redefinition_of_variable( identifier ) ) );
                return idx;
            }
            idx = rTuple.IndexOfKey( identifier );
        } else {
            idx = static_cast<std::size_t>(rId.GetAsInteger());
            if( idx > rTuple.Size() ) {
                HandleException( std::make_exception_ptr( exception::out_of_range() ) );
                idx = static_cast<std::size_t>(-1);
            } else if( idx != rTuple.Size() ) {
                HandleException( std::make_exception_ptr( exception::redefinition_of_variable( std::to_string( idx ) ) ) );
                idx = static_cast<std::size_t>(-1);
            } else {
                rTuple.AppendValue( as_const ? val.MakeShared().MakeConst() : val.MakeShared() );
            }
        }
        return idx;
    }

    void HandleException( std::exception_ptr eptr )
    {
        assert( static_cast<bool>(eptr) ); // this function only makes sense if there is an exception!
        // try to inject SourceLocation
        //SourceLocation const &loc = mCallStack.back().prog->GetBestMatchingSourceLocationFor( mCurrent );
        // all places which might throw an teascript based exception should have an exact matching debug info or none.
        SourceLocation const &loc = mCallStack.back().prog->GetSourceLocationFor( mCurrent );
        if( loc.IsSet() ) {
            try {
                try {
                    std::rethrow_exception( eptr );
                } catch( exception::runtime_error &ex ) {
                    ex.SetSourceLocation( loc ); // inject and throw as original type.
                    throw;
                } catch( std::exception const &ex ) {
                    // make a teascript::runtime_error with set source!
                    mException = std::make_exception_ptr( exception::runtime_error( loc, ex.what() ) );
                    mError = eError::Exception;
                }
            } catch( ... ) {
                mException = std::current_exception();
                mError = eError::Exception;
            }
        } else {
            mException = eptr;
            mError = eError::Exception;
        }
    }


#if TEASCRIPT_ENABLE_STACKVM_INSTR_TIMES
public:
    void DumpInstrTimes( double const threshold = -1.0 )
    {
        // just a basic protect here, because it is only a debugging function...
        {
            auto const guard = std::lock_guard( mStateMutex );
            if( mState == eState::Running || mState == eState::Stopped ) {
                return;
            }
        }
        // check if wrapped around...
        if( mInstrTimesRingBuffer[0].first == eTSVM_Instr::NotImplemented || mInstrTimesIndex == 0 ) { // not yet or exactly, simple...
            auto const end = mInstrTimesIndex == 0 ? mInstrTimesRingBuffer.size() : mInstrTimesIndex;
            for( int i = 1; i < end; ++i ) {
                auto const  t = std::chrono::duration<double>( mInstrTimesRingBuffer[i].second - mInstrTimesRingBuffer[i - 1].second ).count();
                if( t > threshold ) {
                    printf( "%s: %.8f\n", Instruction::ToString( mInstrTimesRingBuffer[i].first ).data(), t );
                }
            }
        } else { // wrapped around.
            for( int i = mInstrTimesIndex; i < mInstrTimesRingBuffer.size(); ++i ) {
                auto const  t = std::chrono::duration<double>( mInstrTimesRingBuffer[i].second - mInstrTimesRingBuffer[i - 1].second ).count();
                if( t > threshold ) {
                    printf( "%s: %.8f\n", Instruction::ToString( mInstrTimesRingBuffer[i].first ).data(), t );
                }
            }
            for( int i = 1; i < mInstrTimesIndex; ++i ) {
                auto const  t = std::chrono::duration<double>( mInstrTimesRingBuffer[i].second - mInstrTimesRingBuffer[i - 1].second ).count();
                if( t > threshold ) {
                    printf( "%s: %.8f\n", Instruction::ToString( mInstrTimesRingBuffer[i].first ).data(), t );
                }
            }
        }

    }
#endif
};

} // namespace StackVM

} // namesapce teascript

// we need to ensure to have the factory... :-(
#include "CompiledFunc.hpp"
