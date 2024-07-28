/* === TeaScript Demo Program ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */


// The following demo code demonstrates the usage of class CoroutineScriptEngine.
// It makes it very simple and handy to execute TeaScript code similar like a coroutine,
// collect its yielded values, use it for background tasks or implement own co-operative multi tasking.


// We opt-out for header only (must be in each TU (Translation Unit) where teascript includes are used, or project wide).
// NOTE: The default is header only, which does not require to do anything special.
#define TEASCRIPT_DISABLE_HEADER_ONLY       1

#include "teascript/CoroutineScriptEngine.hpp"
#include "teascript/ContextFactory.hpp"

#include <future>


namespace {

// calculate factorial starting at 1!
constexpr char factorial_code[] = R"_SCRIPT_(
def fac := 1
def n   := 2
repeat {
    yield fac
    fac := fac * n
    n   := n + 1
}
)_SCRIPT_";


// prints all given input parameters one by one, then finishs.
constexpr char print_input_code[] = R"_SCRIPT_(
if( not is_defined args ) {
    println( "<No arguments>" )
    return void
}
forall( idx in args ) {
    println( args[idx] )
    if( idx < argN - 1 ) {
        suspend
    }
}
)_SCRIPT_";

// calculates fibonnacci recursively (that it takes more time)
constexpr char fibonacci_code[] = R"_SCRIPT_(
func fib( x )
{
    if( x == 1 or x == 0 ) {
        x
    } else {
        fib( x - 1 ) + fib( x - 2 )
    }
}

yield fib( args[0] )   // in this particular case we could also use 'return' or implicit return....

)_SCRIPT_";

} // anonymous


void teascript_coroutine_demo()
{
    try {
        // setup the Coroutine engine with the factorial calculation coroutine.
        teascript::CoroutineScriptEngine  coro_engine( teascript::CoroutineScriptEngine::Build( factorial_code, teascript::eOptimize::O1, "factorial" ) );

        // lets calculate some values and print them...
        std::cout << "next factorial number: " << coro_engine() << std::endl;
        std::cout << "next factorial number: " << coro_engine() << std::endl;
        std::cout << "next factorial number: " << coro_engine() << std::endl;
        std::cout << "next factorial number: " << coro_engine() << std::endl;
        std::cout << "next factorial number: " << coro_engine() << std::endl;

        // change coroutine to a new one: print all parameters one by one.
        coro_engine.ChangeCoroutine( teascript::CoroutineScriptEngine::Build( print_input_code, teascript::eOptimize::O1, "print_input" ) );
        // set some parameters for the coroutine... (this could also be done if coroutine is suspended in the middle if its code is able to handle it.)
        coro_engine.SetInputParameters( 42LL, true, std::string( "Hello" ) );
        // run the coroutine until finished.
        while( coro_engine.CanBeContinued() ) {
            std::cout << "next parameter: ";
            coro_engine.Run();
        }

        // now lets launch some background tasks...
        auto coro = teascript::CoroutineScriptEngine::Build( fibonacci_code, teascript::eOptimize::O2, "fibonacci" );
        coro_engine.ChangeCoroutine( coro );
        //                                                    just for showing the existance of ContextFactory,
        teascript::CoroutineScriptEngine  coro_engine2( coro, teascript::ContextFactory(teascript::config::core()).MoveOutContext() );
        teascript::CoroutineScriptEngine  coro_engine3( coro );

        std::cout << "launching 3 background tasks for calculating fibbonacci 18, 20 and 23 ..." << std::endl;

        auto fut1 = std::async( std::launch::async, [&]( teascript::I64 fib ) {
            coro_engine.SetInputParameters( fib );
            return coro_engine();
        }, 23LL );
        auto fut2 = std::async( std::launch::async, [&]( teascript::I64 fib ) {
            coro_engine2.SetInputParameters( fib );
            return coro_engine2();
        }, 20LL );
        auto fut3 = std::async( std::launch::async, [&]( teascript::I64 fib ) {
            coro_engine3.SetInputParameters( fib );
            return coro_engine3();
        }, 18LL );

        // wait for all.
        fut3.wait();
        fut2.wait();
        fut1.wait();

        std::cout << "fut3=" << fut3.get() << ", fut2=" << fut2.get() << ", fut1=" << fut1.get() << std::endl;


        // now finally run in a loop for a specific time until a value is yielded... (co-oparative multi tasking possible) 

        coro_engine.Reset();
        
#if !defined(NDEBUG)
        constexpr long long n = 20LL;
#else
        constexpr long long n = 30LL;
#endif
        coro_engine.SetInputParameters( n );
        teascript::ValueObject res;
        long long counter = 1;
        std::cout << "start calculating co-operatively..." << std::endl;
        while( not res.HasValue() and coro_engine.CanBeContinued() ) {
            std::cout << counter << ": executing 100 more milliseconds..." << std::endl;
            res = coro_engine.RunFor( teascript::StackVM::Constraints::MaxTime( std::chrono::milliseconds(100) ) );
            ++counter;
            // here we could do sth. else first before continue calculation....
        }

        std::cout << "fibonacci of " << n << " is " << res << std::endl;

    } catch( teascript::exception::runtime_error const &ex ) {
#if TEASCRIPT_FMTFORMAT  // you need libfmt for pretty colored output
        teascript::util::pretty_print_colored( ex );
#else
        teascript::util::pretty_print( ex );
#endif
    } catch( std::exception const &ex ) {
        std::cout << "Exception caught: " << ex.what() << std::endl;
    }
}