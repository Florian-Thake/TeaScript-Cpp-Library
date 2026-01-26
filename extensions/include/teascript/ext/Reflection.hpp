/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

// === USAGE INSTRUCTION ===
// In order to get reflection working, you need the reflectcpp library / source code (tested with version 0.23.0) https://github.com/getml/reflect-cpp
// There are 2 options: Link against the reflectcpp library or choose a TU where the reflectcpp.cpp is embedded.
// The latter option is very simple and doesn't require any package manager or build project tooling.
// 1) Add extensions/include of TeaScript to your include paths (in order to can include this Reflection.hpp)
// 2) Add the 'include' directory of reflectpp to your include paths.
// 3) 2 Options: (see also https://rfl.getml.com/install/ )
// 3.a) If you want to embed reflectcpp source
//      1) Add the 'src' directory of reflectcpp to your include paths.
//      2) include 'rfl.hpp' followed by 'reflectcpp.cpp' in one TU _OR_ add extensions/source/Reflection.cpp to your project.
// 3.b) Or link against a compiled reflectcpp library.
// 4) Include this header and use the API below.

// Detect reflectcpp include dir is present...
#if __has_include("rfl.hpp")
# if __has_include("reflectcpp.cpp")
#  define TEASCRIPT_EXTENSION_REFLECTCPP      2   // src present -> means it could be included in a TU for avoid linking against a separate static lib.
# else
#  define TEASCRIPT_EXTENSION_REFLECTCPP      1   // only headers present.
# endif
#else
# define TEASCRIPT_EXTENSION_REFLECTCPP       0
#endif


#if TEASCRIPT_EXTENSION_REFLECTCPP

#include "teascript/EngineBase.hpp"
#include "teascript/Context.hpp"

#include "Reflection_impl.hpp"

namespace teascript {
namespace reflect {

/// Imports 'object' as 'name' to the current scope of the given engine by constructing a Tuple from it with all nested (sub-)members.
/// \note 'object' is usually a struct with native types/std::string members and may contain nested structs, vectors, arrays, smart pointers, ...
/// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error or std::runtime_error.
/// \note This feature is currently EXPERIMENTAL
void into_teascript( EngineBase &engine, std::string const &name, auto const &object )
{
    ValueObject val;
    reflection::write_tuple( object, val );
    engine.AddSharedValueObject( name, val );
}

/// Exports the Tuple variable 'name' from the given engine into an object of type T.
/// T must be capable of receive all Tuple elements and nested tuples and its elements and so on.
/// \throw May throw exception::unknown_identifier or a different exception based on exception::eval_eror/runtime_error or std::runtime_error.
/// \note This feature is currently EXPERIMENTAL
template<class T>
auto from_teascript( EngineBase const &engine, std::string const &name )
{
    auto const val = engine.GetVar( name );
    return reflection::read_tuple<T>( val ).value();
}

/// Imports 'object' as 'name' to the current scope of the given context by constructing a Tuple from it with all nested (sub-)members.
/// \note 'object' is usually a struct with native types/std::string members and may contain nested structs, vectors, arrays, smart pointers, ...
/// \throw May throw exception::redefinition_of_variable or a different exception based on exception::eval_eror/runtime_error or std::runtime_error.
/// \note This feature is currently EXPERIMENTAL
void into_teascript( Context &context, std::string const &name, auto const &object )
{
    ValueObject val;
    reflection::write_tuple( object, val );
    context.AddValueObject( name, val );
}

/// Exports the Tuple variable 'name' from the given context into an object of type T.
/// T must be capable of receive all Tuple elements and nested tuples and its elements and so on.
/// \throw May throw exception::unknown_identifier or a different exception based on exception::eval_eror/runtime_error or std::runtime_error.
/// \note This feature is currently EXPERIMENTAL
template<class T>
auto from_teascript( Context const &context, std::string const &name )
{
    auto const val = context.FindValueObject( name );
    return reflection::read_tuple<T>( val ).value();
}

} // namespace reflect
} // namespace teascript

#endif // TEASCRIPT_EXTENSION_REFLECTCPP

