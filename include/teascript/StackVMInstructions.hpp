/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ValueObject.hpp"


namespace teascript {

/// All instructions (opcodes) for the TeaStackVM.
enum class eTSVM_Instr : unsigned int
{
    NotImplemented = ~0u,
    HALT       = 0, // (emrgency) halting the machine. 0 for zero initilaized will produce HALTs per default
    ProgramEnd,     // same as NoOp but indicating normal program end, for debugging
    NoOp,           // 'no operation', can be used as filler/placeholder
    NoOp_NaV,       // this is the NoOp ASTNode which pushes a NaV, equivalent to Push (NaV)
    Debug,          // was the Debug operator, only functional in eval mode, NoOp when compile, for debugging only.
    ExprStart,      //=NoOp, was start of Expr ASTNode, for debugging
    ExprEnd,        //=NoOp, was end of Expr ASTNode, for debugging
    If,             //=NoOp, was start of If ASTNode, for debugging
    Else,           //=NoOp, was start of Else ASTNode, for debugging
    RepeatStart,    //=NoOp, was start of Repeat ASTNode, for debugging
    RepeatEnd,      //=NoOp, was end of Repeat ASTNode, for debugging
    Push,           // push a constant value on top of the stack
    Pop,            // pops one value from the stack
    Replace,        // sets the last value of the stack with a new one (equivalent to Pop+Push)
    Swap,           // swaps top stack value with top-1
    Load,           // load variable and push
    Stor,           // store (set) variable (shared/unshared is in payload)
    DefVar,         // define mutable variable (shared/unshared is in payload)
    ConstVar,       // define const variable (shared/unshared is in payload)
    AutoVar,        // define variable which takes const/mutable from origin (shared/unshared is in payload)
    UndefVar,       // undefine variable
    IsDef,          // is_defined variable
    MakeTuple,      // creates a tuple from N elements from the working stack.
    SetElement,     // stores the last value of the stack in the element in top-1 of object top-2 (shared/unshared is in payload)
    DefElement,     // stores the last value of the stack in a new mutable element in top-1 of object top-2 (shared/unshared is in payload)
    ConstElement,   // stores the last value of the stack in a new const element in top-1 of object top-2 (shared/unshared is in payload)
    IsDefElement,   // is_defined obj.element
    UndefElement,   // removes element (top) from tuple (top-1)
    SubscriptGet,   // subscript operator read
    SubscriptSet,   // subscript operator write
    UnaryOp,
    BinaryOp,
    IsType,
    AsType,
    BitOp,
    DotOp,
    EnterScope,     // block open / new local scope
    ExitScope,      // block close / deletes most recent local scope.
    Test,           // converts current top stack to Bool
    JumpRel,        // jump relative unconditional.
    JumpRel_If,     // relative jump if pop returns bool(true) val
    JumpRel_IfNot,  // relative jump if pop returns bool(false) val
    TestAndJumpRel_If,     // test + relative jump if pop returns bool(true) val
    TestAndJumpRel_IfNot,  // test + relative jump if pop returns bool(false) val
    ForallHead,     // start of forall loop, prepares everything for the loop body
    ForallNext,     // next iteration of forall loop
    CallFunc,       // calls function object and save pc+1 as Ret address on call stack.
    ParamList,      //=NoOp, was ParamList ASTNode, for debugging
    FuncDef,        // defines a function and store it as variable in context.
    Ret,            // returns from current function and jumps to call stack top-1 program with retaddress in call stack top.
    ParamSpec,      // starts parameter specificaton of a defined function.
    ParamSpecClean, // cleanup parameter specification.
    FromParam,      // sets current parameter to current value in stack.
    FromParam_Or,   // sets current parameter to current value in stack, if any, or executes the instructions of the "Or" branch.
    ExitProgram,    // exits the program, removes all local scopes, clears stack. (must not be issued for reaching 'ProgramEnd')
    Suspend,        // suspends the prorgam (except Constraints was set to AutoContinue).
    Yield,          // suspends the program with a value.
    Catch,          // if top stack is Error or NaV 
};

namespace StackVM {

/// One instruction for the TeaStackVM (opcode + (possible) payload).
struct Instruction
{
    eTSVM_Instr  instr;
    ValueObject  payload;

    // not sure why clang need this help for construct this simple struct...
#if defined( __clang__ )
    inline Instruction( eTSVM_Instr  const ins, ValueObject &&v ) : instr( ins ), payload( std::move( v ) ) {}
    inline Instruction( eTSVM_Instr  const ins, ValueObject const &v ) : instr( ins ), payload( v ) {}
#endif

