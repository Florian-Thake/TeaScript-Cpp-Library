/* === TeaScript Demo Program ===
 * SPDX-FileCopyrightText:  Copyright (C) 2025 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */

// check for at least C++20
//for VS use /Zc:__cplusplus
#if __cplusplus < 202002L
# if defined _MSVC_LANG // fallback without /Zc:__cplusplus
#  if !_HAS_CXX20
#   error must use at least C++20
#  endif
# else
#  error must use at least C++20
# endif
#endif

// We opt-out for header only (must be in each TU (Translation Unit) where teascript includes are used, or project wide).
// NOTE: The default is header only, which does not require to do anything special.
#define TEASCRIPT_DISABLE_HEADER_ONLY       1
// this is the TU (Translation Unit) where our definitions will be included (there should be only one TU with this macro!)
// NOTE: If header only is disabled this must be done for Engine.hpp, CoroutineScriptEngine.hpp.
#define TEASCRIPT_INCLUDE_DEFINITIONS       1

// TeaScript includes
#include "teascript/version.h"
#include "teascript/Engine.hpp"
#include "teascript/CoroutineScriptEngine.hpp"

#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW
# include "teascript/JsonSupport.hpp" // we need Json for the web preview
// For the kind of simplicity the demo code always use the build-in default Json support, which is PicoJson.
// If your C++ project is using nlohmann::json, RapidJson or Boost.Json you can change the used JsonAdapter for TeaScript.
// But this demo code needs to be adjusted then as well because it uses the C++ Json objects from the library directly.
# if TEASCRIPT_JSONSUPPORT != TEASCRIPT_JSON_PICO
#  error You must use the default PicoJson Adapter for the web preview demo code.
# endif
#endif

// Std includes
#include <cstdlib> // EXIT_SUCCESS
#include <cstdio>
#include <iostream>
#include <vector>
#include <filesystem>
//#include <chrono>


//NOTE: for the case an old WinSDK is used...
// C:\Program Files( x86 )\Windows Kits\10\Include\10.0.19041.0\um\winbase.h( 9531, 5 ) : 
// warning C5105 : macro expansion producing 'defined' has undefined behavior( compiling source file source\TestTeaScriptCore_Main.cpp )
// from WinBase.h: #define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS (_WIN32_WINNT >= 0x0502 || !defined(_WINBASE_))
#if defined( _MSC_VER )
# include <sdkddkver.h> // WDK_NTDDI_VERSION
# if WDK_NTDDI_VERSION < 0x0A00000A  // NTDDI_WIN10_FE
#  pragma warning( push )
#  pragma warning( disable: 5105 )
# endif
#endif

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# define NOGDI
# include <Windows.h>  // only needed for the platform dependend string conversion (Windows code page)
#endif

#if defined( _MSC_VER )
# if WDK_NTDDI_VERSION < 0x0A00000A  // NTDDI_WIN10_FE
#  pragma warning( pop ) // for make compiler happy (warning)
# endif
#endif



// forward declaration, see reflectcpp_demo.cpp for implementation.
void teascript_reflectcpp_demo();

// forward declaration, see coroutine_demo.cpp for implementation.
void teascript_coroutine_demo();

// forward declaration, see suspend_thread_demo.cpp for implementation.
void teascript_thread_suspend_demo();


// web preview code (http client w. json). You must enable the WebPreview module in order to run this code.
// Follow the instructions in instructions.txt or read the relase blog post on https://tea-age.solutions/ 
// or watch out for a tutorial video on YouTube.
void webpreview_code()
{
#if TEASCRIPT_ENGINE_USE_WEB_PREVIEW

    // Imagine you have some Json C++ object and want to send it to a web server...
    // note: This works also with nlohmann::json, RapidJson and Boost.Json. To do so, you need to add the corresponding JsonAdapter from the extensions directory.
    picojson::value  json;
    std::ignore = picojson::parse( json, "{\"name\":\"John\", \"age\":31, \"lottery\":[9,17,22,35,37,41,48]}" );

    // we are building a little import helper engine for us wich will ease our life...
    class WebJsonEngine : public teascript::Engine
    {
    public:
        using teascript::Engine::Engine;  // same constructors as in base class.

        // imports the Json object (JsonType equals picojson::value if PicoJson is used) as a Tuple object with given name.
        void ImportJsonAsTuple( std::string const &name, teascript::JsonSupport<>::JsonType const &json )
        {
            teascript::ValueObject tuple;
            teascript::JsonSupport<>::JsonToValueObject( mContext, tuple, json );
            AddSharedValueObject( name, tuple );
        }
    };
    
    // create the engine
    WebJsonEngine  engine;
    
    // import the json as variable 'payload'
    engine.ImportJsonAsTuple( "payload", json );

    // execute a script which will issue a webserver POST and returns the (json) payload of the reply
    auto reply_payload = engine.ExecuteCode( R"_SCRIPT_(
def reply := web_post( "postman-echo.com", payload, "/post" )
if( is_defined reply.json ) { // we got a json object back from the server
    reply.json
} else {
    if( is_defined reply.error ) {
        reply.what
    } else {
        "Unknown error! No Json object present!"
    }
}
)_SCRIPT_" );
    if( reply_payload.GetTypeInfo()->IsSame< std::string >() ) { // error
        std::cout << "Error: " << reply_payload.GetValue< std::string>() << std::endl;
        return;
    }

    // here we have a Tuple build from the Json reply from the server.
    // Lets make a C++ Json from it...
    picojson::value  reply_json;
    teascript::JsonSupport<>::JsonFromValueObject( reply_payload, reply_json );

    if( reply_json.is<picojson::object>() ) {
        // we just iterate through the top layer for this demo..
        std::cout << "Json entries: " << std::endl;
        for( auto const &[key, value] : reply_json.get<picojson::object>() ) {
            std::cout << key << ": " << std::endl;
            std::cout << value << "\n\n";
        }
    } else {
        // just print it.
        std::cout << reply_json << std::endl;
    }

#else
    puts( "Web Preview module is disabled!\nFollow the instructions in instructions.txt to enable it\n"
          "or read the release blog post on https://tea-age.solutions/ \nor watch the how-to video on YouTube: https://youtu.be/SeRO21U1vMk" );
#endif
}


