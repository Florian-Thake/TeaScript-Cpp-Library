/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


/// Declaration and definition of the ASTNode base class and ASTNodePtr.
/// This is decoupled with class ValueObject, class Context, etc., so that they can use the base class withot knowing
/// the derived classes.


#include <memory> // shared_ptr
#include <vector>
#include <string>
#include <stdexcept>
#include <functional>

#include "SourceLocation.hpp"

namespace teascript {

class ValueObject;
class Context;

class ASTNode;

using ASTNodePtr = std::shared_ptr<ASTNode>;

using ASTNodeContainer = std::vector<ASTNodePtr>;


/// The common base class for all ASTNodes.
class ASTNode
{
    std::string const  mName;
    std::string const  mDetail;

protected:
    SourceLocation /*const*/  mLocation;

public:
    explicit ASTNode( std::string  name, SourceLocation loc = {} )
        : mName( std::move( name ) )
        , mDetail()
        , mLocation( std::move( loc ) )
    {
    }

    ASTNode( std::string  name, std::string const &rDetail, SourceLocation loc = {} )
        : mName( std::move( name ) )
        , mDetail( rDetail )
        , mLocation( std::move( loc ) )
    {
    }

    virtual ~ASTNode()
    {
    }

    /// checks if the ASTNode is ready for Eval() can be called, e.g., all necessarry children are present (if any)
    /// \throws if it is not ready.
    virtual void Check() const
    {
        if( IsIncomplete() ) {
            throw exception::eval_error( GetSourceLocation(), "ASTNode " + GetName() + " is incomplete!" );
        }
    }

    /// recursive evaluation of the AST within the given context \praram rContext. \return ValueObject as result.
    virtual ValueObject Eval( Context &rContext ) const = 0;


    /// \return the name of this ASTNode.
    std::string const &GetName() const noexcept
    {
        return mName;
    }

    /// \return the deatail information of this ASTNode.
    std::string const &GetDetail() const noexcept
    {
        return mDetail;
    }

    /// \return an info string of this ASTNode (useful for print information).
    virtual std::string GetInfoStr() const
    {
        auto const &detail = GetDetail();
        if( detail.empty() ) {
            return GetName();
        }
        return GetName() + ": " + detail;
    }

    /// \return the source code location of this ASTNode.
    inline SourceLocation const &GetSourceLocation() const noexcept
    {
        return mLocation;
    }

    /// \return whether this ASTNode is a dummy ASTNode.
    virtual bool IsDummy() const noexcept
    {
        return false;
    }

    /// \return whether this ASTNode feels satisfied with its children count/kind.
    virtual bool IsComplete() const noexcept
    {
        return true;
    }

    /// \return whether this ASTNode feels unsatisfied with its children count/kind.
    bool IsIncomplete() const noexcept
    {
        return !IsComplete();
    }

    /// \return whether this ASTNode needs a left-hand-side operand.
    virtual bool NeedLHS() const noexcept
    {
        return false;
    }

    /// \return the (operator) precedence of this ASTNode.
    virtual int Precedence() const noexcept
    {
        return 0;
    }

    /// adds given child node. \throw exception::runtime_error on error and node is complete already.
    virtual void AddChildNode( ASTNodePtr )
    {
        if( IsComplete() ) {
            throw exception::runtime_error( GetSourceLocation(), "ASTNode complete! Cannot add additional child!" );
        }
    }

    /// \return whether this ASTNode has children (ChildCount() > 0).
    virtual bool HasChildren() const noexcept
    {
        return false;
    }

    /// \returns the amount of children.
    virtual size_t ChildCount() const noexcept
    {
        return 0;
    }

    /// \return and remove the last. \throws exception::runtime_error on error and ASTNodes without children.
    virtual ASTNodePtr PopChild()
    {
        throw exception::runtime_error( GetSourceLocation(), "ASTNode::PopChild(): This ASTNode cannot have children!" );
    }

    virtual ASTNodeContainer::const_iterator begin() const noexcept
    {
        return {};
    }
    virtual ASTNodeContainer::const_iterator end() const noexcept
    {
        return {};
    }

    /// applies a callback function recursively to the AST. Stops nesting if callback returns false.
    virtual void Apply( std::function<bool( ASTNode const *, int )> const &callback, int depth = 1 ) const
    {
        callback( this, depth );
    }
};

} // namespace teascript