    static std::string_view ToString( eTSVM_Instr const i )
    {
        switch( i ) {
        case eTSVM_Instr::HALT:                  return "HALT";
        case eTSVM_Instr::ProgramEnd:            return "ProgramEnd";
        case eTSVM_Instr::NoOp:                  return "NoOp";
        case eTSVM_Instr::NoOp_NaV:              return "NoOp_NaV";
        case eTSVM_Instr::Debug:                 return "Debug";
        case eTSVM_Instr::ExprStart:             return "ExprStart";
        case eTSVM_Instr::ExprEnd:               return "ExprEnd";
        case eTSVM_Instr::RepeatStart:           return "RepeatStart";
        case eTSVM_Instr::RepeatEnd:             return "RepeatEnd";
        case eTSVM_Instr::If:                    return "If";
        case eTSVM_Instr::Else:                  return "Else";
        case eTSVM_Instr::Push:                  return "Push";
        case eTSVM_Instr::Pop:                   return "Pop";
        case eTSVM_Instr::Replace:               return "Replace";
        case eTSVM_Instr::Swap:                  return "Swap";
        case eTSVM_Instr::Load:                  return "Load";
        case eTSVM_Instr::Stor:                  return "Stor";
        case eTSVM_Instr::DefVar:                return "DefVar";
        case eTSVM_Instr::ConstVar:              return "ConstVar";
        case eTSVM_Instr::AutoVar:               return "AutoVar";
        case eTSVM_Instr::UndefVar:              return "UndefVar";
        case eTSVM_Instr::IsDef:                 return "IsDef";
        case eTSVM_Instr::MakeTuple:             return "MakeTuple";
        case eTSVM_Instr::SetElement:            return "SetElement";
        case eTSVM_Instr::DefElement:            return "DefElement";
        case eTSVM_Instr::ConstElement:          return "ConstElement";
        case eTSVM_Instr::IsDefElement:          return "IsDefElement";
        case eTSVM_Instr::UndefElement:          return "UndefElement";
        case eTSVM_Instr::SubscriptGet:          return "SubscriptGet";
        case eTSVM_Instr::SubscriptSet:          return "SubscriptSet";
        case eTSVM_Instr::UnaryOp:               return "UnaryOp";
        case eTSVM_Instr::BinaryOp:              return "BinaryOp";
        case eTSVM_Instr::IsType:                return "IsType";
        case eTSVM_Instr::AsType:                return "AsType";
        case eTSVM_Instr::BitOp:                 return "BitOp";
        case eTSVM_Instr::DotOp:                 return "DotOp";
        case eTSVM_Instr::EnterScope:            return "EnterScope";
        case eTSVM_Instr::ExitScope:             return "ExitScope";
        case eTSVM_Instr::Test:                  return "Test";
        case eTSVM_Instr::JumpRel:               return "JumpRel";
        case eTSVM_Instr::JumpRel_If:            return "JumpRel_If";
        case eTSVM_Instr::JumpRel_IfNot:         return "JumpRel_IfNot";
        case eTSVM_Instr::TestAndJumpRel_If:     return "TestAndJumpRel_If";
        case eTSVM_Instr::TestAndJumpRel_IfNot:  return "TestAndJumpRel_IfNot";
        case eTSVM_Instr::ForallHead:            return "ForallHead";
        case eTSVM_Instr::ForallNext:            return "ForallNext";
        case eTSVM_Instr::CallFunc:              return "CallFunc";
        case eTSVM_Instr::ParamList:             return "ParamList";
        case eTSVM_Instr::FuncDef:               return "FuncDef";
        case eTSVM_Instr::Ret:                   return "Ret";
        case eTSVM_Instr::ParamSpec:             return "ParamSpec";
        case eTSVM_Instr::ParamSpecClean:        return "ParamSpecClean";
        case eTSVM_Instr::FromParam:             return "FromParam";
        case eTSVM_Instr::FromParam_Or:          return "FromParam_Or";
        case eTSVM_Instr::ExitProgram:           return "ExitProgram";
        case eTSVM_Instr::Suspend:               return "Suspend";
        case eTSVM_Instr::Yield:                 return "Yield";
        case eTSVM_Instr::Catch:                 return "Catch";
        case eTSVM_Instr::NotImplemented:        return "NotImplemented";
        default:
            return "<unknown>";
        }
    }

