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


#include "Content.hpp"
#include "Exception.hpp"
//#define TEASCRIPT_DISABLE_FMTLIB 1
#include "Print.hpp"
#include "SourceLocation.hpp"


namespace teascript {

namespace util {

// anonymous namespace for make the non-inline functions usable in more than one TU.
namespace {

inline void debug_print( teascript::Content const &r )
{
    Content const c( r ); // dont interact with the object used outside to reduce chance of any unwanted side effects!
    TEASCRIPT_PRINT( "(line:{}/col:{})   total: {}, processed: {}, remaining: {},  *cur=\'{}\'(int {})\n", c.CurrentLine(), c.CurrentColumn(), c.TotalSize(), c.Processed(), c.Remaining(), *c, static_cast<int>(*c) );
}

/// Returns Content where current position is moved back to first column of current line.
inline Content carriage_return( teascript::Content const &r ) noexcept
{
#if 1 // NEW, no loop for col < threshold and keeps column count up-to-date for col >= threshold. for col > 1 should be always the same time to execute.
    Content res( r );
    res.MoveInLine_Unchecked( -static_cast<int>(res.CurrentColumn() - 1) );
    return res;
#else // OLD, not so bad but it might invalidate column count if col is >= threshold, but that is not so important. has a loop if col < threshold.
    return r - (r.CurrentColumn() - 1);
#endif
}

/// extracts the current line of Content \param r without the line ending. Thus empty lines will be an empty string (but data() points to line starting address).
std::string_view extract_current_line( teascript::Content const &r ) noexcept
{
    Content const  start = carriage_return( r );
    Content const  end   = [c=start]() mutable noexcept {
        // advance until begin of line feed (or end of script). (c == '\0' is covered by c.MoveToLineFeed())
        if( c != '\n' && (c != '\r' || *(c + 1) != '\n') ) { // not empty line?
            c.MoveToLineFeed();
            // move back to '\r' if line feed is '\r\n'
            if( c == '\n' && *(c - 1) == '\r' ) --c;
        }
        return c;
    }();

    return {&(*start), end.Processed() - start.Processed()};
}

void debug_print_currentline( teascript::Content const &r )
{
    Content const c( r ); // dont interact with the object used outside to reduce chance of any unwanted side effects!
    auto    const line = extract_current_line( r );

    TEASCRIPT_PRINT( "(line:{}/col:{})   current line: {}\n", c.CurrentLine(), c.CurrentColumn(), line );
}


[[noreturn]] void throw_parsing_error( Content const &c, std::shared_ptr<std::string> const &rFile, std::string const &rText )
{
    throw exception::parsing_error( c.CurrentLine(), c.CurrentColumn(), std::string( extract_current_line( c ) ), rFile, rText );
}


SourceLocation make_srcloc( std::shared_ptr<std::string> const & rFile, Content const &c, bool const extract_line = false )
{
    auto loc = SourceLocation( c.CurrentLine(), c.CurrentColumn() );
    loc.SetFile( rFile );
    if( extract_line ) {
        loc.SetSource( std::string( extract_current_line( c )) );
    }
    return loc;
}

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

size_t utf8_string_length( std::string const &rStr )
{
    constexpr unsigned char utf8_Continuation_Prefix = 0x80;  // 10xx xxxx
    constexpr unsigned char utf8_Continuation_Mask   = 0xC0;  // 1100 0000

    // count utf-8 code points by skipping all follow chars.
    size_t glyphs = 0;
    
    for( size_t idx = 0 ; idx < rStr.size(); ++idx ) {
        if( (static_cast<unsigned char>(rStr[idx]) & utf8_Continuation_Mask) != utf8_Continuation_Prefix ) {
            ++glyphs;
        }
    }
    return glyphs;
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

