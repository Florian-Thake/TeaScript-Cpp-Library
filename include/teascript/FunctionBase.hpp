/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <functional>

namespace teascript {

class ValueObject;
class Context;
class SourceLocation;

class FunctionBase;

using FunctionPtr = std::shared_ptr<FunctionBase>; // must be shared_ptr for can use in std::any

/// Callback function type usable at user level. \warning EXPERIMENTAL
using CallbackFunc = std::function< ValueObject( Context & ) >;

/// Common base class for all functions in TeaScript
class FunctionBase
{
public:
    FunctionBase() = default;
    virtual ~FunctionBase() {}

    virtual ValueObject Call( Context &, std::vector<ValueObject> &, SourceLocation const & ) = 0;

    virtual int ParamCount() const
    {
        return -1; // arbitrary amount
    }

    virtual std::string ParameterInfoStr() const
    {
        int const p = ParamCount();
        if( p == -1 ) {
            return "(...)";
        }
        return "(" + std::to_string( p ) + ")";
    }
};

} // namespace teascript