// This test code will add some variables (mutable and const) to the script context
// and then execute script code, which will use them.
void test_code1()
{
    // create the TeaScript default engine.
    teascript::Engine  engine;

    // add 2 integer variables a and b.
    engine.AddVar( "a", 2 );   // variable a is mutable with value 2
    engine.AddVar( "b", 3 );   // variable b is mutable with value 3
    engine.AddConst( "hello", "Hello, World!" );    // variable hello is a const string.
    // For Bool exists different named functions because many types could be implicit converted to bool by accident.
    // like char * for example.
    engine.AddBoolVar( "speak", true );   // variable speak is a Bool with value true.
    

    // execute the script code passed as string. it computes new variable c based on values a and b.
    // also it will print the content of the hello variable.
    // finnally it returns the value of variable c.
    auto const res = engine.ExecuteCode(    "const c := a + b\n"
                                            "if( speak ) {\n"
                                            "    println( hello )\n"
                                            "}\n"
                                            "c\n" 
                                        );
    // print the result.
    std::cout << "c is " << res.GetAsInteger() << std::endl;
}


// this is our simple callback function which we will call from TeaScript code.
// The callback function signature is always ValueObject (*)( Context & )
teascript::ValueObject user_callback( teascript::Context &rContext )
{
    // look up variable 'some_var'
    auto const val = rContext.FindValueObject( "some_var" );

    // print a message and the value of 'some_var'
    std::cout << "Hello from user_callback! some_var = " << val.PrintValue() << std::endl;

    // return 'nothing' aka NaV (Not A Value).
    return {};
}

// This test code will register a C++ callback function and then executes a script
// which will call it.
void test_code2()
{
    // create the TeaScript default engine.
    teascript::Engine  engine;

    // register a function as a callback. can use arbitrary names.
    engine.RegisterUserCallback( "call_me", user_callback );

    // execute the script which will create variable 'some_var' and then call our callback function.
    engine.ExecuteCode( R"_SCRIPT_(
const some_var := "Hello!"
if( is_defined call_me and call_me is Function ) { // safety checks!
    call_me( )
}
)_SCRIPT_" );
}


// This is another callback function.
// It will create the sum of 2 passed parameters and return the result to the script.
teascript::ValueObject calc_sum( teascript::Context &rContext )
{
    if( rContext.CurrentParamCount() != 2 ) { // this check could be relaxed also...
        // in case of wrong parameter count, throw eval error with current source code location.
        throw teascript::exception::eval_error( rContext.GetCurrentSourceLocation(), "Calling calc_sum: Wrong amount of parameters! Expecting 2.");
    }

    // get the 2 operands for the calculation.
    auto const lhs = rContext.ConsumeParam();
    auto const rhs = rContext.ConsumeParam();

    // calculate the sum and return result as ValueObject.
    return teascript::ValueObject( lhs.GetAsInteger() + rhs.GetAsInteger() );
}

// This test code will register a callback function and call it from a script with parameters.
void test_code3()
{
    // create the TeaScript default engine.
    teascript::Engine  engine;

    // register a function as a callback. can use arbitrary names.
    engine.RegisterUserCallback( "sum", calc_sum );

    // execute the script which will call our function
    auto const res = engine.ExecuteCode( "sum( 1234, 4321 )" );

    // print the result.
    std::cout << "res is " << res.GetAsInteger() << std::endl;
}


