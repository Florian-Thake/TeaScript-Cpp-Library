/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
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
