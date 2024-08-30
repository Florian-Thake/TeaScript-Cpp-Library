/* === Part of TeaScript C++ Library ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once


// Define this if you want to disable JSON suppport completely.
//#define TEASCRIPT_DISABLE_JSONSUPPORT    1

// Define this to the desired JSON flavor used in TeaScript, Pico Json is the default and is shipped with TeaScript.
// If you want to use any other flavor, you must add the corresponding adapter to your project and ensure that the 
// includes will be found, e.g. add JsonAdapterNlohmann.cpp to your project and the include path to find #include "nlohmann/json.hpp"
//#define TEASCRIPT_JSON_FLAVOR            TEASCRIPT_JSON_PICO


#if !TEASCRIPT_DISABLE_JSONSUPPORT

# define TEASCRIPT_JSON_PICO              1     /* Pico Json https://github.com/kazuho/picojson */
# define TEASCRIPT_JSON_NLOHMANN          2     /* nlohmann json https://github.com/nlohmann/json */
# define TEASCRIPT_JSON_RAPID             3     /* Rapid Json https://github.com/Tencent/rapidjson */
# define TEASCRIPT_JSON_BOOST             4     /* Boost Json https://github.com/boostorg/json */
//# define TEASCRIPT_JSON_USER_DEFINED      0xff  /* user defined */

// You can define this macro to a value from above to select a different JSON flavor. defualt is Pico Json.
# if !defined( TEASCRIPT_JSON_FLAVOR )
#  define TEASCRIPT_JSONSUPPORT            TEASCRIPT_JSON_PICO
# else 
#  define TEASCRIPT_JSONSUPPORT            TEASCRIPT_JSON_FLAVOR
# endif
#else
# define TEASCRIPT_JSONSUPPORT            0
#endif



#if TEASCRIPT_JSONSUPPORT

#if TEASCRIPT_JSONSUPPORT == TEASCRIPT_JSON_PICO
# include "JsonAdapterPico.hpp"
namespace teascript {
using JsonAdapter = JsonAdapterPico;
}
#elif TEASCRIPT_JSONSUPPORT == TEASCRIPT_JSON_NLOHMANN
#if __has_include("teascript/ext/JsonAdapterNlohmann.hpp")
# include "teascript/ext/JsonAdapterNlohmann.hpp"
#else
# error You must add the include directory for teascript extensions to your project!
#endif
namespace teascript {
using JsonAdapter = JsonAdapterNlohmann;
}
#elif TEASCRIPT_JSONSUPPORT == TEASCRIPT_JSON_RAPID
#if __has_include("teascript/ext/JsonAdapterRapid.hpp")
# include "teascript/ext/JsonAdapterRapid.hpp"
#else
# error You must add the include directory for teascript extensions to your project!
#endif
namespace teascript {
using JsonAdapter = JsonAdapterRapid;
}
#elif TEASCRIPT_JSONSUPPORT == TEASCRIPT_JSON_BOOST
#if __has_include("teascript/ext/JsonAdapterBoost.hpp")
# include "teascript/ext/JsonAdapterBoost.hpp"
#else
# error You must add the include directory for teascript extensions to your project!
#endif
namespace teascript {
using JsonAdapter = JsonAdapterBoost;
}
#else
# error unsupported JSON flavor for TeaScript
#endif


namespace teascript {

template< class Adapter = JsonAdapter >
class JsonSupport
{
public:
    /// The Json Type for interchange on C++ level. The type depends on the underlying Json library.
    using JsonType = typename Adapter::JsonType;

    static std::string_view GetAdapterName()
    {
        return std::string_view( Adapter::Name );
    }

    /// Constructs a ValueObject structure from the given Json formatted string.
    static ValueObject ReadJsonString( Context &rContext, std::string const &rJsonStr )
    {
        return Adapter::ReadJsonString( rContext, rJsonStr );
    }

    /// Constructs a Json formatted string from the given ValueObject. 
    /// \return the constructed string or false on error.
    /// \note the object must only contain supported types and layout for Json.
    static ValueObject WriteJsonString( ValueObject const &rObj )
    {
        return Adapter::WriteJsonString( rObj );
    }

    /// Constructs a ValueObject from a given json root value. If the out ValueObject is of type TypeInfo an error occurred.
    /// \note Transition: The type for the error case will be changed to Error in some next future release!
    static void JsonToValueObject( Context &rContext, ValueObject &rOut, JsonType const &root )
    {
        rOut = Adapter::ToValueObject( rContext, root );
    }

    /// Constructs a Json Object of the underlying Json library from the given ValueObject. \throws on error.
    static void JsonFromValueObject( ValueObject const &rObj, JsonType &rOut )
    {
        Adapter::FromValueObject( rObj, rOut );
    }
};

} // namespace teascript

#endif // TEASCRIPT_JSONSUPPORT