// This test code demonstrates how arbitrary user data can be transferred into function callbacks.
void test_code4()
{
    // assume this is our important application/business context...
    struct MyUserContext
    {
        unsigned int magic_number = 0xcafecafe;
    } mycontext;

    // a capturing lambda is one way, using std::bind another....
    auto lambda = [&]( teascript::Context & /*rContext*/ ) -> teascript::ValueObject {
        // here we have access to all captured variables. So, we can use our context and have access to all members.
        return teascript::ValueObject( static_cast<teascript::Integer>( mycontext.magic_number ) );
    };

    // create the TeaScript default engine.
    teascript::Engine  engine;

    // register the lambda as callback. can use arbitrary names.
    engine.RegisterUserCallback( "getmagic", lambda );

    // execute the script which will call our function
    auto const res = engine.ExecuteCode( "getmagic()" );

    // print the result.
    std::cout << "res is " << std::hex << res.GetAsInteger() << std::endl;
}


// The next test code demonstrates partial evaluation, which is a new feature since TeaScript 0.10
// In most of the use cases partial evaluation is not needed. But if you have a use case for it, here is one example to do it.
// For the time being partial evaluation is only available via the low level interface.
// One way to use it, is to derive from the default TeaScript Engine.
class PartialEvalEngine : public teascript::Engine
{
public:
    // we keep this example simple and just make the Parser and Context public available.
    // a full blown engine would provide new functions for partial evaluation.
    inline teascript::Parser &GetParser() noexcept { return mBuildTools->mParser; }
    inline teascript::Parser const &GetParser() const noexcept { return mBuildTools->mParser; }
    inline teascript::Context &GetContext() noexcept { return mContext; }
    inline teascript::Context const &GetContext() const noexcept { return mContext; }

    // In this example we always configure the Core Library to load only the core parts and without stdio
    // This feature was added in TeaScript 0.10 as well.
    PartialEvalEngine()
        : Engine( teascript::config::no_stdio( teascript::config::core() ) )
    {
    }

};

// now in the test code we define some code chunks and execute it chunk by chunk.
void test_code5()
{
    std::vector<std::string> chunks;
    // part 1 is simple, b/c it consists of complete top level statements only.
    chunks.emplace_back( R"_SCRIPT_(
def a := 1
def b := 3
)_SCRIPT_" );

    // part 2 ends with a not closed multi line comment...
    chunks.emplace_back( R"_SCRIPT_(
def c := 6
/* some comemnt
over several
)_SCRIPT_" );

    // part 3 adds one more statement NOTE: The Parser cannot know if an else will follow. So, the if is not complete yet!
    chunks.emplace_back( R"_SCRIPT_(
lines */ 
def d := 9
if( a + b > 6 ) { 
    d - c
}
)_SCRIPT_" );

    // part 4 is the final part
    chunks.emplace_back( R"_SCRIPT_(
/* just some 
 comment */
else {
    a + b + c + d // 19
}
)_SCRIPT_" );

    PartialEvalEngine  engine;
    teascript::ValueObject res;
    for( size_t idx = 0; idx < chunks.size(); ++idx ) {
        try {
            engine.GetParser().ParsePartial( chunks[idx] );
            auto node = engine.GetParser().GetPartialParsedASTNodes();

            std::cout << "chunk " << (idx + 1) << " has " << node->ChildCount() << " node(s)." << std::endl;

            res = node->Eval( engine.GetContext() );
            if( idx == chunks.size() - 1 ) { // the last chunk, check for left overs
                node = engine.GetParser().GetFinalPartialParsedASTNodes();
                if( node->HasChildren() ) {
                    res = node->Eval( engine.GetContext() );
                }
            }
        } catch( teascript::control::Exit_Script const & ) {
            std::cout << "script exited." << std::endl;
            return;
        } catch( teascript::control::Return_From_Function const & ) {
            std::cout << "script returned from main early." << std::endl;
            return;
        } catch( teascript::exception::runtime_error const &ex ) {
            teascript::util::pretty_print( ex );
            return;
        } catch( std::exception const &ex ) {
            std::cout << "Exception: " << ex.what() << std::endl;
            return;
        }
    }

    std::cout << "res is " << res.GetAsInteger() << std::endl;
}


// The next example will show the use of arbitrary pass through data.
// Passthrough data can only be stored in variables or tunneled through the
// TeaScript layer for use in callback functions / retrieved later. But it
// can be copied / passed around freely like all other variables.

// This is our user defined data struct which we will pass trough. It does not have further meaning, just for illustrating.
struct BusinessData
{
    long long  key;     // arbitrary data member
    long long  secret;  // arbitrary data member
};

// the user defined result type of our data processing, just for illustrating.
struct BusinessOutcome
{
    std::string content;    // arbitrary data member
    long long magic;        // arbitrary data member
};

// A user defined type for the data processing and context. This time we will also pass it via pass through technique and not bind it via a user callback.
class BusinessProcessor
{
    int seed; // some state.
public:
    BusinessProcessor( int s ) : seed(s) {}

