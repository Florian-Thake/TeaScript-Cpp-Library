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

#include "ValueObject.hpp"


namespace teascript {

namespace control {

class ControlBase : public std::exception
{
    std::string mName;
public:
    ControlBase( std::string const &rName = "" )
        : std::exception()
        , mName( rName )
    {
    }

    std::string const &GetName() const
    {
        return mName;
    }
};

class Stop_Loop : public ControlBase
{
    ValueObject mResult;
public:
    Stop_Loop( ValueObject &&rResult = ValueObject(), std::string const &rName = "" )
        : ControlBase( rName )
        , mResult( std::move( rResult ) )
    {

    }

    ValueObject const &GetResult() const
    {
        return mResult;
    }

    const char *what() const noexcept override
    {
        return "teascript::control::Stop_Loop";
    }
};

class Loop_To_Head : public ControlBase
{
public:
    Loop_To_Head( std::string const &rName = "" )
        : ControlBase( rName )
    {

    }

    const char *what() const noexcept override
    {
        return "teascript::control::Loop_To_Head";
    }
};

class Return_From_Function : public ControlBase
{
    ValueObject mResult;
public:
    Return_From_Function( ValueObject &&rResult = ValueObject() )
        : ControlBase()
        , mResult( std::move( rResult ) )
    {
    }

    ValueObject const &GetResult() const
    {
        return mResult;
    }

    ValueObject &&MoveResult()
    {
        return std::move( mResult );
    }

    const char *what() const noexcept override
    {
        return "teascript::control::Return_From_Function";
    }
};

class Exit_Script : public ControlBase
{
    ValueObject mResult;
public:
    Exit_Script( ValueObject &&rResult = ValueObject() )
        : ControlBase()
        , mResult( std::move( rResult ) )
    {
    }

    explicit Exit_Script( long long const code )
        : ControlBase()
        , mResult( code )
    {
    }

    ValueObject const &GetResult() const
    {
        return mResult;
    }

    const char *what() const noexcept override
    {
        return "teascript::control::Exit_Script";
    }
};

} // namespace control

} // namespace teascript
