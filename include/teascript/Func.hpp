/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
 */
#pragma once

#include "FunctionBase.hpp"
#include "ValueObject.hpp"
#include "ASTNode.hpp"
#include "Context.hpp"


namespace teascript {

/// class Func is for all ordinary functions defined within TeaScript code.
class Func : public FunctionBase
{
    std::shared_ptr< ASTNode_ParamSpec > mParamSpec;
    std::shared_ptr< ASTNode_Block >     mBlock;
public:
    Func( ASTNodePtr const &paramspec, ASTNodePtr const &block, SourceLocation const &rLoc = {} )
        : FunctionBase()
        , mParamSpec( std::dynamic_pointer_cast<ASTNode_ParamSpec>(paramspec) )
        , mBlock( std::dynamic_pointer_cast<ASTNode_Block>(block) )
    {
        if( !mParamSpec || !mBlock ) {
            throw exception::runtime_error( rLoc, "Teascript Function has no fitting ASTNode instances!" );
        }
    }

    virtual ~Func() {}

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        ScopedNewScope new_scope( rContext, rParams, rLoc );

        /*auto vals = */(void)mParamSpec->Eval(rContext);

        if( rContext.CurrentParamCount() > 0 ) { // TODO: relax this check ?
            throw exception::eval_error( rLoc, "Calling Func: Too many arguments!" );
        }

        try {
            return mBlock->Eval( rContext );
        } catch( control::Return_From_Function  &rReturn ) {
            return rReturn.MoveResult();
        }
    }
};

} // namespace teascript