    // processes the data to the outcome.
    BusinessOutcome Process( BusinessData const &rData )
    {
        // just some completely nonsense abritrary processing...
        return BusinessOutcome{std::string( "The final content: " ) + std::to_string( rData.secret + (rData.key-seed) ), rData.key * rData.secret + seed};
    }
};

// our callback function for generate business data as passthrough type.
teascript::ValueObject create_data( teascript::Context &rContext )
{
    if( rContext.CurrentParamCount() != 2 ) { // this check could be relaxed also...
        // in case of wrong parameter count, throw eval error with current source code location.
        throw teascript::exception::eval_error( rContext.GetCurrentSourceLocation(), "Calling process: Wrong amount of parameters! Expecting 2." );
    }
    // get the 2 parameters
    auto const key_val = rContext.ConsumeParam();
    auto const secret_val = rContext.ConsumeParam();

    // create the concrete business data ...
    BusinessData data{key_val.GetAsInteger(), secret_val.GetAsInteger()};
    // ... and return it as pass through ValueObject.
    return teascript::ValueObject::CreatePassthrough( data );
}


// our callback function for processing
teascript::ValueObject process( teascript::Context &rContext )
{
    if( rContext.CurrentParamCount() != 2 ) { // this check could be relaxed also...
        // in case of wrong parameter count, throw eval error with current source code location.
        throw teascript::exception::eval_error( rContext.GetCurrentSourceLocation(), "Calling process: Wrong amount of parameters! Expecting 2." );
    }

    // get the 2 parameters
    auto proc_val = rContext.ConsumeParam();
    auto const data_val = rContext.ConsumeParam();

    // get our processor (note: consider to use a shared_ptr or refference_wrapper as an alternative)
    auto &processor = *std::any_cast<BusinessProcessor *>(proc_val.GetPassthroughData());
    // get the business data for processing.
    auto &data = std::any_cast<BusinessData const &>(data_val.GetPassthroughData());
    // invoke data proecssing.
    auto res = processor.Process( data );

    // return the outcome as pass through ValueObject.
    return teascript::ValueObject::CreatePassthrough( res );
}

// This test code will register the user callback functions, add the BusinessProcessor as passthrough data
// and invokes some data processing with the user defined types. Finnaly it will retrieve the user defined result data.
void test_code6()
{
    // The TeasScript default engine. This time we configure it to not load any file io and std io functions and use only the util level.
    teascript::Engine  engine{teascript::config::no_fileio( teascript::config::no_stdio( teascript::config::util() ) )};

    // register the create_data function as callback. can use arbitrary names.
    engine.RegisterUserCallback( "create_data", create_data );

    // register the process function as callback. can use arbitrary names.
    engine.RegisterUserCallback( "process", process );

    // our business data processor
    BusinessProcessor my_processor( 42 );

    // add it as passthrough data. can use arbitrary names.
    engine.AddPassthroughData( "the_processor", &my_processor );

    // execute some script using our functions and pass through types
    engine.ExecuteCode( R"_SCRIPT_(
def some_data := create_data( 11, 899 ) // just some business data.
const copy    := some_data // can be copied around
def outcome1  := process( the_processor, copy ) // make some processing

// can build some C-like struct
def proc := _tuple_create()     // empty tuple
def proc.handle := the_processor
def proc.call   := process
def proc.data   := create_data( 33, 777 )

// invoke it like this:
const outcome2 := proc.call( proc.handle, proc.data )

// a final one more processing
def result := process( the_processor, create_data( 97, 500 ) )
)_SCRIPT_" );

    // retrieve the outcomes
    std::cout << "outcome1: " << engine.GetPassthroughData<BusinessOutcome const &>("outcome1").content << std::endl;
    std::cout << "outcome2: " << engine.GetPassthroughData<BusinessOutcome const &>("outcome2").content << std::endl;
    std::cout << "result:   " << engine.GetPassthroughData<BusinessOutcome const &>("result").content << std::endl;
}


