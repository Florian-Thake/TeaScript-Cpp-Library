/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <string>
#include <stdexcept>
#include <filesystem>

namespace teascript {

namespace util {

inline
std::filesystem::path utf8_path( std::string const &rUtf8 )
{
#if _LIBCPP_VERSION
    _LIBCPP_SUPPRESS_DEPRECATED_PUSH
#elif (defined(__GNUC__) && !defined(__clang__))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // if you get an error here use /w34996 or use _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
#endif
    return std::filesystem::u8path( rUtf8 );
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if _LIBCPP_VERSION
    _LIBCPP_SUPPRESS_DEPRECATED_POP
#elif (defined(__GNUC__) && !defined(__clang__))
#pragma GCC diagnostic pop
#endif
}

inline
std::string utf8_path_to_str( std::filesystem::path const &rPath )
{
#if defined(_WIN32)
    auto const tmp_u8 = rPath.generic_u8string();
    return std::string( tmp_u8.begin(), tmp_u8.end() ); // must make a copy... :-(
#else
    // NOTE: On Windows this will be converted to native code page! Could be an issue when used as TeaScript string!
    return rPath.generic_string();
#endif
}

// anonymous namespace for make the non-inline functions usable in more than one TU.
namespace {


void escape_in_string( std::string &rString, char const what, std::string const &with )
{
    if( with.find( what ) != std::string::npos ) {
        throw std::invalid_argument( "escape_in_string(): cannot escape 'what' with sth. containing 'what'!" );
    }
    auto found = rString.find( what );
    while( found != std::string::npos ) {

        rString.replace( found, 1, with );

        found = rString.find( what );
    }
}

bool shorten_utf8_string( std::string &rStr, size_t const len )
{
    constexpr unsigned char utf8_Continuation_Prefix = 0x80;  // 10xx xxxx
    constexpr unsigned char utf8_Continuation_Mask   = 0xC0;  // 1100 0000

    if( rStr.size() > len ) {
        // count utf-8 code points by skipping all follow chars.
        size_t glyphs = 0; 
        size_t idx = 0;
        for( ; idx < rStr.size() && glyphs <= len; ++idx ) {
            if( (static_cast<unsigned char>(rStr[idx]) & utf8_Continuation_Mask) != utf8_Continuation_Prefix ) {
                ++glyphs;
            }
        }
        if( glyphs > len ) {
            rStr.erase( idx - 1 );
            return true;
        }
    }

    return false;
}

void prepare_string_for_print( std::string &rStr, size_t const len )
{
    // first cut of everything what is too long
    bool cut = shorten_utf8_string( rStr, len );

    //TODO: better loop over all chars and replace if it is not printable!
    escape_in_string( rStr, '\r', "\\r" );
    escape_in_string( rStr, '\n', "\\n" );
    escape_in_string( rStr, '\t', "\\t" );

    size_t const  erase_len = len + 3; // for ...
    cut = shorten_utf8_string( rStr, erase_len ) || cut;

    rStr = "\"" + rStr + (cut ? "\"..." : "\"" );
}

} // namespace {

} // namespace util

} // namespace teascript

