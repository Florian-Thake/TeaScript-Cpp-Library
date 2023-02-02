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
#include "Context.hpp"


namespace teascript {

/// class UserCallbackFunc is for all functions from the user level made available in TeaScript code.
/// \warning EXPERIMENTAL
class UserCallbackFunc : public FunctionBase
{
    CallbackFunc  mCallback;
public:
    UserCallbackFunc( CallbackFunc const &rCallback )
        : mCallback( rCallback )
    {
        if( !mCallback ) {
            throw exception::runtime_error( "UserCallbackFunc(): callback functuion is invalid!" );
        }
    }

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        ScopedNewScope new_scope( rContext, rParams, rLoc );

        return mCallback( rContext );
    }
};

} // namespace teascript

