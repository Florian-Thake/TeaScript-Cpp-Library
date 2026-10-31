/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "teascript/ValueObject.hpp"
#include "teascript/Context.hpp"

#include <optional>

#if __has_include( "nlohmann/json_fwd.hpp" )
# include "nlohmann/json_fwd.hpp"
#else
# error You must add the include directory for nlohmann json to your project.
#endif


namespace teascript {

class JsonAdapterNlohmann
{
public:
    using JsonType = nlohmann::json;

    static constexpr char Name[] = "nlohmann::json";

    [[deprecated( "Please use the other overload without Context parameter!" )]]
    static ValueObject ReadJsonString( Context &/*rContext*/, std::string const &rJsonStr )
    {
        return ReadJsonString( rJsonStr );
    }

    /// Constructs a ValueObject from the given Json formatted string.
    /// \return the constructed ValueObject structure or one with an Error on error.
    static ValueObject ReadJsonString( std::string const &rJsonStr );

    /// Constructs a Json formatted string from the given ValueObject. 
    /// \return the constructed string or Error on error.
    /// \note the object must only contain supported types and layout for Json.
    static ValueObject WriteJsonString( ValueObject const &rObj );

    [[deprecated("Please use the other overload without Context parameter!")]]
    static ValueObject ToValueObject( Context &/*rContext*/, JsonType const &json )
    {
        return ToValueObject( json );
    }

    static ValueObject ToValueObject( JsonType const &json );

    static void FromValueObject( ValueObject const &rObj, JsonType &rOut );


    // nlohmann supports different binary formats. We support BSON here.

    [[deprecated( "Please use the other overload without Context parameter!" )]]
    static ValueObject FromBSON( Context &/*rContext*/, Buffer const &rBuffer )
    {
        return FromBSON( rBuffer );
    }

    static ValueObject FromBSON( Buffer const &rBuffer );
    static ValueObject ToBSON( ValueObject const &rObj );
};

} // namespace teascript
