/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


#include "Content.hpp"
#include "Exception.hpp"
//#define TEASCRIPT_DISABLE_FMTLIB 1
#include "Print.hpp"
#include "SourceLocation.hpp"

#if TEASCRIPT_FMTFORMAT
# include "fmt/color.h"
#endif


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

[[maybe_unused]]
void debug_print_currentline( teascript::Content const &r, bool const with_marked_pos = false )
{
    Content const c( r ); // dont interact with the object used outside to reduce chance of any unwanted side effects!
    auto    const line = extract_current_line( r );

    TEASCRIPT_PRINT( "(line:{:4}/col:{:3}): {}\n", c.CurrentLine(), c.CurrentColumn(), line );
    if( with_marked_pos ) {
        TEASCRIPT_PRINT( "{0:>{1}}\n", "^^^^^", r.CurrentColumn() + 21LL + 4LL );
    }
}

// colored variant is only possible when libfmt is used.
#if TEASCRIPT_FMTFORMAT
[[maybe_unused]]
void debug_print_currentline_colored( teascript::Content const &r, bool const with_marked_pos = false )
{
    Content const c( r ); // dont interact with the object used outside to reduce chance of any unwanted side effects!
    auto    const line = extract_current_line( r );

    fmt::print( fmt::fg( fmt::color::wheat ), "(line:{:4}/col:{:3}): {}\n", c.CurrentLine(), c.CurrentColumn(), fmt::styled( line, fmt::fg( fmt::color::white_smoke ) ) );
    if( with_marked_pos ) {
        fmt::print( fmt::fg( fmt::color::violet ), "{0:>{1}}\n", "^^^^^", r.CurrentColumn() + 21LL + 4LL );
    }
}
#endif


[[noreturn]] void throw_parsing_error( Content const &c, std::shared_ptr<std::string> const &rFile, std::string const &rText )
{
    throw exception::parsing_error( c.CurrentLine(), c.CurrentColumn(), std::string( extract_current_line( c ) ), rFile, rText );
}


SourceLocation make_srcloc( std::shared_ptr<std::string> const &rFile, Content const &c, bool const extract_line = false )
{
    auto loc = SourceLocation( c.CurrentLine(), c.CurrentColumn() );
    loc.SetFile( rFile );
    if( extract_line ) {
        loc.SetSource( std::string( extract_current_line( c ) ) );
    }
    return loc;
}

SourceLocation make_srcloc( std::shared_ptr<std::string> const &rFile, Content const &start, Content const &end, bool const extract_line = false )
{
    auto loc = make_srcloc( rFile, start, extract_line );
    loc.SetEnd( end.CurrentLine(), end.CurrentColumn() );
    return loc;
}

} // namespace {

} // namespace util

} // namespace teascript

