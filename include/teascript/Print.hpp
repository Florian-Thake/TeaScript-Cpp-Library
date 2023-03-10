/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
 */
#pragma once

// This header provides a switch via the TEASCRIPT_PRINT macro for the internal used way to output to stdout.

// Priority order:
// 1.) Use std::print if available.
// 2.) Use fmt::print
// 3.) Use std::cout + std::format
#if defined( __cpp_lib_format ) && __cpp_lib_format > 202110L && !defined( TEASCRIPT_DISABLE_STDFORMAT_23 )
# define TEASCRIPT_STDFORMAT_23   1
# include <format>
# include <print>
#elif __has_include( "fmt/format.h" ) && !defined( TEASCRIPT_DISABLE_FMTLIB )
# define TEASCRIPT_FMTFORMAT      1
# ifndef FMT_HEADER_ONLY
#  define FMT_HEADER_ONLY
# endif
# include "fmt/format.h"
#elif defined( __cpp_lib_format )
# include <format>
# include <iostream>
# define TEASCRIPT_STDFORMAT_20   1
#else
#  error no format lib found!
#endif

#if TEASCRIPT_STDFORMAT_23 
# define TEASCRIPT_PRINT( ... ) std::vprint_unicode( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) std::vprint_unicode( stderr, __VA_ARGS__ )
#elif TEASCRIPT_FMTFORMAT
# define TEASCRIPT_PRINT( ... ) fmt::print( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) fmt::print( stderr, __VA_ARGS__ )
#elif TEASCRIPT_STDFORMAT_20
// NOTE: On Windows you must most likely hook up the streambuf of std::cout/std::cerr for support full UTF-8
# define TEASCRIPT_PRINT( ... ) std::cout << std::format( __VA_ARGS__ )
# define TEASCRIPT_ERROR( ... ) std::cerr << std::format( __VA_ARGS__ )
#endif
