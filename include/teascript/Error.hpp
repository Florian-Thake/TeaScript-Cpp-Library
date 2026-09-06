/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2025 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include <system_error>

namespace teascript {

/// The different error values. These are not yet complete and will be extended.
enum class eError
{
    // skip 0 for now
    RuntimeError = 1,
    NotAValue,
    OutOfRange,
};


/// Error type of TeaScript. Consisting of an error code and an error message.
class Error
{
    //TODO: Decide whether to use one of these or use only own enum!
    //std::error_condition  mErrorCond;
    //std::error_code  mErrorCode; // C++ API use this, e.g. std::filesystem!!
    eError      mCode;
    std::string mMessage;

public:
    Error( eError const code, std::string const & rMessage )
        : mCode(code)
        , mMessage(rMessage)
    {
    }

    static Error MakeRuntimeError( std::string const &rMessage )
    {
        return Error( eError::RuntimeError, rMessage );
    }

    static Error MakeNotAValueError()
    {
        return Error( eError::NotAValue, "Resulted in not a valid value!" );
    }

    static Error MakeOutOfRangeError( std::string const &rMessage )
    {
        return Error( eError::OutOfRange, rMessage );
    }

    int Code() const
    {
        return static_cast<int>(mCode);
    }

    std::string Name() const
    {
        switch( mCode ) {
        case eError::RuntimeError:
            return "Runtime Error";
        case eError::NotAValue:
            return "Not A Value";
        default:
            return "Unknown Error";
        }
        
    }

    std::string const & Message() const
    {
        return mMessage;
    }

    std::string ToDisplayString() const
    {
        if( mMessage.empty() ) {
            return Name() + "!";
        }
        return Name() + ": " + mMessage;
    }
};

} // namespace teascript