// This testcode demonstrates the (optional) integrated TOML Support.
// In order to activate TOML Support you need the toml++ library from here: https://github.com/marzer/tomlplusplus
// Then just add the toml++ include folder to the include pathes of this demo. See teascript/TomlSupport.hpp for some more details.
// --
// The test code reads a TOML formatted string direct into a Named Tuple structure of TeaScript.
// Also, it shows the raw string literal support as well as the new subscript operators for ValueObjects.
void test_code7()
{
#if !TEASCRIPT_TOMLSUPPORT
    std::cout << "TOML Support is deactivated. Please ensure toml++ is added to the include pathes.\nSee teascript/TomlSupport.hpp for more details." << std::endl;
#else
    // The TeasScript default engine. This time we configure it to not load any eval functions and use only the util level.
    // The util level contains the Toml Support already.
    teascript::Engine  engine{teascript::config::no_eval( teascript::config::util() ) };

    // add a TOML formatted string (usually read it from a file)
    engine.AddConst( "content", R"_TOML_(
[[people]]
first_name = "Bruce"
last_name = "Springsteen"

[[people]]
first_name = "Eric"
last_name = "Clapton"

[[people]]
first_name = "Bob"
last_name = "Seger"
)_TOML_" );

    // read the TOML string into a Tuple via readtomlstring function (NOTE: use readtomlfile function for read it from a file instead.)
    engine.ExecuteCode( R"_SCRIPT_(
const dict := readtomlstring( content )
)_SCRIPT_" );

    // access the result via script
    std::cout << "second entry first name: " << engine.ExecuteCode( "dict.people[1].first_name" ).GetValue<std::string>() << std::endl; // Eric
    std::cout << "third entry last name: " << engine.ExecuteCode( "dict.people[2].last_name" ).GetValue<std::string>() << std::endl; // Seger

    
    // now use a second TOML formatted string directly in TeaScript via a raw string literal.
    engine.ExecuteCode( R"_SCRIPT_(
const stock := readtomlstring( """
[[products]]
name = "Hammer"
sku = 738594937

[[products]]
name = "Nail"
sku = 284758393

color = "gray"
""" )
)_SCRIPT_" );


    // we only want the 'toml array' products.
    auto const  products = engine.ExecuteCode( "stock.products" );

    // access the result via C++ interface.
    std::cout << "name of first entry: " << products[0]["name"].GetValue<std::string>() << std::endl; // Hammer
    std::cout << "sku of second entry: " << products[1]["sku"].GetValue<teascript::Integer>() << std::endl; // 284758393
    std::cout << "color of second entry: " << products[1]["color"].GetValue<std::string>() << std::endl; // gray
#endif
}


// overloaded idiom for variant visitor. inherit from each 'functor/lambda' so that the derived class contains an operator( T ) for each type which shall be dispatchable.
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
#if defined( __clang__ ) // clang needs a little help...
template<class... Ts> overloaded( Ts... ) -> overloaded<Ts...>;
#endif


/// This testcode demonstrates the obtaining of a script result via a visitor by using also the well known overloaded idiom.
/// The TeaScript code creates a tuple with random amount of elements whereby each element is created by functor from 
/// a set of given functors. Each functor creates a different type. At the end one random element will be choosen
/// as the script result. Thus, it is unknown at parsing/compile time which type will be returned by the script.
/// A visitor can help to dispatch the result.
/// NOTE: The TeaScript part also nicely shows the usage of the forall loop as well as usage of functors.
void test_code8()
{
    // create the TeaScript default engine.
    teascript::Engine  engine;

    auto res = engine.ExecuteCode( R"_SCRIPT_(
def functors   := _tuple_create()     // container for the functors
def functors.0 := func ( n ) { n }    // first functor just returns what it gets....
def functors.1 := func ( n ) { if( n > 0 ) { n + 1.0/n } else { 0.0 } }    // second functor creates a Decimal...
def functors.2 := func ( n ) { n % "" }    // third functor creates a string...
def functors.3 := func ( n ) { (n, n*n) }  // fourth functor creates a tuple...

const VARIANTS := _tuple_size( functors )
const NUM      := random( 10,100 )    // choose randomly the number of result elements.
def   tup      := _tuple_create()     // empty tuple for store the result elements.

// iterate from 1 to NUM (inclusive) with step 1
forall( n in _seq( 1, NUM, 1 ) ) {
    // create a new result element by invoking the functor for the current iteration
    _tuple_append( tup, functors[ n mod VARIANTS ]( n ) )
}

// now finally select one element as return value....

return tup[ random( 0, NUM-1) ]

)_SCRIPT_" );

    std::cout << "the result is:" << std::endl;

    // NOTE: Visit can also return a value if the lambdas return one (must be always the same type).
    res.Visit( overloaded{
            []( auto ) -> void { std::cout << "<unhandled type>" << std::endl; },
            []( std::any const & ) { std::cout << "<unhandled type>" << std::endl; },
            []( teascript::NotAValue ) { std::cout << "<not a value>" << std::endl; },
            []( teascript::Bool ) { std::cout << "<Bool>" << std::endl;; },
            []( teascript::Integer const i ) { std::cout << "Integer: " << i << std::endl; },
            []( teascript::Decimal const d ) { std::cout << "Decimal: " << d << std::endl; },
            []( teascript::String const &rStr ) { std::cout << "String: " << rStr << std::endl; },
            []( teascript::Tuple const &rTuple ) { std::cout << "Tuple: (" << rTuple.GetValueByIdx( 0 ) << ", " << rTuple.GetValueByIdx( 1 ) << ")" << std::endl; } 
         }
     );
}

