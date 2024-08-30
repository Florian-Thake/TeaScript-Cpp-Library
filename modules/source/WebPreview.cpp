/* === Part of TeaScript C++ Library Extension ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

#include "teascript/modules/WebPreview.hpp"
#include "teascript/Context.hpp"
#include "teascript/LibraryFunctions.hpp"
#include "teascript/StackVMCompiler.hpp"
#include "teascript/StackMachine.hpp"
#include "teascript/JsonSupport.hpp"


#if defined( _MSC_VER )
# include <sdkddkver.h>  // for  _WIN32_WINNT if not defined by project
//#  define BOOST_CONTAINER_NO_LIB
//#  define BOOST_JSON_NO_LIB
#endif
#include "boost/beast.hpp"
#include "boost/algorithm/string.hpp"

// define this to 1 for add a Tuple with debugtimes to the result of web_request() which contains several time measurements.
#define TEASCRIPT_TIME_MEASURMENT   0

// for time measurement...
#if TEASCRIPT_TIME_MEASURMENT
#include <chrono>
namespace {
auto Now()
{
    return std::chrono::steady_clock::now();
}

double CalcTimeInSecs( auto s, auto e )
{
    std::chrono::duration<double> const  timesecs = e - s;
    return timesecs.count();
}
} // namespace {
#define TIMEMEASURE(name)  auto const name = Now()
#else
#define TIMEMEASURE(name)  ((void)0)
#endif


namespace teascript {


ValueObject WebPreviewModule::HttpRequest( Context &rContext, ValueObject const &rRequest )
{
    auto  const  cfg  = ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() );
    Tuple        res;
    try {
        TIMEMEASURE( start_time );
        if( rRequest.InternalType() != ValueObject::TypeTuple ) {
            throw std::invalid_argument( "Must provide a request Tuple structure as parameter!" );
        }
        Tuple const &req = rRequest.GetValue<Tuple>();
        if( not req.ContainsKey( "host" ) ) {
            throw std::invalid_argument( "Host not provided!" );
        }
        std::string const host = req.GetValueByKey( "host" ).GetAsString();
        std::string const port = req.ContainsKey( "port" ) ? req.GetValueByKey( "port" ).GetAsString() : std::string("80");
        std::string const path = req.ContainsKey( "path" ) ? req.GetValueByKey( "path" ).GetAsString() : std::string( "/" );

        std::string method = req.ContainsKey( "method" ) ? req.GetValueByKey( "method" ).GetValue<std::string>() : "GET";
        boost::to_upper( method );
        auto const http_method = boost::beast::http::string_to_verb( method );
        if( http_method == boost::beast::http::verb::unknown ) {
            throw std::invalid_argument( "Unknown http method!" );
        }
#if 0 // TBA: Possibly reuse io_context _AND_ the tcp connection //TODO: Implement keep alive + saving the tcp connection for later re-use
        auto ioc_ptr = req.ContainsKey( "io_context" )
            ? std::any_cast<std::shared_ptr<boost::asio::io_context>>(req.GetValueByKey( "io_context" ).GetPassthroughData())
            : std::make_shared<boost::asio::io_context>( 1 );
        if( not ioc_ptr ) {
            throw std::invalid_argument( "Invalid io_context!" );
        }
#else
        boost::asio::io_context  ioc;
#endif
        TIMEMEASURE( time_01 );
        boost::asio::ip::tcp::resolver  resolver( ioc );
        boost::beast::tcp_stream        stream( ioc );

        auto const endpoint = resolver.resolve( host, port );
        TIMEMEASURE( time_02 );
        // NOTE: if connect takes too long (> 1 sec), try to figure out if it is an IPv4 vs IPv6 problem where the wrong IP is tried first, 
        //       e.g., use 127.0.0.1 instead of localhost
        stream.connect( endpoint );
        TIMEMEASURE( time_03 );

        boost::beast::http::request<boost::beast::http::string_body>  request{http_method, path, 11};
        // fill in all header values
        if( req.ContainsKey( "header" ) ) {
            auto const &header_tup = req.GetValueByKey( "header" ).GetValue<Tuple>();
            for( auto const &[k, v] : header_tup ) {
                request.set( k, v.GetAsString() );
            }
        }

        // handle (optional) payload
        if( req.ContainsKey( "payload" ) ) {
            auto const & payload_val = req.GetValueByKey( "payload" );
#if TEASCRIPT_JSONSUPPORT
            // if payload is a Tuple we interpret it as json
            if( payload_val.GetTypeInfo()->IsSame<Tuple>() ) {
                // Tuple -> Json string
                ValueObject json_str_val = JsonSupport<>::WriteJsonString( payload_val );
                if( not json_str_val.GetTypeInfo()->IsSame<std::string>() ) {
                    throw std::invalid_argument( "payload cannot be transformed into a compatible json string!" );
                }

                request.body() = json_str_val.GetValue<std::string>();
                request.set( boost::beast::http::field::content_type, "application/json" );
            } else 
#endif
            {
                request.body() = payload_val.GetAsString();
            }

            // send as plain text if nothing is set yet.
            if( request.find( boost::beast::http::field::content_type ) == request.end() ) {
                request.set( boost::beast::http::field::content_type, "text/plain" );
            }

            // compute Content-Length
            request.prepare_payload();
        }
        TIMEMEASURE( time_04 );
        // send the request
        boost::beast::http::write( stream, request );
        TIMEMEASURE( time_05 );

        boost::beast::flat_buffer  buffer;
        boost::beast::http::response<boost::beast::http::dynamic_body> response;

        // receive the response.
        boost::beast::http::read( stream, buffer, response );
        TIMEMEASURE( time_06 );
        boost::beast::error_code ec;
        stream.socket().shutdown( boost::asio::ip::tcp::socket::shutdown_both, ec );
        // ignore error when shutdown...

        res.AppendKeyValue( "code", ValueObject(static_cast<long long>(response.result_int()), ValueConfig(true)) );
        res.AppendKeyValue( "reason", ValueObject( std::string(response.reason()), ValueConfig( true ) ) );

        Tuple  header;
        for( auto const &f : response ) {
            header.AppendKeyValue( f.name_string(), ValueObject( std::string( f.value() ), ValueConfig( true ) ) );
        }
        if( not header.IsEmpty() ) {
            res.AppendKeyValue( "header", ValueObject( std::move( header ), cfg ) );
        }

        std::string payload = boost::beast::buffers_to_string( response.body().data() );
        if( not payload.empty() ) {
#if TEASCRIPT_JSONSUPPORT
            // check if payload is json and if so build a Tuple structure from it.
            if( auto field = response.find( "Content-Type" ); field != response.end() ) {
                if( field->value().starts_with( "application/json" ) ) {
                    auto json = JsonSupport<>::ReadJsonString( rContext, payload );
                    if( not json.GetTypeInfo()->IsSame<TypeInfo>() ) { // bool (true/false) and NaV (null) is a possible result, so we use a TypeInfo as error indicator.
                        res.AppendKeyValue( "json", json );
                    }
                }
            }
#endif
            // add original payload as string always.
            res.AppendKeyValue( "payload", ValueObject( std::move( payload ), ValueConfig( true ) ) );
        }

        TIMEMEASURE( end_time );
#if TEASCRIPT_TIME_MEASURMENT
        Tuple timemeasure;
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( start_time, time_01 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_01, time_02 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_02, time_03 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_03, time_04 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_04, time_05 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_05, time_06 ), ValueConfig( true ) ) );
        timemeasure.AppendValue( ValueObject( CalcTimeInSecs( time_06, end_time ), ValueConfig( true ) ) );
        res.AppendKeyValue( "debugtimes", ValueObject( std::move( timemeasure ), cfg ) );
#endif

        return ValueObject( std::move( res ), cfg );
    } catch( boost::beast::system_error const &ex ) {
        
        res.AppendKeyValue( "error", ValueObject( static_cast<long long>(ex.code().value()), ValueConfig(true)));
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );
        
        return ValueObject( std::move( res ), cfg );
    } catch( std::invalid_argument const &ex ) {

        res.AppendKeyValue( "error", ValueObject( static_cast<long long>(std::errc::invalid_argument), ValueConfig( true ) ) );
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );
        
        return ValueObject( std::move( res ), cfg );
    } catch( std::exception const &ex ) {
        
        res.AppendKeyValue( "error", ValueObject( true, ValueConfig( true ) ) );
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );
        
        return ValueObject( std::move( res ), cfg );
    }
}


void WebPreviewModule::Load( Context &rInto, config::eConfig const /*config*/, bool const eval_only )
{
    auto const  cfg = ValueConfig( ValueShared, ValueConst, rInto.GetTypeSystem() );
    //auto const  cfg_mutable = ValueConfig( ValueShared, ValueMutable, rInto.GetTypeSystem() );

    {
        auto func = std::make_shared< LibraryFunction< decltype(HttpRequest) > >( &HttpRequest );
        ValueObject val{std::move( func ), cfg};
        rInto.AddValueObject( "web_request", val );
    }

    {
        auto func = std::make_shared< LibraryFunction< decltype(HttpServerSetup) > >( &HttpServerSetup );
        ValueObject val{std::move( func ), cfg};
        rInto.AddValueObject( "web_server_setup", val );
    }

    {
        auto func = std::make_shared< LibraryFunction< decltype(HttpServerAcceptOne) > >( &HttpServerAcceptOne );
        ValueObject val{std::move( func ), cfg};
        rInto.AddValueObject( "web_server_accept", val );
    }

    {
        auto func = std::make_shared< LibraryFunction< decltype(HttpServerReply) > >( &HttpServerReply );
        ValueObject val{std::move( func ), cfg};
        rInto.AddValueObject( "web_server_reply", val );
    }

    static constexpr char web_preview_code[] = R"_SCRIPT_(
// builds a web request Tuple which can be send via web_request()
// e.g., def result := web_request( web_build_request( "GET", "date.jsontest.com", "/", 80 ) )
func web_build_request( method, host, path, port )
{
    def req := _tuple_create()
    def req.method := method
    def req.host   := host as String
    def req.path   := path as String
    def req.port   := port
    
    def req.header :=  _tuple_create()
    def req.header.Host := req.host
    def req.header."User-Agent" := "TeaScript/%(_version_major).%(_version_minor).%(_version_patch)"

    req
}

func web_add_header( msg @=, name, value )
{
    if( not is_defined msg.header ) {
        def msg.header :=  _tuple_create()
    }
    _tuple_named_append( msg.header, name, value )
}

func web_set_payload( msg @=, const payload @= )
{
    if( is_defined msg.payload ) {
        undef msg.payload
    }
    def msg.payload := payload
}

// builds and send a http GET and returns the received result as a Tuple structure
// e.g., def res := web_get( "127.0.0.1" )
func web_get( host, path := "/", port := 80 )
{
    def req := web_build_request( "GET", host, path, port )
    
    web_request( req )
}

// builds and send a http POST and returns the received result as a Tuple structure.
// the payload can be either a String or a Tuple which will be transformed to a Json formatted String.
// e.g., def res := web_post( "127.0.0.1", json )
func web_post( host, const payload @=, path := "/", port := 80 )
{
    def req := web_build_request( "POST", host, path, port )
    web_set_payload( req, payload )
    web_request( req )
}


func web_server_build_reply( const req @=, code, payload @= "" )
{
    if( not is_defined req.socket ) {
        return false
    }
    def reply := _tuple_create()
    def reply.socket := req.socket
    def reply.code := code
    def reply.payload := payload

    def reply.header :=  _tuple_create()
    def reply.header.Server := "TeaScript/%(_version_major).%(_version_minor).%(_version_patch)"

    reply
}

)_SCRIPT_";

    Parser p;
