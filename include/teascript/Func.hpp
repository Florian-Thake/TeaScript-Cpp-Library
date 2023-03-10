/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
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

