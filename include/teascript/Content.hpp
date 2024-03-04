/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <cassert>
#include <cstring> //strlen
#include <string>
#include <stdexcept>
#include <type_traits> // std::is_pointer<>


namespace teascript {


/// Class for positional moving inside a char content range for parsing/processing, line and column calculating and forming sub ranges.
/// IMPORTANT: The char content range of an instance of this class must stay valid for the lifetime of the object.
/// The class is not threadsafe but reentrant. Only valid objects can be constructed with the provided Constructors.
/// \note With the current approach the current char belongs neither to processed nor remaining.
///       So you have [---processed---|cur|---remaining---] for a content layout, where processed and/or remaining can be empty but never cur.
///       Also, the current char is always valid and in the range [start, end]. 
///       That leads to processed (and remaining as well) can never reach total amount of chars but total amount - 1.
class Content
{
    char const *start = nullptr;
    char const *cur   = nullptr;
    char const *end   = nullptr;

    // -1 idicates to calculate, mutable for keeps the getters const.
    long long  mutable  line   = 1;
    long long  mutable  column = 1;

    long long           line_offset = 0;

    static constexpr char nul[] = "";

    /// jumps smaller this value will be in a for loop with pre[--/++] instead of [+/-]= and invalidating line/column
    static constexpr size_t distance_threshold = 16 + 1;


    inline bool validate() const noexcept
    {
        return start != nullptr && end != nullptr && cur != nullptr && !(end < start) && cur >= start && cur <= end;
    }

    void calculate_column() const noexcept
    {
        column = 1;

        for( auto p = cur; p != start && p[-1] != '\n'; --p ) {
            ++column;
        }
    }

    void calculate_line() const noexcept
    {
        line = 1 + line_offset;

        for( auto p = start; p != cur; ++p ) {
            if( *p == '\n' ) {
                ++line;
            }
        }
    }

    inline void lazy_update_line( int  distance ) const noexcept
    {
        if( -1 != line ) {
            line += distance; // distance can be negative
            assert( line > 0 );
        }
    }

    inline void lazy_update_column( int  distance ) const noexcept
    {
        if( -1 != column ) {
            column += distance; // distance can be negative
            assert( column > 0 );
        }
    }

    inline void next_line() const noexcept
    {
        lazy_update_line( +1 );
        column = 1;
    }

    inline void previous_line() const noexcept
    {
        lazy_update_line( -1 );
        column = -1;
    }

    inline void next_column() const noexcept
    {
        lazy_update_column( +1 );
    }

    inline void previous_column() const noexcept
    {
        lazy_update_column( -1 );
    }

public:
    Content( Content const & ) = default;
    ~Content() = default;
    //Content( Content && ) = delete;
    Content &operator=( Content const & ) = default;
    //Content &operator=( Content && ) = delete;

    /// Constructs a valid content consists only of the '\0' character.
    /// \see Content( char const *const pContent, size_t const len )
    inline
    Content()
        : Content( nul )
    {
    }

    /// Constructs a content range from [pContent, len-1] and validates it.
    /// \throws std::invalid_argument if invariants are not hold (range is not valid).
    /// \post if the object is constructed, the current position points to start; line and column are set to 1.
    /// \note This Constructor is intended to be called (directly or indirectly) by any other constructor of this class (if not otherwise specified).
    Content( char const *const pContent, size_t const len )
        : start( pContent )
        , cur( start )
        , end( start + len - 1 )
    {
        if( !validate() ) {
            throw std::invalid_argument( "Content is not valid!" );
        }
    }

    /// Constructor mainly for static string literals (and fixed size char arrays)
    /// Usage: Content( "Text to parse as string literal" ); (or have a constexpr char[]="xxx" somewhere)
    /// \see Content( char const *const pContent, size_t const len )
    template< size_t N>
    Content( char const (&content)[N] )
        : Content( &content[0], N )
    {
    }

    /// Constructor for bare metal char *. For someone still uses a char pointer and cannot pass a string literal to match the templated constructor....
    /// \see Content( char const *const pContent, size_t const len )
    /// \note IMPLEMENTATION DETAIL 
    /// char const * is ambigous with char const (&)[] because of array to pointer decay. Thus we use the template and C++20 concepts here to help the compiler
    /// (Before C++20 you had to use std::enable_if)
    template< typename CharPtr = char const * >
    Content( CharPtr pStr ) requires std::is_pointer_v<CharPtr>
        : Content( pStr, ::strlen(pStr) + 1 ) // '\0' is included!
    {
    }

