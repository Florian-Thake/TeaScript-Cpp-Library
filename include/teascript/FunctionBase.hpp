/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
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
