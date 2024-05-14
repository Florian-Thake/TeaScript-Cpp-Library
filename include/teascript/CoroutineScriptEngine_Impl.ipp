/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#pragma once


#if !TEASCRIPT_COROUTINE_ENGINE_IMPL
# error This file is not meant to be included on user level. Include CoroutineScriptEngine.hpp
#endif

#include "teascript/CoroutineScriptEngine.hpp" // for improve usability in IDE
#include "teascript/StackMachine.hpp"
#include "teascript/CoreLibrary.hpp"
#include "teascript/Parser.hpp"
#include "teascript/StackVMCompiler.hpp"


namespace teascript {

CoroutineScriptEngine::CoroutineScriptEngine()
    : mMachine( std::make_shared<teascript::StackVM::Machine<true>>() )
{
    CoreLibrary().Bootstrap( mContext, config::full() ); // we always use the complete core library by default.
}

CoroutineScriptEngine::CoroutineScriptEngine( Context &&rContext )
    : mContext( std::move( rContext ) )
    , mMachine( std::make_shared<teascript::StackVM::Machine<true>>() )
{
}

CoroutineScriptEngine::CoroutineScriptEngine( StackVM::ProgramPtr const &coroutine )
    : mMachine( std::make_shared<teascript::StackVM::Machine<true>>() )
{
    CoreLibrary().Bootstrap( mContext, config::full() ); // we always use the complete core library by default.
    ChangeCoroutine( coroutine );
}

CoroutineScriptEngine::CoroutineScriptEngine( StackVM::ProgramPtr const &coroutine, Context &&rContext )
    : mContext( std::move( rContext ) )
    , mMachine( std::make_shared<teascript::StackVM::Machine<true>>() )
{
    ChangeCoroutine( coroutine );
}

/*static*/
StackVM::ProgramPtr CoroutineScriptEngine::Build( Content const &rContent, eOptimize const opt_level, std::string const &name )
{
    Parser p;
    p.SetDebug( opt_level == eOptimize::Debug );
    StackVM::Compiler c;
    return c.Compile( p.Parse( rContent, name ), opt_level );
}

void CoroutineScriptEngine::ChangeCoroutine( StackVM::ProgramPtr const &coroutine )
{
    bool expected = false;
    if( not mRunning.compare_exchange_strong( expected, true ) ) {
        throw exception::runtime_error( "Coroutine is running! Cannot call ChangeCoroutine()!" );
    }

    ScopedRunning const  guard( mRunning );

    mMachine->Reset();
    // cleanup old local scopes
    while( mContext.LocalScopeCount() > 0 ) {
        mContext.ExitScope();
    }
    // every coroutine will run in its own new local scope, so that the global scope will not become dirty.
    mContext.EnterScope();
    mMachine->Exec( coroutine, mContext, StackVM::Constraints::MaxInstructions( 0 ) ); // just setup everything and stop prior the first instruction!
    mMachine->ThrowPossibleErrorException();
}

void CoroutineScriptEngine::Reset()
{
    auto current = mMachine->GetMainProgram(); // copy is intended!
    ChangeCoroutine( current );
}

bool CoroutineScriptEngine::CanBeContinued() const
{
    // NOTE: the potential race between && is ok, the state may change after the call anyway.
    //       The target is to protect RunFor + ChangeCoroutine!
    return not IsRunning() && mMachine->IsSuspended();
}

bool CoroutineScriptEngine::IsFinished() const
{
    // NOTE: the potential race between && is ok, the state may change after the call anyway.
    //       The target is to protect RunFor + ChangeCoroutine!
    return not IsRunning() && mMachine->IsFinished();
}

bool CoroutineScriptEngine::IsSuspendRequestPossible() const
{
    return mMachine->SuspendRequestPossible();
}

bool CoroutineScriptEngine::Suspend() const
{
    return mMachine->Suspend();
}

ValueObject CoroutineScriptEngine::Run()
{
    return RunFor( StackVM::Constraints::None() );
}

ValueObject CoroutineScriptEngine::RunFor( StackVM::Constraints const &constraint )
{
    bool expected = false;
    if( not mRunning.compare_exchange_strong( expected, true ) ) {
        throw exception::runtime_error( "Coroutine is running (or exchanging)! Cannot call RunFor()!" );
    }

    ScopedRunning const  guard( mRunning );

    mMachine->Continue( mContext, constraint );
    mMachine->ThrowPossibleErrorException();
    if( mMachine->HasResult() ) {
        return mMachine->MoveResult();
    }
    return {};
}

void CoroutineScriptEngine::SetInputParameters( std::vector<ValueObject> const &params )
{
    // NOTE: This checks don't make the call threadsafe, it is just to potentially detect wrong usage.
    if( IsRunning() || mMachine->IsRunning() ) {
        throw exception::runtime_error( "TeaStackVM must not be running!" );
    }

    mContext.SetScriptArgs( params );
}


} // namespace teasript
