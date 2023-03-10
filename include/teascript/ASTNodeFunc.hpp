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

/// File for the class ASTNode_Func and ASTNode_CallFunc definition. Must be decoupled from ASTNode.hpp for can easily include Func.hpp.

#include "ASTNode.hpp"
#include "Func.hpp"

namespace teascript {


/// represents a TeaScript Function definition (not call)
class ASTNode_Func : public ASTNode_Child_Capable
{
    bool mIsComplete = false;
public:
    explicit ASTNode_Func( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "Func", std::move(loc) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mIsComplete; // not the most accurate but enough for now... (last child should be a block and minimum size 2)
    }

    void SetComplete() noexcept
    {
        mIsComplete = true;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "Func ASTNode complete! Cannot add additional child!" );
        }

        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "Func ASTNode incomplete! Some parts are missing!" );
        }

        if( mChildren.size() < 2 || (mChildren[0]->GetName() == "Id" && mChildren.size() < 3) ) {
            throw exception::eval_error( GetSourceLocation(), "Internal error! Parameter or Block for func def is missing!" );
        }

        auto  paramdef = mChildren[mChildren.size() - 2];
        auto  block = mChildren[mChildren.size() - 1];

        auto func = std::make_shared<Func>( paramdef, block, GetSourceLocation() );

        ValueObject  val{std::move(func), ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() )};

        if( mChildren[0]->GetName() == "Id" ) {
            rContext.AddValueObject( mChildren[0]->GetDetail(), val, mChildren[0]->GetSourceLocation() );
            return ValueObject(true); // make it usable in boolean expressions: use_xxx and (func test(a) {a*a})
        }

        return val;
    }
};


/// represents a TeaScript Function Call
class ASTNode_CallFunc : public ASTNode_Child_Capable
{
    bool mIsComplete = false;
public:
    explicit ASTNode_CallFunc( SourceLocation loc = {} )
        : ASTNode_Child_Capable( "CallFunc", std::move(loc) )
    {
    }

    bool IsComplete() const noexcept override
    {
        return mIsComplete; // not the most accurate but enough for now... 
    }

    void SetComplete() noexcept
    {
        mIsComplete = true;
    }

    void AddChildNode( ASTNodePtr node ) override
    {
        assert( node.get() != nullptr );
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "CallFunc ASTNode complete! Cannot add additional child!" );
        }

        mChildren.emplace_back( std::move( node ) );
    }

    ValueObject Eval( Context &rContext ) override
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "CallFunc ASTNode incomplete! Some parts are missing!" );
        }

        if( mChildren.size() < 2 ) {
            throw exception::eval_error( GetSourceLocation(), "Internal error! Id or Parameter List for func call is missing!" );
        }

        // get the ValueObject with the Func
        auto funcval = mChildren[0]->Eval( rContext );
        auto func = funcval.GetValue< FunctionPtr >(); // copy is intended

        // get and evaluate parameter list
        auto  paramval = mChildren[1]->Eval( rContext );
        auto  & params = paramval.GetValue< std::vector< ValueObject> >();

        return func->Call( rContext, params, GetSourceLocation() );
    }

};

} // namespace teascript

