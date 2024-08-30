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

#if __has_include( "rapidjson/fwd.h" )
/* on x86_64 we always have SSE2 */
# if (defined(_M_AMD64) && !defined( _M_ARM64EC )) || defined(__amd64__)
#  define RAPIDJSON_SSE2
# endif
# include "rapidjson/fwd.h"
# include "rapidjson/document.h" // we need Document::ValueType here :-(
#else
# error You must add the include directory for RapidJson to your project.
#endif


namespace teascript {

class JsonAdapterRapid
{
public:
    using JsonType = rapidjson::Document;
    using JsonValue = rapidjson::Document::ValueType; // we cannot use this as the type since we need the Allocator from the Document. :-(

    static constexpr char Name[] = "RapidJSON";

    /// Constructs a ValueObject from the given Json formatted string.
    static ValueObject ReadJsonString( Context &rContext, std::string const &rJsonStr );

    /// Constructs a Json formatted string from the given ValueObject. 
    /// \return the constructed string or false on error.
    /// \note the object must only contain supported types and layout for Json.
    static ValueObject WriteJsonString( ValueObject const &rObj );

    static ValueObject ToValueObject( Context &rContext, JsonValue const &json );

    static void FromValueObject( ValueObject const &rObj, JsonType &rOut );
    static void FromValueObject( ValueObject const &rObj, JsonType &rDoc, JsonValue &rOut ); // needed because we always need the root document for obtain the Allocator! :-(
};

} // namespace teascript
