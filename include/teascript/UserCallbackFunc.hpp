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
#include "Context.hpp"


namespace teascript {

/// class UserCallbackFunc is for all functions from the user level made available in TeaScript code.
/// \warning EXPERIMENTAL, the interface, members and mechnaics may change!
class UserCallbackFunc : public FunctionBase
{
    CallbackFunc  mCallback;
    int           mParamCount;   // desired parameter count, -1 == arbitrary (e.g., func( ... ) )
public:
    UserCallbackFunc( CallbackFunc const &rCallback, int param_count = -1 )
        : mCallback( rCallback )
        , mParamCount( param_count )
    {
        if( !mCallback ) {
            throw exception::runtime_error( "UserCallbackFunc(): callback function is invalid!" );
        }
    }

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        ScopedNewScope new_scope( rContext, rParams, rLoc );

        return mCallback( rContext );
    }

    int ParamCount() const override
    {
        return mParamCount;
    }

};

} // namespace teascript