    /// Constructor for use the content of the std::string \param rStr as content range.
    /// \see Content( char const *const pContent, size_t const len )
    Content( std::string const &rStr )
        : Content( rStr.data(), rStr.length() + 1 ) // '\0' is included!
    {
    }

    /// Constructor for use the content of the std::string_view \param rSv as content range.
    /// \see Content( char const *const pContent, size_t const len )
    /// \note You must ensure the view ends with a '\0' or '\n' (included in the length())!
    Content( std::string_view const &rSv )
        : Content( rSv.data(), rSv.length() )
    {
    }


    /// Rewinds to the start position.
    void Rewind() noexcept
    {
        cur = start;
        line = 1 + line_offset;
        column = 1;
    }

    /// Creates a SubContent which is a reduced content of the original with a new start and/or end
    /// Because of that the line and column count, etc. are also distinct from the original.
    /// This is useful for e.g. some preprocessing or if a SubContent shall be parsed speparately again.
    Content SubContent( size_t offset = 0, size_t count = static_cast<size_t>(-1) )
    {
        Content res( *this );
        if( res.Remaining() < offset ) {
            throw std::out_of_range( "offset results in start behind end!" );
        }
        res.start = res.cur + offset;
        res.cur = res.start;

        if( count != static_cast<size_t>(-1) ) {
            if( count > res.Remaining() ) {
                throw std::out_of_range( "growing behind original end!" );
            }
            res.end = res.start + count;
        }
        if( 0 == offset ) {
            res.line   = 1; // always starts at 1
            res.column = 1;
        } else {
            res.line   = -1;
            res.column = -1;
        }

        // safety!
        if( !res.validate() ) {
            throw std::invalid_argument( "Content is not valid!" );
        }

        return res;
    }

    /// Sets the line_offset to \param off and (lazy) updates current line if needed.
    void SetLineOffset( long long off )
    {
        if( off >= 0 && line_offset != off ) {
            long long const diff = off - line_offset;
            line_offset = off;
            if( line > 0 ) {
                line += diff;
                assert( line > 0 );
            }
        }
    }

    /// Returns the current set line offset (usually 0).
    long long GetLineOffset() const noexcept
    {
        return line_offset;
    }


    /// Returns the current line of the current position. 
    /// \note It may calculate it first if the current line is not known.
    long long CurrentLine() const noexcept
    {
        if( -1 == line ) calculate_line();
        return line;
    }

    /// Returns the current column of the current position. 
    /// \note It may calculate it first if the current column is not known.
    long long CurrentColumn() const noexcept
    {
        if( -1 == column ) calculate_column();
        return column;
    }

    /// Checks whether there is at least one more char available.
    /// This call is equivalent to Remaining() > 0
    /// \note Even if this method returns false, the current char is always pointing to a valid part of the input content (e.g. the last char of the input).
    inline bool HasMore() const noexcept
    {
        return cur != end;
    }

    /// Returns the remaining chars available _behind_ the current one.
    inline size_t Remaining() const noexcept
    {
        return static_cast<size_t>(end - cur);
    }

    /// Returns the total size in chars, e.g. amount of chars of the content (including any whitespace and linefeeds). Is at least 1.
    inline size_t TotalSize() const noexcept
    {
        return static_cast<size_t>(end - start) + 1u;
    }

    /// Returns the amount of processed chars _before_ the current one. NOTE: Because of that Processed() can never reach TotalSize() (but TotalSize() - 1).
    inline size_t Processed() const noexcept
    {
        return static_cast<size_t>(cur - start);
    }

    /// Returns the character at current position. The current position is always valid.
    inline char const &get() const noexcept
    {
        return *cur;
    }

    /// Returns the character at current position (ex.: char x = *content;). The current position is always valid.
    inline char const &operator*() const noexcept
    {
        return get();
    }