    static eTSVM_Instr FromString( std::string_view const str )
    {
        if( str == "HALT" ) {
            return eTSVM_Instr::HALT;
        } else if( str == "ProgramEnd" ) {
            return eTSVM_Instr::ProgramEnd;
        } else if( str == "NoOp" ) {
            return eTSVM_Instr::NoOp;
        } else if( str == "NoOp_NaV" ) {
            return eTSVM_Instr::NoOp_NaV;
        } else if( str == "Debug" ) {
            return eTSVM_Instr::Debug;
        } else if( str == "ExprStart" ) {
            return eTSVM_Instr::ExprStart;
        } else if( str == "ExprEnd" ) {
            return eTSVM_Instr::ExprEnd;
        } else if( str == "RepeatStart" ) {
            return eTSVM_Instr::RepeatStart;
        } else if( str == "RepeatEnd" ) {
            return eTSVM_Instr::RepeatEnd;
        } else if( str == "If" ) {
            return eTSVM_Instr::If;
        } else if( str == "Else" ) {
            return eTSVM_Instr::Else;
        } else if( str == "Push" ) {
            return eTSVM_Instr::Push;
        } else if( str == "Pop" ) {
            return eTSVM_Instr::Pop;
        } else if( str == "Replace" ) {
            return eTSVM_Instr::Replace;
        } else if( str == "Swap" ) {
            return eTSVM_Instr::Swap;
        } else if( str == "Load" ) {
            return eTSVM_Instr::Load;
        } else if( str == "Stor" ) {
            return eTSVM_Instr::Stor;
        } else if( str == "DefVar" ) {
            return eTSVM_Instr::DefVar;
        } else if( str == "ConstVar" ) {
            return eTSVM_Instr::ConstVar;
        } else if( str == "AutoVar" ) {
            return eTSVM_Instr::AutoVar;
        } else if( str == "UndefVar" ) {
            return eTSVM_Instr::UndefVar;
        } else if( str == "IsDef" ) {
            return eTSVM_Instr::IsDef;
        } else if( str == "MakeTuple" ) {
            return eTSVM_Instr::MakeTuple;
        } else if( str == "SetElement" ) {
            return eTSVM_Instr::SetElement;
        } else if( str == "DefElement" ) {
            return eTSVM_Instr::DefElement;
        } else if( str == "ConstElement" ) {
            return eTSVM_Instr::ConstElement;
        } else if( str == "IsDefElement" ) {
            return eTSVM_Instr::IsDefElement;
        } else if( str == "UndefElement" ) {
            return eTSVM_Instr::UndefElement;
        } else if( str == "SubscriptGet" ) {
            return eTSVM_Instr::SubscriptGet;
        } else if( str == "SubscriptSet" ) {
            return eTSVM_Instr::SubscriptSet;
        } else if( str == "UnaryOp" ) {
            return eTSVM_Instr::UnaryOp;
        } else if( str == "BinaryOp" ) {
            return eTSVM_Instr::BinaryOp;
        } else if( str == "IsType" ) {
            return eTSVM_Instr::IsType;
        } else if( str == "AsType" ) {
            return eTSVM_Instr::AsType;
        } else if( str == "BitOp" ) {
            return eTSVM_Instr::BitOp;
        } else if( str == "DotOp" ) {
            return eTSVM_Instr::DotOp;
        } else if( str == "EnterScope" ) {
            return eTSVM_Instr::EnterScope;
        } else if( str == "ExitScope" ) {
            return eTSVM_Instr::ExitScope;
        } else if( str == "Test" ) {
            return eTSVM_Instr::Test;
        } else if( str == "JumpRel" ) {
            return eTSVM_Instr::JumpRel;
        } else if( str == "JumpRel_If" ) {
            return eTSVM_Instr::JumpRel_If;
        } else if( str == "JumpRel_IfNot" ) {
            return eTSVM_Instr::JumpRel_IfNot;
        } else if( str == "TestAndJumpRel_If" ) {
            return eTSVM_Instr::TestAndJumpRel_If;
        } else if( str == "TestAndJumpRel_IfNot" ) {
            return eTSVM_Instr::TestAndJumpRel_IfNot;
        } else if( str == "ForallHead" ) {
            return eTSVM_Instr::ForallHead;
        } else if( str == "ForallNext" ) {
            return eTSVM_Instr::ForallNext;
        } else if( str == "CallFunc" ) {
            return eTSVM_Instr::CallFunc;
        } else if( str == "ParamList" ) {
            return eTSVM_Instr::ParamList;
        } else if( str == "FuncDef" ) {
            return eTSVM_Instr::FuncDef;
        } else if( str == "Ret" ) {
            return eTSVM_Instr::Ret;
        } else if( str == "ParamSpec" ) {
            return eTSVM_Instr::ParamSpec;
        } else if( str == "ParamSpeClean" ) {
            return eTSVM_Instr::ParamSpecClean;
        } else if( str == "FromParam" ) {
            return eTSVM_Instr::FromParam;
        } else if( str == "FromParam_Or" ) {
            return eTSVM_Instr::FromParam_Or;
        } else if( str == "ExitProgram" ) {
            return eTSVM_Instr::ExitProgram;
        } else if( str == "Suspend" ) {
            return eTSVM_Instr::Suspend;
        } else if( str == "Yield" ) {
            return eTSVM_Instr::Yield;
        } else if( str == "Catch" ) {
            return eTSVM_Instr::Catch;
        } else if( str == "NotImplemented" ) {
            return eTSVM_Instr::NotImplemented;
        } else {
            throw exception::runtime_error( "Instruction::FromString(): Unknown instruction!" );
        }
    }
};

} // namespace StackVM

} // namespace teascript

