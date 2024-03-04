/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <stdexcept>
#include <string>
#include <memory>

namespace teascript {

namespace priv_ {
inline static const std::string unknown_file_ = "<unknown>";
}

/// class for a location inside TeaScript code.
class SourceLocation
{
    long long mStartLine;
    long long mStartColumn;
    long long mEndLine;
    long long mEndColumn;

    std::shared_ptr<std::string>  mFile;  // shared instance of the file name.

    std::string  mSource;        // optional copy of the first source line with the relevant part.

    inline bool validate() const noexcept
    {
        if( mStartLine == -1LL ) { // not set
            return true;
        }
        if( mStartLine <= 0LL || mEndColumn <= 0LL || mEndLine < mStartLine || (mEndLine == mStartLine && mEndColumn < mStartColumn) ) {
            return false;
        }
        return true;
    }

public:
    SourceLocation() : SourceLocation( -1LL, 0LL ) {}

    explicit SourceLocation( std::shared_ptr<std::string> const &rFile, long long start_line = -1LL, long long start_column = 1LL )
        : SourceLocation( start_line, start_column )
    {
        SetFile( rFile );
    }

    explicit SourceLocation( long long start_line, long long start_column = 1LL )
        : mStartLine( start_line )
        , mStartColumn( start_column )
        , mEndLine( start_line )
        , mEndColumn( start_column )
    {
        if( !validate() ) {
            throw std::invalid_argument( "SourceLocation: mStartLine <= 0LL || mEndColumn <= 0LL || mEndLine < mStartLine || mEndColumn < mStartColumn" );
        }
    }

    SourceLocation( long long start_line, long long start_column, long long end_line, long long end_column )
        : mStartLine( start_line )
        , mStartColumn( start_column )
        , mEndLine( end_line )
        , mEndColumn( end_column )
    {
        if( !validate() ) {
            throw std::invalid_argument( "SourceLocation: mStartLine <= 0LL || mEndColumn <= 0LL || mEndLine < mStartLine || mEndColumn < mStartColumn" );
        }
    }

    /// \return whether this instance contains set data.
    inline bool IsSet() const noexcept
    {
        return mStartLine > 0LL;
    }

    /// sets the (optional) end of the source code location. 
    /// The end line must be >= start line and end column must be >= start column if end line is equal to start line. \throw std::invalid_argument
    void SetEnd( long long end_line, long long end_column )
    {
        auto const  s = SourceLocation( mStartLine, mStartColumn, end_line, end_column ); // will throw on validation error.
        mEndLine      = s.GetEndLine();
        mEndColumn    = s.GetEndColumn();
    }

    /// adds an end for the source code location \see SetEnd() and \return a lvalue reference of this.
    SourceLocation &AddEnd( long long end_line, long long end_column ) &
    {
        SetEnd( end_line, end_column );
        return *this;
    }

    /// adds an end for the source code location \see SetEnd() and \return a rvalue refrence of a moved this.
    SourceLocation && AddEnd( long long end_line, long long end_column ) &&
    {
        SetEnd( end_line, end_column );
        return std::move( *this );
    }

    /// sets an optional source code string for start line. start column must be in range for the set source. \throw std::out_of_range
    void SetSource( std::string source )
    {
        if( mStartColumn <= 0LL || static_cast<size_t>(mStartColumn) > source.size() + 1 ) { // col start at 1
            throw std::out_of_range( "SourceLocation::SetSource: mStartColumn <= 0LL || mStartColumn > source.size() + 1" );
        }
        mSource = std::move( source );
    }

    /// adds a source code string \see SetSource() and \return a lvalue reference of this.
    SourceLocation &AddSource( std::string source ) &
    {
        SetSource( std::move( source ) );
        return *this;
    }

    /// adds a source code string \see SetSource() and \return a rvalue reference of a moved this.
    SourceLocation && AddSource( std::string source ) &&
    {
        SetSource( std::move( source ) );
        return std::move( *this );
    }

    /// sets the corresponding file name.
    void SetFile( std::shared_ptr<std::string> const &rFile )
    {
        mFile = rFile;
    }

    /// adds the corresponding file name and \return a lvalue reference of this.
    SourceLocation &AddFile( std::shared_ptr<std::string> const &rFile ) &
    {
        SetFile( rFile );
        return *this;
    }

    /// adds the corresponding file name and \return a rvalue reference of a moved this.
    SourceLocation && AddFile( std::shared_ptr<std::string> const &rFile ) &&
    {
        SetFile( rFile );
        return std::move( *this );
    }

    /// \return the corresponding file name. \throw never.
    inline std::string const & GetFileName() const noexcept
    {
        return mFile.get() != nullptr ? *mFile : priv_::unknown_file_;
    }

    /// \return the start line. \throw never.
    inline long long GetStartLine() const noexcept
    {
        return mStartLine;
    }

    /// \return the end line. \throw never.
    inline long long GetEndLine() const noexcept
    {
        return mEndLine;
    }

    /// \return the start column. \throw never.
    inline long long GetStartColumn() const noexcept
    {
        return mStartColumn;
    }

    /// \return the end column. \throw never.
    inline long long GetEndColumn() const noexcept
    {
        return mEndColumn;
    }

    /// \return the source code (might be empty). \throw never.
    inline std::string const &GetSource() const noexcept
    {
        return mSource;
    }
};

} // namespace teascript

