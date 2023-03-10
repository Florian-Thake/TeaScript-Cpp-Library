/* === TeaScript Demo Program ===
 * SPDX-FileCopyrightText:  Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License 
 * as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>
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


// Why on earth MS decided to make compile errors for these C++ standard library functions? They are still available and don't have a successor/replacement!
#if defined _MSC_VER  && !defined _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
# define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#if defined _MSC_VER  && !defined _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
# define _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
#endif
#if defined _MSC_VER  && !defined _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif


// TeaScript includes
#include "teascript/version.h"
#include "teascript/Engine.hpp"


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
    (void)is_from_getline; // silence unused parameter warning.
    return std::string( arg );
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
    // For Bool exists different named methods because many types could be implicit converted to bool by accident.
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
    engine.ExecuteCode( "const some_var := \"Hello!\"\n"
                        "if( is_defined call_me and call_me is Function ) { // safety checks! \n"
                        "    call_me( )\n"
                        "}\n"                 
                      );
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
    auto const res = engine.ExecuteCode( "sum( 1234, 4321 )\n" );

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
    auto const res = engine.ExecuteCode( "getmagic()\n" );

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
    // a full blown engine would provide new methods for partial evaluation.
    inline teascript::Parser &GetParser() noexcept { return mParser; }
    inline teascript::Parser const &GetParser() const noexcept { return mParser; }
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
        engine.GetParser().ParsePartial( chunks[idx] );
        auto node = engine.GetParser().GetPartialParsedASTNodes();
        std::cout << "chunk " << (idx + 1) << " has " << node->ChildCount() << " node(s)." << std::endl;
        try {
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
// Passthroug data can only be stored in variables or tunneled through the
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

        // does the script end via any exit( code ) ?
        if( engine.HasExitCode() ) {
            std::cout << "script exit code: " << engine.GetExitCode() << std::endl;
            return static_cast<int>(engine.GetExitCode());
        } else if( res.HasPrintableValue() ) { // does it return a printable result?
            std::cout << "result: " << res.PrintValue() << std::endl;
        }
    } catch( teascript::exception::runtime_error const &ex ) {
        teascript::util::pretty_print( ex );
        return EXIT_FAILURE;
    } catch( std::exception const &ex ) {
        std::cout << "Exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


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
    } else if( argc >= 2 ) {
        return exec_script_file( args );
    } else {
        std::cout << "TeaScript demo app. Based on TeaScript C++ Library version: " << teascript::version::as_str() << std::endl;
        std::cout << teascript::copyright_info() << std::endl;
        std::cout << "\nUsage:\n"
                  << args[0] << " -<N>              --> execs test code N\n"
                  << args[0] << " filename [args]   --> execs TeaScript \"filename\" with \"args\"" << std::endl;
        std::cout << "\n\nContact: " << teascript::contact_info() << std::endl;
        std::cout << "The TeaScript Host Application for execute standalone TeaScript files\n"
                     "is available for free here: https://tea-age.solutions/downloads/ \n";
    }
    
    return EXIT_SUCCESS;
}

