/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "CompiledFuncBase.hpp"
#include "ValueObject.hpp"
#include "Context.hpp"
#include "StackVMProgram.hpp"
#include "StackMachine.hpp"


namespace teascript {

class CompiledFunc : public CompiledFuncBase
{
    StackVM::ProgramPtr mProgram;
    size_t  mStartAddress;
public:
    explicit CompiledFunc( StackVM::ProgramPtr const &program, size_t const start )
        : CompiledFuncBase()
        , mProgram( program )
        , mStartAddress( start )
    {}

    virtual ~CompiledFunc() {}

    StackVM::ProgramPtr GetProgram() const override
    {
        return mProgram;
    }

    size_t GetStartAddress() const override
    {
        return mStartAddress;
    }

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        // NOTE: Here we are entering compiled land and coming from the eval world.
        //       Thus we need a machine to execute the compiled code.
        //       From the compiled land we would not enter here. This call is only issued from an Eval() call.
        //       Because of this we just create a new machine locally. In later versions it would be great if
        //       a) eiter a machine pool could be used or better,
        //       b) a machine (or a subroutine executor with a machine) is carried via the Context or sth. similar.

        auto machine = std::make_shared<StackVM::Machine<false>>();

        machine->ExecSubroutine( mProgram, mStartAddress, rContext, std::span( rParams.data(), rParams.size() ), rLoc );
        machine->ThrowPossibleErrorException();

        if( machine->HasResult() ) {
            return machine->MoveResult();
        }

        return {};
    }

    int ParamCount() const override
    {
        if( mStartAddress >= mProgram->GetInstructions().size() ) { // sth. strange.
            return FunctionBase::ParamCount();
        }
        auto const &instr = mProgram->GetInstructions();
        auto const s = instr.size();
        auto idx = mStartAddress;
        while( idx < s ) {
            if( instr[idx].instr == eTSVM_Instr::ParamSpec ) {
                return static_cast<int>(instr[idx].payload.GetValue<U64>());
            }
            ++idx;
        }

        // sth. strange!
        return FunctionBase::ParamCount();
    }

    std::string ParameterInfoStr() const override
    {
        if( mStartAddress >= mProgram->GetInstructions().size() ) { // sth. strange.
            return FunctionBase::ParameterInfoStr();
        }
        auto const &instr = mProgram->GetInstructions();
        auto const s = instr.size();
        auto idx = mStartAddress;

        std::string res = "(";
        bool first = true;
        while( idx < s ) {
            if( instr[idx].instr == eTSVM_Instr::ParamSpecClean ) {
                break; // finished
            }
            if( instr[idx].instr == eTSVM_Instr::FromParam || instr[idx].instr == eTSVM_Instr::FromParam_Or ) {
                assert( idx > 0 );
                if( first ) {
                    first = false;
                } else {
                    res += ", ";
                }
                // one instruction prior FromParam[_Or] is either a Push "id" or Replace "id"
                res += instr[idx - 1].payload.GetValue<std::string>();
            }
            
            ++idx;
        }
        res += ")";
        return res;
    }
};

namespace {
FunctionPtr CompiledFuncFactory(StackVM::ProgramPtr const &program, size_t const start)
{
    return std::make_shared<CompiledFunc>( program, start );
}
}

} // namespace teascript
