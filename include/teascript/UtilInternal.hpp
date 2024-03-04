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


namespace teascript {

namespace util {

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

