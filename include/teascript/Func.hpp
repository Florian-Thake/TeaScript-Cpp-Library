/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
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

    int ParamCount() const override
    {
        return static_cast<int>(mParamSpec->ChildCount());
    }

    std::string ParameterInfoStr() const override
    {
        std::string res = "(";
        bool first = true;
        for( auto const &node : *mParamSpec ) {
            if( node->ChildCount() == 0 ) { // sth. strange!
                return FunctionBase::ParameterInfoStr();
            }
            if( first ) {
                first = false;
            } else {
                res += ", ";
            }
            res += (*node->begin())->GetDetail();
        }
        res += ")";
        return res;
    }

};

} // namespace teascript

