/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
 */
#pragma once

#include "Sequence.hpp"
#include "Type.hpp"

namespace teascript {

using IntegerSequence = Sequence<Integer>;

//FIXME: After split Type.hpp this instance should live in TypeSystem and always be registered!
//static TypeInfo const TypeIntegerSequence = MakeTypeInfo<IntegerSequence>( "IntegerSequence" );


namespace seq {

/// returns the given Sequence as printable string
inline std::string print( IntegerSequence const &seq )
{
    return "seq(" + std::to_string( seq.Start() ) + "," + std::to_string( seq.End() ) + "," + std::to_string( seq.Step() ) + ")";
}

} // namespace seq

} // namespace teascript
