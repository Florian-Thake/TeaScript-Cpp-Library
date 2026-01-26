/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#include "teascript/ext/Reflection.hpp"

// NOTE: You can either add this .cpp to your project, then the reflectcpp is compiled here,
//       or you choose your own cpp to do this, or you must link against a reflectcpp library.

// check if all include dependencies are there...
#if !__has_include("reflectcpp.cpp")
# error You must add the src directory of reflectcpp to your include paths.
#endif

#if !__has_include("rfl.hpp")
# error You must add the include directory of reflectcpp to your include paths.
#endif

// Pull in reflectcpp, as described here https://rfl.getml.com/install/#option-4-include-source-files-into-your-own-build

// we use Warning Level 4, but reflectcpp has warings there, so disable them...
#if defined(_MSC_VER )
# pragma warning( push )
# pragma warning( disable: 4324 )
#endif

//via Refection.hpp\\#include "rfl.hpp"
#include "reflectcpp.cpp"

#if defined(_MSC_VER )
# pragma warning( pop )
#endif

#if 0  // test code c++ struct -> toml++
// little helper as we are not interested in strings but the toml object.
void write_toml( const auto &obj, ::toml::table *root )
{
    using T = std::remove_cvref_t<decltype(obj)>;
    using ParentType = rfl::parsing::Parent<rfl::toml::Writer>;
    auto w = rfl::toml::Writer( root );
    using ProcessorsType = rfl::Processors<>;
    rfl::toml::Parser<T, ProcessorsType>::write( w, obj, typename ParentType::Root{} );
}
#endif

namespace teascript {
namespace reflection {


// nothing to do here, since refelctcpp is nearly a template header only lib.


} // namespace reflection
} // namespace teascript
