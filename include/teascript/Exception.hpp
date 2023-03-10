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

#include <stdexcept>
#include <string>
#include <memory>

#include "SourceLocation.hpp"

/// \brief TeaScript Exceptions.
/// The mainly used exceptions in TeaScript are divided in 2 sub categories:
/// parsing error (during parsing) and evaluation error (during evaluation).
/// Both have the teascript::exception::runtime_error as a common base. Thus,
/// both of them have an optionally SourceLoction available.
/// There is one more exception teascript::exception::bad_value_cast, which has
/// std::bad_any_cast as base.
/// Another exception category is for control flow in the teascript::control::*
/// Those exceptions are not meant for the user level, but might escape from
/// the inner teascript library if either a user or library bug exists.

namespace teascript {

namespace exception {

/// The base class for the most of all exceptions in TeaScript. \see teascript::exception::bad_value_cast for another exception.
class runtime_error : public std::runtime_error
{
protected:
    SourceLocation const  mLoc;
    std::string    const  mErrorStr;
public:
    explicit runtime_error( std::string const &rText ) : std::runtime_error( rText ), mLoc(), mErrorStr() {}

    runtime_error( SourceLocation loc, std::string const & rText ) : std::runtime_error( rText ), mLoc( std::move(loc)), mErrorStr() {}

    runtime_error( SourceLocation loc, std::string const &rErrorStr, std::string const &rText ) : std::runtime_error( rText ), mLoc( std::move( loc ) ), mErrorStr(rErrorStr) {}

    runtime_error( long long const line, long long const col, std::string lineStr, std::shared_ptr<std::string> const &rFile, std::string const &rErrorStr, std::string const &rText )
        : runtime_error( SourceLocation( line, col ).AddSource(std::move(lineStr)).AddFile(rFile), rErrorStr, rText )
    { }

    runtime_error( std::shared_ptr< std::string > const &rFile, std::string const &rErrorStr, std::string const &rText )
        : runtime_error( SourceLocation().AddFile(rFile), rErrorStr, rText )
    { }

    virtual std::string_view GetGategory() const
    {
        using namespace std::string_view_literals;
        return "Runtime"sv;
    }

    /// returns either the set error_str (if any) or the return value of what().
    char const *ErrorStr_or_What() const noexcept
    {
        if( mErrorStr.empty() ) {
            return what();
        } else {
            return mErrorStr.c_str();
        }
    }

    inline bool IsSourceLocSet() const noexcept // convenience
    {
        return mLoc.IsSet();
    }

    inline SourceLocation const &GetSourceLocation() const noexcept
    {
        return mLoc;
    }

    std::string const &GetFileStr() const noexcept
    {
        return mLoc.GetFileName(); // this is unconditional safe.
    }

    /// The specific error string. note: this is not necessarily the text returned by what() and/or it might be empty. \see ErrorStr_or_What()
    std::string const &GetErrorStr() const noexcept
    {
        return mErrorStr;
    }

    long long GetLine() const noexcept // convenience (left over by old parsing_error)
    {
        return mLoc.GetStartLine();
    }

    long long GetColumn() const noexcept // convenience (left over by old parsing_error)
    {
        return mLoc.GetStartColumn();
    }

    std::string const & GetContextStr() const // convenience (left over by old parsing_error)
    {
        return mLoc.GetSource();
    }
};

/// Base class for all exception during parsing time indicating a parsing error.
class parsing_error : public runtime_error
{
public:
    parsing_error( std::shared_ptr< std::string > const &rFile, std::string const &rText )
        : runtime_error( rFile, rText, std::string("TeaScript parsing error in file ") + *rFile + ": " + rText )
    {
    }

    parsing_error( long long const line, long long const col, std::string lineStr, std::shared_ptr<std::string> const &rFile, std::string const &rText )
        : runtime_error( line, col, std::move(lineStr), rFile, rText,
                         std::string( "TeaScript parsing error at line ") + std::to_string( line ) + ", column " + std::to_string( col ) 
                                    + " in file " + *rFile + ": " + rText )
    {
    }

    parsing_error( SourceLocation loc, std::string const &rText )
        : runtime_error( std::move( loc ), rText )
    {
    }

