/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

namespace teascript {

enum class eOptimize
{
    Debug,   // full debug information with a lot of extra 'no op' like Instructions (ExprStart/End, If, Else,...) for easier debugging
    O0,      // default, minimal debug infos, no extra instructions, no optimizing
    O1,      // simple optimizing with pre compute constant logical and arithmethic operations (TODO: Implement!)
    O2,      // more aggressive optimizing (NOT IMPLEMENTED! will fallback to O1)
};


namespace config {

/// Config enum for specify what shall be loaded.
enum eConfig : unsigned int
{
    LevelMask = 0x00'00'00'0f,
    FeatureOptOutMask = 0xff'ff'ff'00,

    // level numbers are not or'able, just have some spare room for future extensions.
    LevelMinimal = 0x00'00'00'00, /// loading only Types and version variables NOTE: The language and usage is very limited since even basic things like creation of empty tuple or length of string are not available.
    LevelCoreReduced = 0x00'00'00'01, /// this is a reduced variant of the Core level where not all string / tuple utilities are loaded. Not all language features / built-in types are fully usable in this mode.
    LevelCore = 0x00'00'00'02, /// loading full tuple / string utility and some other type utilities. Language and its built-in types are fully usable.
    LevelUtil = 0x00'00'00'04, /// loading more library utilities like clock, random, sleep, some math functions, etc.
    LevelFull = 0x00'00'00'08, /// loading all normal and standard stuff.

    // optional feature disable (counts from Level >= LevelCoreReduced, below its always disabled)

    NoStdIn = 0x00'00'01'00,
    NoStdErr = 0x00'00'02'00,
    NoStdOut = 0x00'00'04'00,
    NoFileRead = 0x00'00'08'00,
    NoFileWrite = 0x00'00'10'00,
    NoFileDelete = 0x00'00'20'00,
    NoEval = 0x00'00'40'00,
    NoEvalFile = NoFileRead | NoEval,
    //NoNetworkClient,
    //NoNetworkServer,
};

/// helper function for build config, usage example: build( config::LevelFull, config::NoFileWrite | config::NoFileDelete )
constexpr eConfig build( eConfig const level, unsigned int const opt_out = 0 ) noexcept
{
    return static_cast<eConfig>((static_cast<unsigned int>(level) & LevelMask) | (opt_out & FeatureOptOutMask));
}

// some convenience helper functions to build custom configs

constexpr eConfig minimal() noexcept { return LevelMinimal; }
constexpr eConfig core_reduced() noexcept { return LevelCoreReduced; }
constexpr eConfig core() noexcept { return LevelCore; }
constexpr eConfig util() noexcept { return LevelUtil; }
constexpr eConfig full() noexcept { return LevelFull; }

constexpr eConfig optout_everything( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | FeatureOptOutMask); }

constexpr eConfig no_stdio( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoStdIn | NoStdOut | NoStdErr); }
constexpr eConfig no_fileio( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoFileRead | NoFileWrite | NoFileDelete); }
constexpr eConfig no_eval( eConfig const in = static_cast<eConfig>(0) )  noexcept { return static_cast<eConfig>(in | NoEval); }

} // namespace config

} // namespace teascript

