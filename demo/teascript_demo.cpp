/*
 * SPDX-FileCopyrightText:  Copyright (c) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
 * SPDX-License-Identifier: SEE LICENSE IN LICENSE.txt
 *
 * Licensed under the TeaScript Library Standard License. See LICENSE.txt or you may find a copy at
 * https://tea-age.solutions/teascript/product-variants/
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