#if !defined(NDEBUG)  //TODO: Do we want this block always enabled?
    p.SetDebug( rInto.is_debug );
    eOptimize opt_level = rInto.is_debug ? eOptimize::Debug : eOptimize::O0;
#else
    eOptimize opt_level = eOptimize::O1;
#endif

    p.ParsePartial( web_preview_code, "WebPreview" );
    auto ast = p.ParsePartialEnd();

    if( eval_only ) {
        ast->Eval( rInto );
    } else {
        StackVM::Compiler  compiler;
        auto program = compiler.Compile( ast, opt_level );
        StackVM::Machine<false>  machine;
        machine.Exec( program, rInto );
        machine.ThrowPossibleErrorException();
    }
}


ValueObject WebPreviewModule::HttpServerSetup( Context &rContext, ValueObject const &rParams )
{
    auto  const  cfg = ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() );
    Tuple res;
    try {
        if( rParams.InternalType() != ValueObject::TypeTuple ) {
            throw std::invalid_argument( "Must provide a Tuple structure with 'host' and 'port' as parameter!" );
        }
        Tuple const &param = rParams.GetValue<Tuple>();
        std::string const host = param.ContainsKey( "host" ) ? param.GetValueByKey( "host" ).GetAsString() : "0.0.0.0";
        unsigned short const port = param.ContainsKey( "port" ) ? static_cast<unsigned short>(param.GetValueByKey( "port" ).GetAsInteger()) : static_cast<unsigned short>(8080);

        auto const addr = boost::asio::ip::make_address( host );

        auto io_ptr = std::make_shared<boost::asio::io_context>( 1 );
        auto acceptor_ptr = std::make_shared<boost::asio::ip::tcp::acceptor>( *io_ptr, boost::asio::ip::tcp::endpoint{addr,port} );
        // reaching here, server is listening for new connections.

        // build result structure
        res.AppendKeyValue( "io_context", ValueObject::CreatePassthrough( std::move( io_ptr )  ) );
        res.AppendKeyValue( "acceptor", ValueObject::CreatePassthrough( std::move( acceptor_ptr ) ) );

        return ValueObject( std::move( res ), cfg );

    } catch( std::exception const &ex ) {
        res.AppendKeyValue( "error", ValueObject( true, ValueConfig( true ) ) );
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );

        return ValueObject( std::move( res ), cfg );
    }
}

