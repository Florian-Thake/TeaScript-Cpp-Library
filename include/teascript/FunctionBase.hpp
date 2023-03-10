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

#include <memory>
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
};

} // namespace teascript