// This testcode demonstrates the usage of the new Buffer type (aka std::vector<unsigned char>) in TeaScript (since version 0.13).
// It shows the great bidirectional usage of the same data (and type) in C++ and TeaScript.
// Also, you can see a little bit of the new possibilities for integer (literals), e.g., suffix, hex, cast.
// If libfmt include is found you can see a runtime switch using the format() function to print a number as hex.
void test_code9()
{
    // our desired buffer size (in bytes)
    constexpr size_t  size = 128ULL;

    // TeaScript will manage the lieftime of the Buffer (takes ownership).
    // If we need more control on C++ level, we can just use a ValueObject as a "shared pointer variant" (the 'ValueShared' is important for it!)
    auto  managed_value = teascript::ValueObject( teascript::Buffer( size ), teascript::ValueConfig{teascript::ValueShared,teascript::ValueMutable} );
    //auto const managed_value = teascript::ValueObject( teascript::Buffer( 20ULL ), true ); // short cut of the above...

    // now the buffer lives until the last value object instance with the same shared value goes out of scope (actually there is only ours above.)
    
    // get a reference to the buffer for a more handy usage... :-)
    teascript::Buffer &buffer = managed_value.GetValue<teascript::Buffer>();
    // alternative:
    //std::vector<unsigned char> &buffer = managed_value.GetValue<std::vector<unsigned char>>();

    // now imagine we have some proprietary binary header/struct we need to store in the buffer for interchange and/or binary storage.
    // lets assume there is one magic number as 32 bit unsigned, followed by 2 16 bit (signed!) values for version major / minor, 
    // followed by a 64 bit unsigned length information, followed by a string (without trailing 0) of the previous mentioned length.
    
    std::uint32_t  const magic = 0x1337cafe;
    std::int16_t   const major = 2;
    std::int16_t   const minor = 11;
    std::string    const content = "Some text message is included here.";
    std::uint64_t  const len = static_cast<std::uint64_t>(content.length()); // ensure it is always 64 bit.

    // store the data in our propietary buffer format... 
    // NOTE: for simplicity we always use the host byte order! also, we assume buffer is always big enough (ok for demo mode, not for productive code!)
    ::memcpy( buffer.data(), &magic, sizeof( magic ) );
    ::memcpy( buffer.data() + 4, &major, sizeof( major ) );
    ::memcpy( buffer.data() + 6, &minor, sizeof( minor ) );
    ::memcpy( buffer.data() + 8, &len, sizeof( len ) );
    ::memcpy( buffer.data() + 16, content.data(), len );

    // now everything is prepared, lets use the buffer in TeaScript.

    // create the TeaScript default engine.
    teascript::Engine  engine;

    // add the buffer as variable 'buffer' (can be an arbitrary name).
    engine.AddSharedValueObject( "buffer", managed_value );

    // ok, doing some scripting stuff... and use the buffer inside a script.
    // NOTE: Usually scripts are loaded from file, e.g., use ExecuteScript( std::filesystem::path("") )
    //       For nice editing of scriptcode on Windows you can use Notepad++ with TeaScript SyntaxHighlighting from the repo.
    auto res = engine.ExecuteCode( R"_SCRIPT_(
const magic := _buf_get_u32( buffer, 0 )
if( magic != 0x1337cafe ) {
    fail_with_message( "magic number is wrong: %(magic)!" )
} else {
    if( features.format ) { // need libfmt support!
        print( format( "magic: {:#x}\n", magic ) )
    } else {
        print( "magic (dec): %(magic)\n" )
    }
}
const major := _buf_get_i16( buffer, 4 )
const minor := _buf_get_i16( buffer, 6 )
if( major < 2 or (major == 2 and minor < 11) ) {
    fail_with_message( "version too old: %(major).%(minor)!" )
}

const len := _buf_get_u64( buffer, 8 )

const str := _buf_get_string( buffer, 16, len )

println( "The message is: %(str)" )

// change sth., just for demonstration
const newstr := "This is the newest and greatest message ever!"

def ok := true
ok := ok and _buf_set_u64( buffer, 8, _strlen(newstr) as u64 )
ok := ok and _buf_set_string( buffer, 16, newstr )
ok := ok and _buf_set_u32( buffer, 16 + _strlen(newstr), 0xFEEDC0DEu64 ) // we only have u8 and u64, so we must use u64 here!

)_SCRIPT_" );

    // lets check what is the actual content in the buffer.
    if( res.GetTypeInfo()->IsSame<bool>() && res.GetValue<bool>() ) {
        // we can also use the CoreLibrary functions to operate on the buffer.
        auto const new_len_val = teascript::CoreLibrary::BufGetU64( buffer, teascript::ValueObject( 8LL ) );
        auto const new_content = teascript::CoreLibrary::BufGetString( buffer, teascript::ValueObject( 16LL ), new_len_val );
        std::cout << "New string content: " << new_content << std::endl;
        //auto const some_secret = teascript::CoreLibrary::BufGetU32( buffer, teascript::ValueObject( 16ULL + new_len_val.GetValue<unsigned long long>() ) );
        // or call TeaScript functions with the help of the engine (since 0.14.0)
        auto const some_secret = engine.CallFuncEx( "_buf_get_u32", buffer, 16ULL + new_len_val.GetValue<unsigned long long>() );
        std::cout << "a secret code is present: " << std::hex << some_secret.GetAsInteger() << std::endl;
    }
}


