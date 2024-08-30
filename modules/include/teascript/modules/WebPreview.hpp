/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */
#pragma once

#include "teascript/IModule.hpp"

namespace teascript {

class ValueObject;

/// EXPERIMENTAL module for web / http functionality (Preview 0).
/// If this module is loaded into the Context, it offers some easy functions to issue
/// http requests (client), e.g., web_get() and web_post() as well as the more generic
/// web_build_request() + web_request() for issuing all kind of http requests.
/// The module uses the Json support of TeaScript for handling Content-Type with
/// "application/json". It automatically transforms the Json string into a Tuple representation.
/// Also you can send Tuple objects as Json string payload as well.
/// Use the utility functions web_add_header() and web_set_payload() for manipulate/set header
/// values and add a payload to the message.
/// For the web server side use web_server_setup(), web_server_accept() and web_server_reply().
/// The latter comes with a utility function web_server_build_reply() for easily setup a reply message.
class WebPreviewModule : public IModule
{
public:
    /// \return the name of the module.
    std::string_view GetName() const override
    {
        using namespace std::string_view_literals;
        return "WebPreview"sv;
    }

    /// Loads the Module into the given context. config options and eval_only are considered as a hint only.
    /// Actually, the config value is ignored in this module. Should be set to config::full() for best future compatibility.
    void Load( Context &rInto, config::eConfig const config, bool const eval_only ) override;

    /// Issuing an http request and returns the received reply as a Tuple.
    /// The request must be a Tuple with all needed elements set. See web_build_request() for details.
    static ValueObject HttpRequest( Context &rContext, ValueObject const &rRequest );

    /// Setup a listening server and returns the 'server' object as a Tuple object.
    /// The parameter must be a named tuple of the form (("host", <name or ip as string>), ("port", <port as string or int>)).
    static ValueObject HttpServerSetup( Context &rContext, ValueObject const &rParams );

    /// Blocks until a new client connection arrives. \returns the request message of the client as a Tuple.
    static ValueObject HttpServerAcceptOne( Context &rContext, ValueObject const &rServer );
    /// Sends the reply to the client and closes(!) the connection. See web_server_build_reply for details to the reply parameter.
    static ValueObject HttpServerReply( Context &rContext, ValueObject const &rReply );
};

} // namespace teascript
