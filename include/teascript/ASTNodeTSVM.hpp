/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


#include "ASTNode.hpp"
#include "StackVMInstructions.hpp"

namespace teascript {

class ASTNode_TSVM : public ASTNode_Child_Capable
{
public:
    explicit ASTNode_TSVM( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "TSVM", std::move( loc ) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mChildren.size() >= 2;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "TSVM ASTNode complete! Cannot add additional child!" );
        }

        if( mChildren.empty() && node->GetName() != "Id" ) {
            throw exception::runtime_error( GetSourceLocation(), "First child of TSVM ASTNode must an identifier (the TSVM instruction)!" );
        } else if( mChildren.size() == 1 && node->GetName() != "Constant" ) {
            throw exception::runtime_error( GetSourceLocation(), "Second child of TSVM ASTNode must an constant value (the TSVM payload)!" );
        }

        mChildren.emplace_back( std::move( node ) );
    }

    StackVM::Instruction GetInstruction() const
    {
        Check();
        return {StackVM::Instruction::FromString( mChildren[0]->GetDetail() ), std::static_pointer_cast<ASTNode_Constant>(mChildren[1])->GetValue()};
    }

    void Check() const override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "TSVM ASTNode incomplete! Some parts are missing!" );
        }
    }

    ValueObject Eval( Context &/*rContext*/ ) const override
    {
        Check();
        throw exception::eval_error( GetSourceLocation(), "TSVM ASTNode cannot be evaluated. It must be compiled." );
    }
};

} // namespace teascript