// This testcode demonstrate how to use explicitly the compile and program execution feature (added in vesion 0.14) with the default Engine.
// NOTE: The ExecuteCode/ExecuteScript() functions will compile the code automatically under the hood (default).
//       An alternative AST evaluation mode (which was always used in older versions < 0.14) can be activated during construction of the Engine.
void test_code10()
{
    // create the TeaScript default engine.
    teascript::Engine  engine;

    // from example file "gcd.tea"
    constexpr char gcd_tea[] = R"_SCRIPT_(
// computes the gcd with a loop
def x1 := if( is_defined arg1 ) { +arg1 } else { 1 }
def x2 := if( is_defined arg2 ) { +arg2 } else { 1 }
def gcd := repeat {
    if( x1 == x2 ) {
        stop with x1
    } else if( x1 > x2 ) {
        x1 := x1 - x2
    } else /* x2 > x1 */ {
        x2 := x2 - x1
    }
}
)_SCRIPT_";
    
    // explicit compile the code to a program.
    auto const program = engine.CompileCode( gcd_tea, teascript::eOptimize::O1 );

    // NOTE: You can also directly compile a script file with:
    // auto const program = engine.CompileScript( std::filesystem::path("path/to/file.tea"), teascript::eOptimize::O1 );

    // Now you could save the TeaScript binary to disk via
#if 0
    program->Save( std::filesystem::path( "./file.tsb" ) );
#endif
    // load is possible with
#if 0
    auto const prg2 = teascript::StackVM::Program::Load( std::filesystem::path( "./file.tsb" ) );
#endif

    // our script wants 2 variables from which the gcd is computed.
    engine.AddVar( "arg1", 42LL );
    engine.AddVar( "arg2", 18LL );
    // NOTE: an alternative way would be to pass a std::vector<teascript::ValueObject> to ExecuteProgram().
    
    // execute the compiled program
    auto const res = engine.ExecuteProgram( program );
    if( res.HasPrintableValue() ) { // does it return a printable result?
        std::cout << "the gcd is: " << res.PrintValue() << std::endl;
    }
}