    /// Returns the character at offset \param off of current position. If out of range, it returns end or start respectively. \throw never throws.
    inline char const &operator[]( std::ptrdiff_t off ) const noexcept
    {
        if( off > 0 ) {
            if( Remaining() < static_cast<std::size_t>(off) ) {
                return *end;
            }
        } else if( off < 0 ) {
            if( Processed() < static_cast<std::size_t>(std::abs( off )) ) {
                return *start;
            }
        }
        return *(cur + off);
    }

    /// Returns the character at offset \param off of current position. If out of range, it returns end or start respectively. \throw never throws.
    inline char const &operator[]( std::size_t off ) const noexcept
    {
        if( Remaining() < off ) {
            return *end;
        }

        return *(cur + off);
    }

    // convenience
#if UINT_MAX != SIZE_MAX
    inline char const &operator[]( int off ) const noexcept { return operator[]( static_cast<std::ptrdiff_t>(off) ); }
    inline char const &operator[]( unsigned int off ) const noexcept { return operator[]( static_cast<std::size_t>(off) ); }
#endif

    /// Returns whether the character at current position is equal to char \param c.
    inline bool operator==( char const c ) const noexcept
    {
        return c == get();
    }
    /// Returns whether the character at current position is unequal to char \param c.
    inline bool operator!=( char const c ) const noexcept { return get() != c; }
    inline bool operator<(  char const c ) const noexcept { return get() <  c; }
    inline bool operator>(  char const c ) const noexcept { return get() >  c; }
    inline bool operator<=( char const c ) const noexcept { return get() <= c; }
    inline bool operator>=( char const c ) const noexcept { return get() >= c; }

    /// Advances the current position by one if there is at least one more character remaining, eventually updates line or column if possible and necessary.
    /// If the current position is at end already nothing is happened and the object is returned unchanged.
    /// \note This is the pre-increment operator, e.g. ++c;
    Content &operator++() noexcept // pre
    {
        // we don't throw and don't pass the end. In that case it is a no-op.
        if( cur != end ) {
            if( *cur == '\n' ) {
                next_line();
            } else {
                next_column();
            }
            ++cur;

        }
        return *this;
    }

    /// Advances the current position by one if there is at least one more character remaining, eventually updates line or column if possible and necessary
    /// but returns an object with the old (unchanged) state.
    /// If the current position is at end already nothing is happened and the object stays unchanged.
    /// \note This is the post-increment operator, e.g. c++;
    Content operator++( int ) noexcept // post
    {
        // we don't throw and don't pass the end. In that case it is a no-op.
        auto const res = *this;
        ++(*this);
        return res;
    }

    /// Decrements the current position by one if there is at least one previous character remaining, eventually updates line or column if possible and necessary.
    /// If the current position is at start already nothing is happened and the object is returned unchanged.
    /// \note This is the pre-decrement operator, e.g. --c;    
    Content &operator--() noexcept // pre
    {
        // we don't throw and don't pass the start. In that case it is a no-op.
        if( cur != start ) {
            --cur;
            if( *cur == '\n' ) {
                previous_line();
            } else {
                previous_column();
            }
        }
        return *this;
    }

    /// Decrements the current position by one if there is at least one previous character remaining, eventually updates line or column if possible and necessary
    /// but returns an object with the old (unchanged) state.
    /// If the current position is at start already nothing is happened and the object stays unchanged.
    /// \note This is the post-decrement operator, e.g. c--;
    Content operator--( int ) noexcept // post
    {
        // we don't throw and don't pass the start. In that case it is a no-op.
        auto const res = *this;
        --(*this);
        return res;
    }

    /// Advances the current position of a copy of this content by \param distance characters. The position will not go behind end.
    /// If \param distance is 0 or the current position is at end already the content returned will be equal to this content.
    Content operator+( size_t distance ) const noexcept
    {
        Content res( *this );
        if( distance < distance_threshold ) {
            for( size_t i = 0; i != distance; ++i ) {
                ++res;
            }
        } else {
            if( res.Remaining() >= distance ) {
                res.cur += distance;
            } else {
                res.cur = res.end;
            }
            res.line = -1;
            res.column = -1;
        }
        return res;
    }

    /// Advances the current position of this content by \param distance characters. The position will not go behind end.
    /// If \param distance is 0 or the current position is at end already the content stays unchanged.
    Content &operator+=( size_t distance ) noexcept
    {
        *this = *this + distance;
        return *this;
    }

