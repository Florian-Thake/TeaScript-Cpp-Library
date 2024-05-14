/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Engine.hpp"
#include "Context.hpp"

namespace teascript {

/// This class is meant to be used for prepare a Context instance and
/// load arbitrary variables and functions into the (global) scope of
/// a Context instance by executing custom TeaScript code.
/// After the context is prepared and ready, it can be moved to e.g.,
/// a CoroutineScriptEngine and be used as the environment for the 
/// coroutines.
/// \note The ContextFactory always compiles the code. Evaluation mode
///       is not available.
class ContextFactory : public Engine
{
public:
    /// The dafault constuctor bootstraps the full Core Library.
    ContextFactory()
        : Engine( config::full(), Engine::eMode::Compile )
    {
    }

    /// This constructor will use the given config for bootstrapping the Core Library.
    explicit ContextFactory( config::eConfig const conf )
        : Engine( conf, Engine::eMode::Compile )
    {
    }

    /// \returns the context, which was moved from member. The member context is now empty but usable.
    /// \note For re-use this factory instance most likely a ResetState() call should be issued.
    Context MoveOutContext()
    {
        Context c = std::move( mContext );

        // safety!
        mContext = Context();

        return c; // RVO
    }
};

}
