/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "Sequence.hpp"
#include "Types.hpp"

namespace teascript {

/// Sequence of Integers (i64), the standard/default Sequence type in TeaScript. Used in e.g., the forall loop.
using IntegerSequence = Sequence<Integer>;


namespace seq {

/// returns the given Sequence as printable string
inline std::string print( IntegerSequence const &seq )
{
    return "seq(" + std::to_string( seq.Start() ) + "," + std::to_string( seq.End() ) + "," + std::to_string( seq.Step() ) + ")";
}

} // namespace seq

} // namespace teascript
