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
            throw exception::runtime_error( "UserCallbackFunc(): callback function is invalid!" );
        }
    }

    ValueObject Call( Context &rContext, std::vector<ValueObject> &rParams, SourceLocation const &rLoc ) override
    {
        ScopedNewScope new_scope( rContext, rParams, rLoc );

        return mCallback( rContext );
    }
};

} // namespace teascript

