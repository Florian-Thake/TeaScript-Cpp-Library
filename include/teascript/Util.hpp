/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


#include "Exception.hpp"
//#define TEASCRIPT_DISABLE_FMTLIB 1
#include "Print.hpp"
#include "SourceLocation.hpp"

#if TEASCRIPT_FMTFORMAT
# include "fmt/color.h"
#endif

#include <span>


namespace teascript {

namespace util {

// anonymous namespace for make the non-inline functions usable in more than one TU.
namespace {


[[maybe_unused]]
void pretty_print( teascript::exception::runtime_error const &ex, std::string const src_overwrite = {} )
{
    if( not ex.IsSourceLocSet() ) {
        TEASCRIPT_PRINT( "{} error in file \"{}\": {}\n", ex.GetGategory(), ex.GetFileStr(), ex.ErrorStr_or_What() );
    } else {
        TEASCRIPT_PRINT( "{} error in file \"{}\"\nin line {}, column {}:\n", ex.GetGategory(), ex.GetFileStr(), ex.GetLine(), ex.GetColumn() );
        if( not ex.GetContextStr().empty() || not src_overwrite.empty() ) {
            if( src_overwrite.empty() ) {
                TEASCRIPT_PRINT( "{}\n", ex.GetContextStr() );
            } else {
                TEASCRIPT_PRINT( "{}\n", src_overwrite );
            }
            TEASCRIPT_PRINT( "{0:>{1}}\n", "^^^^^", ex.GetColumn() + 4LL );
        }
        TEASCRIPT_PRINT( "{}\n", ex.ErrorStr_or_What() );
    }
}


// colored variant is only possible when libfmt is used.
#if TEASCRIPT_FMTFORMAT
[[maybe_unused]]
void pretty_print_colored( teascript::exception::runtime_error const &ex, std::string const src_overwrite = {} )
{
    if( not ex.IsSourceLocSet() ) {
        fmt::print( "{} error in file \"{}\": {}\n", ex.GetGategory(), fmt::styled( ex.GetFileStr(), fmt::fg( fmt::color::white_smoke ) ), fmt::styled( ex.ErrorStr_or_What(), fmt::fg( fmt::color::tomato ) ) );
    } else {
        fmt::print( "{} error in file \"{}\"\nin line {}, column {}:\n", ex.GetGategory(), fmt::styled( ex.GetFileStr(), fmt::fg( fmt::color::white_smoke ) ),
                    fmt::styled( ex.GetLine(), fmt::fg( fmt::color::wheat ) ), fmt::styled( ex.GetColumn(), fmt::fg( fmt::color::wheat ) ) );
        if( not ex.GetContextStr().empty() || not src_overwrite.empty() ) {
            std::string const &src = src_overwrite.empty() ? ex.GetContextStr() : src_overwrite;
            // try to mark the "marked" position
            if( ex.GetSourceLocation().GetStartLine() == ex.GetSourceLocation().GetEndLine() && ex.GetSourceLocation().GetEndColumn() > ex.GetSourceLocation().GetStartColumn() ) {
                fmt::print( fmt::fg( fmt::color::white_smoke ), "{}", src.substr( 0, ex.GetSourceLocation().GetStartColumn() - 1 ) );
                fmt::print( fmt::fg( fmt::color::violet ), "{}", src.substr( ex.GetSourceLocation().GetStartColumn() - 1, ex.GetSourceLocation().GetEndColumn() - ex.GetSourceLocation().GetStartColumn() + 1 ) );
                fmt::print( fmt::fg( fmt::color::white_smoke ), "{}\n", static_cast<size_t>(ex.GetSourceLocation().GetEndColumn()) >= src.size() ? "" : src.substr( ex.GetSourceLocation().GetEndColumn() ) );
            } else {
                fmt::print( fmt::fg( fmt::color::white_smoke ), "{}\n", src );
            }
            fmt::print( fmt::fg( fmt::color::violet ), "{0:>{1}}\n", "^^^^^", ex.GetColumn() + 4LL );
        }
        fmt::print( fmt::fg( fmt::color::tomato ), "{}\n", ex.ErrorStr_or_What() );
    }
}
#endif


/// \returns whether [start,start+count) is forming a complete UTF-8 range in \param rStr.
/// \note: This functions assumes /param rStr is a valid UTF-8 encoded string!
[[maybe_unused]]
bool is_complete_utf8_range( std::string const &rStr, std::size_t const start, std::size_t const count )
{
    // if we peek in the middle of an utf-8 code point sequence it is not a complete utf-8 range.
    if( start >= rStr.length() || (static_cast<unsigned char>(rStr[start]) & 0xC0) == 0x80 ) {
        return false;
    }
    if( count != std::string::npos && count > 0 && static_cast<std::size_t>(start + count - 1) < rStr.length() ) {
        // follow char?
        if( (static_cast<unsigned char>(rStr[static_cast<std::size_t>(start + count - 1)]) & 0xC0) == 0x80 ) {
            // next char is still follow char? then it is not the valid end. (note: end of the string will yield 0)
            if( (static_cast<unsigned char>(rStr[static_cast<std::size_t>(start + count)]) & 0xC0) == 0x80 ) {
                return false;
            }
        // does it point to the start of a multibyte sequence?
        } else if( static_cast<unsigned char>(rStr[static_cast<std::size_t>(start + count - 1)]) > 0xC1 ) {
            return false;
        }
    }
    return true;
}

[[maybe_unused]]
size_t utf8_string_length( std::string const &rStr )
{
    constexpr unsigned char utf8_Continuation_Prefix = 0x80;  // 10xx xxxx
    constexpr unsigned char utf8_Continuation_Mask   = 0xC0;  // 1100 0000

    // count utf-8 code points by skipping all follow chars.
    size_t glyphs = 0;

    for( size_t idx = 0; idx < rStr.size(); ++idx ) {
        if( (static_cast<unsigned char>(rStr[idx]) & utf8_Continuation_Mask) != utf8_Continuation_Prefix ) {
            ++glyphs;
        }
    }
    return glyphs;
}

[[maybe_unused]]
size_t utf8_glyph_to_byte_pos( std::string const &rStr, size_t const glyph )
{
    if( rStr.empty() ) [[unlikely]] {
        return std::string::npos;
    }

    constexpr unsigned char utf8_Continuation_Prefix = 0x80;  // 10xx xxxx
    constexpr unsigned char utf8_Continuation_Mask   = 0xC0;  // 1100 0000

    // count utf-8 code points by skipping all follow chars.
    size_t glyph_count = 0;
    size_t idx         = 0;
    for( ; idx < rStr.size(); ++idx ) {
        if( (static_cast<unsigned char>(rStr[idx]) & utf8_Continuation_Mask) != utf8_Continuation_Prefix ) {
            if( glyph == glyph_count ) {
                return idx;
            }
            ++glyph_count;
        }
    }

    // string too short
    return std::string::npos;;
}

template< typename CharT = char >
requires ( std::is_same_v<std::remove_const_t<CharT>, char> 
        || std::is_same_v< std::remove_const_t<CharT>, unsigned char> 
        || std::is_same_v< std::remove_const_t<CharT>, char8_t> )
bool is_valid_utf8( std::span<CharT> const &rData, bool const reject_control_chars = false )
{
    for( size_t idx = 0; idx < rData.size(); ++idx ) {
        unsigned char const x = static_cast<unsigned char>(rData[idx]);
        if( x > 127 ) {

            // start with a follow char is not allowed, 0xC0|C1 and > 0xF4 is invalid.
            //if( x < 0xC0 || x == 0xC0 || x == 0xC1 || x > 0xF4 ) {
            if( x < 0xC2 || x > 0xF4 ) {
                return false;
            }
            int follow = 0;
            unsigned int unicode = 0;
            if( (x & 0xF8) == 0xF0 ) {
                follow = 3;
                unicode = x & ~0xF8;
            } else if( (x & 0xF0) == 0xE0 ) {
                follow = 2;
                unicode = x & ~0xF0;
            } else if( (x & 0xE0) == 0xC0 ) {
                follow = 1;
                unicode = x & ~0xE0;
            }
            if( idx + follow >= rData.size() ) {
                return false;
            }
            int const orig_follow = follow;
            while( follow > 0 ) {
                ++idx;
                unsigned char const x1 = static_cast<unsigned char>(rData[idx]);
                if( x1 < 0x80 || x1 > 0xBF ) {
                    return false;
                }
                unicode = (unicode << 6) | (x1 & ~0xC0);
                --follow;
            }

            // finally check if our unicode is valid
            if( unicode >= 0xD800 && unicode <= 0xDFFF ) { // UTF16 Surrogate pairs are not allowed
                return false;
            } else if( unicode > 0x10FFFF ) { // too big
                return false;
            } else if( (unicode < 0x800 && orig_follow > 1) || (unicode < 0x10000 && orig_follow > 2) ) { // overlong encoding!
                return false;
            }

        } else if( reject_control_chars ) {
            //                                             '\t' until '\r' are allowed
            if( x == 127 /*DEL*/ || (x < 0x20 /*Space*/ && (x < 0x8 || x > 0xD)) ) {
                return false;
            }
        }
    } // for

    return true;
}

} // namespace {

} // namespace util

} // namespace teascript

