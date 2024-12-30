/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


// With the Dialect class various inofficial, experimental and/or legacy dialects of the TeaScript language can be created.
// The unmodified default values of this file defining the official TeaScript standard language.
// Either change the defines (or set from outside) or manipulate the dialect class instances where required 
// for define a new TeaScript dialect.
// WARNING: If you change the default settings you will create an unofficial dialect where (existing) scripts may not work or don't work as expected.
//          They might even do unexpected damage!


// IMPORTANT: The option to have a TeaScript dialect where unknown identifiers will get automatically defined is actually not official supported and untested.
#ifndef TEASCRIPT_DEFAULT_AUTO_DEFINE_UNKNOWN_IDENTIFIERS
# define TEASCRIPT_DEFAULT_AUTO_DEFINE_UNKNOWN_IDENTIFIERS    false
#endif

// if true: undef an unknown identifier is allowed and will not produce an eval error (default: true)
#ifndef TEASCRIPT_UNDEFINE_UNKNOWN_IDENTIFIERS_ALLOWED
# define TEASCRIPT_UNDEFINE_UNKNOWN_IDENTIFIERS_ALLOWED       true
#endif

// IMPORTANT: The option to have a TeaScript dialect where declaration without assignment is allowed is actually unsupported and not functional.
#ifndef TEASCRIPT_DECLARE_IDENTIFIERS_WITHOUT_ASSIGN_ALLOWED
# define TEASCRIPT_DECLARE_IDENTIFIERS_WITHOUT_ASSIGN_ALLOWED false
#endif

// if true parameters of functions are const by default. (default: true (since 0.12.0, before was false))
// NOTE: before 0.16.0 this rule applied only to copy assign (:=).
#ifndef TEASCRIPT_DEFAULT_CONST_PARAMETERS
# define TEASCRIPT_DEFAULT_CONST_PARAMETERS                   true
#endif

// if true parameters of functions are shared by default (@=). (default: true (since 0.16.0, before it was implicit set to false))
#ifndef TEASCRIPT_DEFAULT_SHARED_PARAMETERS
# define TEASCRIPT_DEFAULT_SHARED_PARAMETERS                  true
#endif

// if true explicit(!) shared parameters without explicit def/const will be automatically set to same as origin.
// E.g. with a hypothetical auto keyword an 'id @=' will become 'auto id @=' and id will be either 'const' or 'def' depending on the rhs variable.
#ifndef TEASCRIPT_DEFAULT_SHARED_AUTO_PARAMETERS
# define TEASCRIPT_DEFAULT_SHARED_AUTO_PARAMETERS             true
#endif

namespace teascript {

/// class for define the TeaScript language behavior. 
/// The unmodified default values form the TeaScript standard language.
/// Everything else will create an inofficial, experimental and/or legacy dialect.
class Dialect
{
public:
    bool auto_define_unknown_identifiers            = TEASCRIPT_DEFAULT_AUTO_DEFINE_UNKNOWN_IDENTIFIERS;
    bool undefine_unknown_idenitifiers_allowed      = TEASCRIPT_UNDEFINE_UNKNOWN_IDENTIFIERS_ALLOWED;
    bool declare_identifiers_without_assign_allowed = TEASCRIPT_DECLARE_IDENTIFIERS_WITHOUT_ASSIGN_ALLOWED;
    bool parameters_are_default_const               = TEASCRIPT_DEFAULT_CONST_PARAMETERS;
    bool parameters_are_default_shared              = TEASCRIPT_DEFAULT_SHARED_PARAMETERS;
    bool shared_parameters_are_default_auto         = TEASCRIPT_DEFAULT_SHARED_AUTO_PARAMETERS;
};

} // namespace teascript