// Executes a TeaScript file and returns its result. 
// This function has a very basic feature set. The TeaScript Host Application has more capabilities.
// Beside other features it also comes with an interactive shell, REPL, debug options and time measurement.
// The TeaScript Host Application can be downloaded for free here: https://tea-age.solutions/downloads/
int exec_script_file( std::vector<std::string> &args )
{
    args.erase( args.begin() );  // we don't need the program name
    std::string const filename = args.front(); // copy script filename
    args.erase( args.begin() );  // ... and remove it. the script args are left over (if any)

    std::filesystem::path const script( std::filesystem::absolute( teascript::util::utf8_path( filename ) ) );

    // create the TeaScript default engine.
    teascript::Engine  engine;

    try {
        // and execute the script file.
        auto const res = engine.ExecuteScript( script, args );

        if( res.HasPrintableValue() ) { // does it return a printable result?
            std::cout << "result: " << res.PrintValue() << std::endl;
        }
    } catch( teascript::exception::runtime_error const &ex ) {
#if TEASCRIPT_FMTFORMAT
        teascript::util::pretty_print_colored( ex );
#else
        teascript::util::pretty_print( ex );
#endif
        return EXIT_FAILURE;
    } catch( std::exception const &ex ) {
        std::cout << "Exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


// forward declaration, see below...
std::string build_string_from_commandline( char *arg, bool is_from_getline );



int main( int argc, char **argv )
{
    std::vector<std::string> args;
    try {
        args.reserve( static_cast<size_t>(argc) );
        args.emplace_back( argv[0] );
        for( int i = 1; i < argc; ++i ) {
            args.emplace_back( build_string_from_commandline( argv[i], false ) );
        }
    } catch( std::exception const &ex ) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    // check for correct console mode for be able to do colorized output (old conhost.exe needs this)
#if defined(_WIN32) && TEASCRIPT_FMTFORMAT
    DWORD mode = 0;
    if( not ::GetConsoleMode( ::GetStdHandle( STD_OUTPUT_HANDLE ), &mode ) ) {
        std::cerr << "Warning: Console mode could not be detected. Colorized output might produce garbage on screen." << std::endl;
    } else if( not (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ) {
        if( not ::SetConsoleMode( ::GetStdHandle( STD_OUTPUT_HANDLE ), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT ) ) {
            std::cerr << "\nERROR! Colorized output could not be activated for your console!\n"
                      << "Colorized output may produce garbage on the screen.\n"
                      << "Please, use a modern Console Host (e.g., Windows Terminal).\n" << std::endl;
        }
    }
#endif


    if( argc == 2 && args[1] == "-1" ) {
        test_code1();
    } else if( argc == 2 && args[1] == "-2" ) {
        test_code2();
    } else if( argc == 2 && args[1] == "-3" ) {
        test_code3();
    } else if( argc == 2 && args[1] == "-4" ) {
        test_code4();
    } else if( argc == 2 && args[1] == "-5" ) {
        test_code5();
    } else if( argc == 2 && args[1] == "-6" ) {
        test_code6();
    } else if( argc == 2 && args[1] == "-7" ) {
        test_code7();
    } else if( argc == 2 && args[1] == "-8" ) {
        test_code8();
    } else if( argc == 2 && args[1] == "-9" ) {
        test_code9();
    } else if( argc == 2 && args[1] == "-10" ) {
        test_code10();
    } else if( argc == 2 && args[1] == "suspend" ) {
        teascript_thread_suspend_demo();
    } else if( argc == 2 && args[1] == "coro" ) {
        teascript_coroutine_demo();
    } else if( argc == 2 && args[1] == "web" ) {
        webpreview_code();
    } else if( argc == 2 && args[1] == "reflect" ) {
        teascript_reflectcpp_demo();
    } else if( argc >= 2 ) {
        return exec_script_file( args );
    } else {
        std::cout << "TeaScript demo app. Based on TeaScript C++ Library version: " << teascript::version::as_str() << std::endl;
        std::cout << teascript::copyright_info() << std::endl;
        std::cout << "\nUsage:\n"
                  << args[0] << " -<N>              --> execs test code N\n"
                  << args[0] << " web               --> execs web preview\n"
                  << args[0] << " coro              --> execs coroutine demo\n"
                  << args[0] << " suspend           --> execs thread suspend demo\n"
                  << args[0] << " reflect           --> execs thread reflectcpp demo\n"
                  << args[0] << " filename [args]   --> execs TeaScript \"filename\" with \"args\"" << std::endl;
        std::cout << "\n\nContact: " << teascript::contact_info() << std::endl;
        std::cout << "The TeaScript Host Application for execute standalone TeaScript files\n"
                     "is available for free here: https://tea-age.solutions/downloads/ \n";
    }
    
    return EXIT_SUCCESS;
}



// helper function for Windows to convert a string from ANSI/OEM code page to UTF-8 encoded string. Does nothing on Non-Windows platforms.
// NOTE: This function does only half of the job. It makes the full set of the current ANSI/OEM code page available as UTF-8 instead of ASCII only.
//       For full unicode support different techniques are required. These are used in the TeaScript Host Application.
//       The TeaScript Host Application can be downloaded for free here: https://tea-age.solutions/downloads/
std::string build_string_from_commandline( char *arg, bool is_from_getline )
{
#if defined( _WIN32 )
    // first convert from ANSI/OEM code page to UTF-16, then from UTF-16 to UTF-8
    // interestingly only CP_ACP converts correctly when the string comes from argv, 
    // but if from std::getline (console API) must use CP_OEMCP.
    UINT const  codepage = is_from_getline ? CP_OEMCP : CP_ACP;
    int const  len = static_cast<int>(strlen( arg ));
    int res = MultiByteToWideChar( codepage, 0, arg, len, 0x0, 0 );
    if( res <= 0 ) {
        throw std::invalid_argument( "Cannot convert parameter to UTF-8. Please check proper encoding!" );
    }
    std::wstring utf16;
    utf16.resize( static_cast<size_t>(res) );
    res = MultiByteToWideChar( codepage, 0, arg, len, utf16.data(), (int)utf16.size() );
    if( res <= 0 ) {
        throw std::invalid_argument( "Cannot convert parameter to UTF-8. Please check proper encoding!" );
    }

    // now convert to UTF-8
    res = WideCharToMultiByte( CP_UTF8, 0, utf16.data(), (int)utf16.size(), 0x0, 0, 0x0, 0x0 );
    if( res <= 0 ) {
        throw std::invalid_argument( "Cannot convert parameter to UTF-8. Please check proper encoding!" );
    }
    std::string  utf8;
    utf8.resize( static_cast<size_t>(res) );
    res = WideCharToMultiByte( CP_UTF8, 0, utf16.data(), (int)utf16.size(), utf8.data(), (int)utf8.size(), 0x0, 0x0 );
    if( res <= 0 ) {
        throw std::invalid_argument( "Cannot convert parameter to UTF-8. Please check proper encoding!" );
    }
    return utf8;
#else
    (void)teascript::util::debug_print_currentline; // silence unused function warning.
# if TEASCRIPT_FMTFORMAT
    ( void )teascript::util::debug_print_currentline_colored; // silence unused function warning.
# endif
    (void)is_from_getline; // silence unused parameter warning.
    return std::string( arg );
#endif
}