    std::string_view GetGategory() const override
    {
        using namespace std::string_view_literals;
        return "Parsing"sv;
    }
};

/// This exception might be thrown during parsing if an operand for the left-hand-side of a binary operator is missing.
class lhs_missing : public parsing_error
{
public:
    lhs_missing( std::shared_ptr<std::string> const &rFile, std::string const &rText )
        : parsing_error( rFile, rText )
    { 
    }

    lhs_missing( SourceLocation loc, std::string const &rText )
        : parsing_error( std::move( loc ), rText )
    {
    }
};

/// The common base class for all kind of errors during evaluation which indicates broken/buggy code.
class eval_error : public runtime_error
{
public:
    eval_error( SourceLocation const &rLoc, std::string const &rText ) : runtime_error( rLoc, rText ) {}
    eval_error( std::string const &rText ) : runtime_error( rText ) {}

    std::string_view GetGategory() const override
    {
        using namespace std::string_view_literals;
        return "Eval"sv;
    }
};

/// Exception thrown if an identifier is not found.
class unknown_identifier : public eval_error
{
public:
    unknown_identifier( SourceLocation const &rLoc, std::string const &rIdentifier ) : eval_error( rLoc, "Unknown identifier: \"" + rIdentifier + "\"!" ) {}
    unknown_identifier( std::string const &rIdentifier ) : unknown_identifier( SourceLocation(), rIdentifier ) {}
};

/// Exception thrown if a variable with same name on same scope level exists already.
class redefinition_of_variable : public eval_error
{
public:
    redefinition_of_variable( SourceLocation const &rLoc, std::string const &rIdentifier ) : eval_error( rLoc, "Redefinition of variable: \"" + rIdentifier + "\"!" ) {}
    redefinition_of_variable( std::string const &rIdentifier ) : redefinition_of_variable( SourceLocation(), rIdentifier ) {}
};

/// Exception thrown if a variable was declared but not initialized.
class declare_without_assign : public eval_error
{
public:
    declare_without_assign( SourceLocation const &rLoc, std::string const &rIdentifier ) : eval_error( rLoc, "Declared identifier \"" + rIdentifier + "\" without assignment!" ) {}
    declare_without_assign( std::string const &rIdentifier ) : declare_without_assign( SourceLocation(), rIdentifier ) {}
};

/// Exception thrown if a name is not allowed to use / reserved for internal usage.
class internal_name : public eval_error
{
public:
    internal_name( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Internal names (starting with '_') cannot be defined/undefined!" ) {}
};

/// Exception thrown if a type of a value does not match the requirements.
class type_mismatch : public eval_error
{
public:
    type_mismatch( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Type mismatch! Cannot assign different types! No conversion rules found!" ) {}
};

/// Exception thrown if a const value was attempted to change.
class const_assign : public eval_error
{
protected:
    const_assign( SourceLocation const &rLoc, std::string const & rText ) : eval_error( rLoc, rText ) {}
public:
    const_assign( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Const assign: Variable is const! Cannot assign to const variables!" ) {}
};

/// Exception thrown if try to share a const variable as mutable.
class const_shared_assign : public const_assign
{
public:
    const_shared_assign( SourceLocation const &rLoc = {} ) : const_assign( rLoc, "Const shared assign: Cannot share a const variable as non-const object!" ) {}
};

/// Exception thrown if a integer division by zero was prevented.
class division_by_zero : public eval_error
{
public:
    division_by_zero( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Division by zero!" ) {}
};

/// Exception thrown if a floating point modulo operation was prevented.
class modulo_with_floatingpoint : public eval_error
{
public:
    modulo_with_floatingpoint( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Modulo operator not available for floating point numbers!" ) {}
};

/// Exception thrown if an index was out of range.
class out_of_range : public eval_error
{
public:
    out_of_range( std::string const &rText, SourceLocation const &rLoc = {} ) : eval_error( rLoc, rText ) {}
    out_of_range( SourceLocation const &rLoc = {} ) : out_of_range( "Invalid index! Index is out of range!", rLoc ) {}
};

/// Exception thrown if a file could not be opened or read.
class load_file_error : public eval_error
{
public:
    load_file_error( SourceLocation const &rLoc, std::string const &rFile ) : eval_error( rLoc, "Cannot open/read file \"" + rFile + "\"!" ) {}
    load_file_error( std::string const &rFile ) : load_file_error( SourceLocation(), rFile ) {}
};

} // namespace exception

} // namespace teascript
