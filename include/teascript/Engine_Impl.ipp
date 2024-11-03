/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


#if !TEASCRIPT_ENGINE_IMPL
# error This file is not meant to be included on user level. Include Engine.hpp
#endif

#include "Engine.hpp"  // for improve usability of IDE
#include "Parser.hpp"
#include "CoreLibrary.hpp"
#include "StackVMCompiler.hpp"
#include "StackMachine.hpp"

namespace teascript {

// definition of the BuildTools struct
struct Engine::BuildTools
{
    Parser             mParser;
    StackVM::Compiler  mCompiler;
    std::shared_ptr<StackVM::Machine<true>> mMachine;

    inline BuildTools() : mParser(), mCompiler(), mMachine( std::make_shared<StackVM::Machine<true>>() ) {}
};

TEASCRIPT_COMPILE_MODE_INLINE
Engine::Engine( bool const bootstrap, config::eConfig const config, eMode const mode, eOptimize const opt_level )
    : EngineBase()
    , mMode( mode )
    , mOptLevel( opt_level )
    , mCoreConfig( config )
    , mContext()
    , mBuildTools( std::make_shared<BuildTools>() )
{
    if( bootstrap ) {
        CoreLibrary().Bootstrap( mContext, mCoreConfig, mMode == eMode::Eval );
#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW
        WebPreviewModule().Load( mContext, mCoreConfig, mMode == eMode::Eval );
#endif
    }
}

TEASCRIPT_COMPILE_MODE_INLINE
void Engine::AddValueObject( std::string const &rName, ValueObject val )
{
    (void)mContext.AddValueObject( rName, val );
}

TEASCRIPT_COMPILE_MODE_INLINE
ValueObject Engine::EvaluateContent( Content const &rContent, std::string const &rName )
{
    try {
        auto const ast = mBuildTools->mParser.Parse( rContent, rName );
        if( mMode == eMode::Eval ) {
            return ast->Eval( mContext );
        } else {
            auto const program = mBuildTools->mCompiler.Compile( ast, mOptLevel );
            mBuildTools->mMachine->Reset();
            mBuildTools->mMachine->Exec( program, mContext );
            mBuildTools->mMachine->ThrowPossibleErrorException();
            if( mBuildTools->mMachine->HasResult() ) {
                return mBuildTools->mMachine->MoveResult();
            }
            return {}; // NaV (Not a Value)
        }
    } catch( teascript::control::ControlBase const & ) {
        throw exception::runtime_error( "A TeaScript control flow exception escaped. Check for wrong named loop labels!" );
    }
}

TEASCRIPT_COMPILE_MODE_INLINE
Engine::Engine(config::eConfig const config, eMode const mode )
    : EngineBase()
    , mMode( mode )
    , mCoreConfig( config )
    , mContext()
    , mBuildTools( std::make_shared<BuildTools>() )
{
    CoreLibrary().Bootstrap( mContext, mCoreConfig, mMode == eMode::Eval );
#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW
    WebPreviewModule().Load( mContext, mCoreConfig, mMode == eMode::Eval );
#endif
}

TEASCRIPT_COMPILE_MODE_INLINE
void Engine::ResetState()
{
    mBuildTools->mMachine->Reset();
    mBuildTools->mParser.ClearState();
    CoreLibrary().Bootstrap( mContext, mCoreConfig, mMode == eMode::Eval );
#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW
    WebPreviewModule().Load( mContext, mCoreConfig, mMode == eMode::Eval );
#endif
}

TEASCRIPT_COMPILE_MODE_INLINE
void Engine::SetDebugMode( bool const enabled ) noexcept
{
    mBuildTools->mParser.SetDebug( enabled );
    mContext.is_debug = enabled;
    mOptLevel = enabled ? eOptimize::Debug : eOptimize::O0;
}

TEASCRIPT_COMPILE_MODE_INLINE
ValueObject Engine::GetVar(std::string const &rName) const
{
    return mContext.FindValueObject( rName );
}

TEASCRIPT_COMPILE_MODE_INLINE
ValueObject Engine::CallFunc( std::string const &rName, std::vector<ValueObject> &rParams )
{
    auto funcval = GetVar( rName );
    auto func = funcval.GetValue< FunctionPtr >(); // copy is intended
    return func->Call( mContext, rParams, SourceLocation() );
}

TEASCRIPT_COMPILE_MODE_INLINE
ValueObject Engine::ExecuteScript( std::filesystem::path const &path, std::vector<ValueObject> const &args )
{
    // build utf-8 filename again... *grrr*
    auto const filename = util::utf8_path_to_str( path );
    auto const val = CoreLibrary::ReadTextFileEx( path );
    if( val.GetTypeInfo()->IsSame<String>() ) {
        if( not args.empty() ) {
            mContext.SetScriptArgs( args, 1 == TEASCRIPT_ENGINE_USE_LEGACY_ARGS );
        }

        Content  content{val.GetValue<String>()};
        return EvaluateContent( content, filename ); // TODO: absolute path or filename?
    }

    throw exception::load_file_error( filename );
}

TEASCRIPT_COMPILE_MODE_INLINE
ValueObject Engine::ExecuteProgram( StackVM::ProgramPtr const &program, std::vector<ValueObject> const &args )
{
    if( not args.empty() ) {
        mContext.SetScriptArgs( args, 1 == TEASCRIPT_ENGINE_USE_LEGACY_ARGS );
    }
    mBuildTools->mMachine->Reset();
    mBuildTools->mMachine->Exec( program, mContext );
    mBuildTools->mMachine->ThrowPossibleErrorException();
    if( mBuildTools->mMachine->HasResult() ) {
        return mBuildTools->mMachine->MoveResult();
    }
    return {}; // NaV (Not a Value)
}

TEASCRIPT_COMPILE_MODE_INLINE
StackVM::ProgramPtr Engine::CompileContent( Content const &rContent, eOptimize const opt_level, std::string const &rName )
{
    auto const ast = mBuildTools->mParser.Parse( rContent, rName );
    return mBuildTools->mCompiler.Compile( ast, opt_level );
}

TEASCRIPT_COMPILE_MODE_INLINE
StackVM::ProgramPtr Engine::CompileScript( std::filesystem::path const &path, eOptimize const opt_level )
{
    // build utf-8 filename again... *grrr*
    auto const filename = util::utf8_path_to_str( path );
    auto const val = CoreLibrary::ReadTextFileEx( path );
    if( val.GetTypeInfo()->IsSame<String>() ) {
        Content  content{val.GetValue<String>()};
        return CompileContent( content, opt_level, filename ); // TODO: absolute path or filename?
    }

    throw exception::load_file_error( filename );
}

} // namespace teascript

