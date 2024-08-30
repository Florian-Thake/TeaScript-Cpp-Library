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

#if __has_include( <boost/json/fwd.hpp> )
# if defined( _MSC_VER )
#  define BOOST_CONTAINER_NO_LIB
#  define BOOST_JSON_NO_LIB
# endif
# include <boost/json/fwd.hpp>
#else
# error You must add the include directory for boost to your project.
#endif


namespace teascript {

class JsonAdapterBoost
{
public:
    using JsonType = boost::json::value;

    static constexpr char Name[] = "Boost.Json";

    /// Constructs a ValueObject from the given Json formatted string.
    static ValueObject ReadJsonString( Context &rContext, std::string const &rJsonStr );

    /// Constructs a Json formatted string from the given ValueObject. 
    /// \return the constructed string or false on error.
    /// \note the object must only contain supported types and layout for Json.
    static ValueObject WriteJsonString( ValueObject const &rObj );

    static ValueObject ToValueObject( Context &rContext, JsonType const &json );

    static void FromValueObject( ValueObject const &rObj, JsonType &rOut );
};


} // namespace teascript