ValueObject WebPreviewModule::HttpServerAcceptOne( Context &rContext, ValueObject const &rServer )
{
    auto  const  cfg = ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() );
    Tuple res;
    try {
        if( rServer.InternalType() != ValueObject::TypeTuple ) {
            throw std::invalid_argument( "Must provide a server Tuple structure as parameter obtained via web_server_setup()!" );
        }
        Tuple const &server = rServer.GetValue<Tuple>();
        if( not server.ContainsKey( "io_context" ) or not server.ContainsKey( "acceptor" ) ) {
            throw std::invalid_argument( "Must provide a server Tuple structure as parameter obtained via web_server_setup()!" );
        }

        auto ioc_ptr = std::any_cast<std::shared_ptr<boost::asio::io_context>>(server.GetValueByKey( "io_context" ).GetPassthroughData());
        auto acceptor_ptr = std::any_cast<std::shared_ptr<boost::asio::ip::tcp::acceptor>>(server.GetValueByKey( "acceptor" ).GetPassthroughData());

        // sanity check
        if( not ioc_ptr or not acceptor_ptr ) {
            throw std::invalid_argument( "Invalid server object!" );
        }

        boost::asio::ip::tcp::socket  socket( *ioc_ptr );

        acceptor_ptr->accept( socket );

        boost::beast::flat_buffer  buffer;
        boost::beast::http::request<boost::beast::http::string_body>  request;
        // receive the request.
        boost::beast::http::read( socket, buffer, request );

        res.AppendKeyValue( "method", ValueObject( std::string( request.method_string() ), ValueConfig( true ) ) );
        res.AppendKeyValue( "path", ValueObject( std::string( request.target() ), ValueConfig( true ) ) );

        Tuple  header;
        for( auto const &f : request ) {
            header.AppendKeyValue( f.name_string(), ValueObject( std::string( f.value() ), ValueConfig( true ) ) );
        }
        if( not header.IsEmpty() ) {
            res.AppendKeyValue( "header", ValueObject( std::move( header ), cfg ) );
        }

        std::string payload = request.body();
        if( not payload.empty() ) {
#if TEASCRIPT_JSONSUPPORT
            // check if payload is json and if so build a Tuple structure from it.
            if( auto field = request.find( "Content-Type" ); field != request.end() ) {
                if( field->value().starts_with( "application/json" ) ) {
                    auto json = JsonSupport<>::ReadJsonString( rContext, payload );
                    if( not json.GetTypeInfo()->IsSame<TypeInfo>() ) { // bool (true/false) and NaV (null) is a possible result, so we use a TypeInfo as error indicator.
                        res.AppendKeyValue( "json", json );
                    }
                }
            }
#endif
            // add original payload as string always.
            res.AppendKeyValue( "payload", ValueObject( std::move( payload ), ValueConfig( true ) ) );
        }

        res.AppendKeyValue( "socket", ValueObject::CreatePassthrough( std::make_shared< boost::asio::ip::tcp::socket>( std::move( socket ) ) ) );

        return ValueObject( std::move( res ), cfg );

    } catch( std::exception const &ex ) {
        res.AppendKeyValue( "error", ValueObject( true, ValueConfig( true ) ) );
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );

        return ValueObject( std::move( res ), cfg );
    }
}

