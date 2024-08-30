/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

// This header provides a switch via the TEASCRIPT_PRINT macro for the internal used way to output to stdout.


// Define this if the format library (libfmt) is present in your include pathes but you don't want to use it.
//#define TEASCRIPT_DISABLE_FMTLIB         1


// Define this if you have C++23 but you don't want to use std::vprint_unicode
//#define TEASCRIPT_DISABLE_STDFORMAT_23   1


// Define this if you have and want use libfmt, but not as header only (you are responsible for it compiles and links in your project!).
//#define TEASCRIPT_DISABLE_FMT_HEADER_ONLY  1


// Priority order:
// 1.) Use fmt::print if available (this will offer the most possible features, e.g. colored output, string formatting within script code, etc...)
// 2.) Use std::vprint_unicode if available.
// 3.) Use std::cout + std::format
#if __has_include( "fmt/format.h" ) && !defined( TEASCRIPT_DISABLE_FMTLIB )
# define TEASCRIPT_FMTFORMAT      1
# ifndef TEASCRIPT_DISABLE_FMT_HEADER_ONLY
#  ifndef FMT_HEADER_ONLY
#   define FMT_HEADER_ONLY
#  endif
# endif
# include "fmt/format.h"
# if FMT_VERSION < 100101
#  error format lib must be at least 10.1.1
# endif
#elif __has_include( <format> )
# include <format>
# if defined( __cpp_lib_format ) && __cpp_lib_format > 202110L && __cplusplus > 202002L && !defined( TEASCRIPT_DISABLE_STDFORMAT_23 )
#  define TEASCRIPT_STDFORMAT_23   1
#  include <print>
# else
#  include <iostream>
#  define TEASCRIPT_STDFORMAT_20   1
# endif
#else
# error no format lib found!
#endif

// define this always to a number for can use it as a bool (TEASCRIPT_FMTFORMAT==1)
#ifndef TEASCRIPT_FMTFORMAT
# define TEASCRIPT_FMTFORMAT      0
#endif

#if TEASCRIPT_FMTFORMAT
# define TEASCRIPT_PRINT( ... ) fmt::print( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) fmt::print( stderr, __VA_ARGS__ )
#elif TEASCRIPT_STDFORMAT_23 
# define TEASCRIPT_PRINT( ... ) std::vprint_unicode( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) std::vprint_unicode( stderr, __VA_ARGS__ )
#elif TEASCRIPT_STDFORMAT_20
// NOTE: On Windows you must most likely hook up the streambuf of std::cout/std::cerr for support full UTF-8
# define TEASCRIPT_PRINT( ... ) std::cout << std::format( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) std::cerr << std::format( __VA_ARGS__ )
#endif
