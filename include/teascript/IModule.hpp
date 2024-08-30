/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "ConfigEnums.hpp"
#include <string_view>


namespace teascript {

class Context;

/// Interface for all TeaScript C++ Modules.
/// \warning EXPERIMANTAL
class IModule
{
public:
    virtual ~IModule() = default;

    /// \return the name of the module.
    virtual std::string_view GetName() const = 0;

    /// Shall load the Module into the given context. config options and eval_only are considered a hint only.
    /// E.g., if eval_only is true but the module comes without source or it wants alwys be loaded in compiled form, then the module can do so.
    virtual void Load( Context &rInto, config::eConfig const config, bool const eval_only ) = 0;

};

} // namespace teascript
