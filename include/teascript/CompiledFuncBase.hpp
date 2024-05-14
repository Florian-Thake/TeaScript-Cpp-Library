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

namespace teascript {
namespace StackVM {
class Program;
using ProgramPtr = std::shared_ptr<Program>;
}

class CompiledFuncBase : public FunctionBase
{
public:
    virtual ~CompiledFuncBase() {}

    virtual size_t  GetStartAddress() const = 0;
    virtual StackVM::ProgramPtr GetProgram() const = 0;
};

} // namespace teascript