    /// Decrement the current position of a copy of this content by \param distance characters. The position will not go before start.
    /// If \param distance is 0 or the current position is at start already the content returned will be equal to this content.
    Content operator-( size_t distance ) const noexcept
    {
        Content res( *this );
        if( distance < distance_threshold ) {
            for( size_t i = 0; i != distance; ++i ) {
                --res;
            }
        } else {
            if( res.Processed() >= distance ) {
                res.cur -= distance;
            } else {
                res.cur = res.start;
            }
            res.line = -1;
            res.column = -1;
        }
        return res;
    }

    /// Decrements the current position of this content by \param distance characters. The position will not go before start.
    /// If \param distance is 0 or the current position is at start already the content stays unchanged.
    Content &operator-=( size_t distance ) noexcept
    {
        *this = *this - distance;
        return *this;
    }

    /// Sets the current position regardless of the actual position to the absolute character index \param absolute.
    /// If \param absolute indexes behind the last character, the current position will be set to the last valid index/character.
    void JumpToIndex( size_t absolute ) noexcept
    {
        Rewind();
        *this += absolute;
    }

    /// Moves the current position and column accordingly by \param distance characters forwards or backwards _without_ checking for line breaks!
    /// If \param distance indexes behind the last or before the first character, the current position will be set to the last (or first) valid index/character.
    void MoveInLine_Unchecked( int distance ) noexcept
    {
        if( distance > 0 ) { // forward
            if( Remaining() >= static_cast<size_t>(distance) ) {
                cur += distance;
                lazy_update_column( distance );
            } else { // should not happen when a check was done outside. So we do it the slow way...
                *this += static_cast<size_t>(distance);
            }
        } else if( distance < 0 ) { // backward
            if( Processed() >= static_cast<size_t>(-distance) ) {
                cur += distance; // distance is negative!
                lazy_update_column( distance );
            } else { // should not happen when a check was done outside. So we do it the slow way...
                *this -= static_cast<size_t>(-distance);
            }
        } // else: distance == 0 --> nothing to do.
    }

    /// Increments the current position by one and updates column _without_ checking for line breaks.
    /// This is useful when you are operating inside the same line and checking for line feed by your self.
    void IncInLine_Unchecked() noexcept
    {
        // we don't throw and don't pass the end. In that case it is a no-op.
        if( cur != end ) {
            next_column(); // we don't check for line feed.
            ++cur;
        }
    }

    /// Will go to line \param line and column \param col as current position if exists. Possible line_offset is taken into account.
    /// \throw std::invalid_argument if either \param line is smaller 1 or \param col is smaller 1.
    /// \throw std::out_of_range if the desired position does not exist.
    /// \post: If any exception is thrown the object stays unchanged. Otherwise current position, line and column is set as decribed.
    /// NOTE: implementation detail: the search will always start from the begin of the whole content regardless from the current position!
    void GoTo( long long const to_line, long long const to_col = 1 )
    {
        if( to_line < 1 || to_col < 1 ) {
            throw std::invalid_argument( "GoTo( line, col ): line < 1 || col < 1" );
        }
        long long c = 1 + line_offset;
        auto p = start;
        for( ; p != end && c != to_line; ++p ) {
            if( *p == '\n' ) {
                ++c;
            }
        }
        // line found?
        if( to_line == c ) {
            // search for column within the found line...
            for( c = 1; p != end && c != to_col; ++p ) {
                if( *p == '\n' ) { // end of line?
                    break; // ... column not found.
                }
                ++c;
            }
            // column found?
            if( to_col == c ) {
                // yes, set values and return (no exception)
                this->cur    = p;
                this->line   = to_line;
                this->column = to_col;
                return;
            } // else: exception!
        } // else: exception!

        throw std::out_of_range( "GoTo position is out of range!" );
    }

    /// fast move to next line feed character.
    inline void MoveToLineFeed() noexcept
    {
        char const *const  s = cur;
        while( cur != end && *cur != '\n' ) ++cur;
        lazy_update_column( static_cast<int>(std::distance( s, cur )) );
    }

    // TODO: maybe add MoveToLine( relative_line ) or better ApplyLineFeeds( int )? but this is more likely only useful for editing....

};


} // namespace teascript


