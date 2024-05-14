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
#include <limits>

#include "SourceLocation.hpp"

/// \brief TeaScript Exceptions.
/// All exceptions thrown by TeaScript are at least based on std::exception.
/// The great majority of them have the teascript::exception::runtime_error
/// as a common base (derived by std::runtime_error), which is able to carry
/// a teascript::SourceLocation to indicate the (TeaScript) source location
/// of the error.
/// There are 5 sub categories of teascript::exception::runtime_error:
///     1. parsing_error: can be thrown during parsing of the code.
///        e.g., a syntax error was detected.
///     2. eval_error: mainly thrown during AST evaluation, but might be 
///        thrown as well during execution in TeaStackVM or during compilation.
///     3. compile_error: can be thrown during compilation of the AST into a 
///        TeaScript binary for the TeaStackVM.
///     4. pure runtime_error: used for all other kind of errors, especially
///        if a faulty state is detected or if another category cannot be
///        clearly assigned.
///     5. bad_value_cast: thrown if a teascript::ValueObject does not contain
///        the expected/requested value type.
/// Another exception category is for the control flow of the recursively 
/// evaluated AST code in the teascript::control::*.
/// Those exceptions are not meant for the user level, but might escape from
/// the inner teascript library if either a user or library bug exists. These
/// control flow exceptions are only used when the code is evaluated via
/// recursive AST walking and _not_ when executing a compiled TeaScript code
/// inside the TeaStackVM.

namespace teascript {

namespace exception {

/// The base class for the most of all exceptions in TeaScript.
class runtime_error : public std::runtime_error
{
protected:
    SourceLocation  mLoc;
    std::string     mErrorStr;
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

    inline void SetSourceLocation( SourceLocation const &rLoc )
    {
        mLoc = rLoc;
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

/// The common base class / exception for all compilation related errors.
class compile_error : public runtime_error
{
public:
    compile_error( SourceLocation const &rLoc, std::string const &rText ) : runtime_error( rLoc, rText ) {}
    compile_error( std::string const &rText ) : runtime_error( rText ) {}

    std::string_view GetGategory() const override
    {
        using namespace std::string_view_literals;
        return "Compile"sv;
    }
};

/// The common base class for all kind of errors during evaluation/execution which indicates broken/buggy code.
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
    type_mismatch( std::string const &rText, SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Type mismatch! " + rText ) {}
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

/// Exception thrown if an integer overflow occurs and was detected.
class integer_overflow : public eval_error
{
public:
    integer_overflow( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Integer overflow!" ) {}
    template< typename T, std::integral U > requires( std::is_integral_v<T> || std::is_floating_point_v<T> )
    integer_overflow( T val, U /*dummy*/, SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Integer overflow with " + std::to_string( val ) + ", limits: "
                                                                                          + std::to_string( std::numeric_limits<U>::min() ) + ", "
                                                                                          + std::to_string( std::numeric_limits<U>::max() ) )
    {
    }
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

/// Exception thrown if a suspend or yield statement is executed in eval mode (suspend/yield statement is only supported in compiled mode.)
class suspend_statement : public eval_error
{
public:
    explicit suspend_statement( SourceLocation const &rLoc = {} ) : eval_error( rLoc, "Suspend/Yield statement is only supported when executed via TeaStackVM (as a compiled script)!" ) {}
};

} // namespace exception

} // namespace teascript