ValueObject WebPreviewModule::HttpServerReply( Context &rContext, ValueObject const &rReply )
{
    auto  const  cfg = ValueConfig( ValueShared, ValueMutable, rContext.GetTypeSystem() );
    try {
        if( rReply.InternalType() != ValueObject::TypeTuple ) {
            throw std::invalid_argument( "Must provide a reply Tuple structure as parameter!" );
        }

        Tuple const &reply = rReply.GetValue<Tuple>();
        if( not reply.ContainsKey( "socket" ) or not reply.ContainsKey( "code" ) ) {
            throw std::invalid_argument( "Must provide a reply Tuple structure as parameter!" );
        }

        auto socket_ptr = std::any_cast<std::shared_ptr<boost::asio::ip::tcp::socket>>(reply.GetValueByKey( "socket" ).GetPassthroughData());
        if( not socket_ptr ) {
            throw std::invalid_argument( "Invalid socket ptr!" );
        }

        boost::beast::http::response<boost::beast::http::string_body> response{boost::beast::http::int_to_status( static_cast<int>(reply.GetValueByKey("code").GetAsInteger())), 11};

        // fill in all header values
        if( reply.ContainsKey( "header" ) ) {
            auto const &header_tup = reply.GetValueByKey( "header" ).GetValue<Tuple>();
            for( auto const &[k, v] : header_tup ) {
                response.set( k, v.GetAsString() );
            }
        }

        // handle (optional) payload
        if( reply.ContainsKey( "payload" ) ) {
            auto const &payload_val = reply.GetValueByKey( "payload" );
#if TEASCRIPT_JSONSUPPORT
            // if payload is a Tuple we interpret it as json
            if( payload_val.GetTypeInfo()->IsSame<Tuple>() ) {
                // Tuple -> Json string
                ValueObject json_str_val = JsonSupport<>::WriteJsonString( payload_val );
                if( not json_str_val.GetTypeInfo()->IsSame<std::string>() ) {
                    throw std::invalid_argument( "payload cannot be transformed into a compatible json string!" );
                }

                response.body() = json_str_val.GetValue<std::string>();
                response.set( boost::beast::http::field::content_type, "application/json" );
            } else
#endif
            {
                response.body() = payload_val.GetAsString();
            }

            // send as plain text if nothing is set yet.
            if( response.find( boost::beast::http::field::content_type ) == response.end() ) {
                response.set( boost::beast::http::field::content_type, "text/plain" );
            }

            // compute Content-Length
            response.prepare_payload();
        }

        boost::beast::http::write( *socket_ptr, response );

        boost::beast::error_code ec;
        socket_ptr->shutdown( boost::asio::ip::tcp::socket::shutdown_both, ec );
        // ignore error when shutdown...

        return ValueObject( true ); // success!

    } catch( std::exception const &ex ) {
        Tuple res;
        res.AppendKeyValue( "error", ValueObject( true, ValueConfig( true ) ) );
        res.AppendKeyValue( "what", ValueObject( std::string( ex.what() ), ValueConfig( true ) ) );

        return ValueObject( std::move( res ), cfg );
    }
}


} // namespace teascript
