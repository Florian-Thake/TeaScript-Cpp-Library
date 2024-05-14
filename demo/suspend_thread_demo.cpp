/* === TeaScript Demo Program ===
 * SPDX-FileCopyrightText:  Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */


// The following demo code demostrates how a running script can be suspended by another thread.

// This demo code uses the low level components of TeaScript which may change often in its API, structure, layout, etc.
// An alternactive way would be to derive from the Engine class and use its members.
// Or have a look at class CoroutineScriptEngine and the demo code in coroutine_demo.cpp

// NOTE: This feature relies on the C++20 library feature std::stop_source|token.
//       Unfortunately, clang does not have this actually. 
//       It might be possible with the last recent version 18 combined with -fexperimental-library or
//       when use libstdc++ instead of libc++.

// We opt-out for header only (must be in each TU (Translation Unit) where teascript includes are used, or project wide).
// NOTE: The default is header only, which does not require to do anything special.
#define TEASCRIPT_DISABLE_HEADER_ONLY       1

#include "teascript/StackMachine.hpp"
#include "teascript/CoreLibrary.hpp" // we don't need the core lib for the demo TeaScript code, but because usually at least the minimal config is needed, it is here as well.
#include "teascript/Parser.hpp"
#include "teascript/StackVMCompiler.hpp"

#include <thread>

namespace {

constexpr char endless_loop_code[] = R"_SCRIPT_(
def c := 0
repeat {
    c := c + 1
}
)_SCRIPT_";

// the thread function, it will compile a program with an endless loop and execute it.

void thread_func( std::shared_ptr<teascript::StackVM::Machine<true>> the_machine, teascript::Context &rContext, std::exception_ptr &ep )
{
    try {
        teascript::Parser p;
        teascript::StackVM::Compiler c;

        auto const prog = c.Compile( p.Parse( endless_loop_code ) );

        the_machine->Exec( prog, rContext );

    } catch( ... ) {
        ep = std::current_exception();
    }
}

} // anonymous 

void teascript_thread_suspend_demo()
{
#if TEASCRIPT_SUSPEND_REQUEST_POSSIBLE

    std::exception_ptr  ep; // for error propagating.
    teascript::Context  context;
    teascript::CoreLibrary().Bootstrap( context, teascript::config::minimal() );
    auto machine = std::make_shared< teascript::StackVM::Machine<true> >();

    try {
        // launch the thread
        std::cout << "Launching thread with a TeaScript endless loop..." << std::endl;
        std::thread t(thread_func, machine, std::ref(context), std::ref(ep));

        // and sleep zZzZzZzZz
        std::cout << "... and going to sleep 5 seconds..." << std::endl;
        std::this_thread::sleep_for( std::chrono::seconds( 5 ) );

        // send the suspend request
        std::cout << "woke up, sending suspend request now..." << std::endl;
        if( not machine->Suspend() ) [[unlikely]] { // can only fail if std::stop_source::request_stop() fails.
            throw std::runtime_error( "Could not send suspend request!" );
        }
        std::cout << "waiting for join...";
        // wait for the thread.
        t.join();
        std::cout << "joined." << std::endl;


        // are there errors?
        if( ep ) {
            std::rethrow_exception( ep );
        }
        machine->ThrowPossibleErrorException();

        // just some state checking
        if( not machine->IsSuspended() ) [[unlikely]] {
            throw std::runtime_error( "Unexpected state!" );
        }

        // lets see where the endless loop was suspended....

        auto const c = context.FindValueObject( "c" );
        std::cout << "the endless loop counted until " << c.PrintValue() << " before was suspended." << std::endl;

    } catch( teascript::exception::runtime_error const &ex ) {
#if TEASCRIPT_FMTFORMAT  // you need libfmt for pretty colored output
        teascript::util::pretty_print_colored( ex );
#else
        teascript::util::pretty_print( ex );
#endif
    } catch( std::exception const &ex ) {
        std::cout << "Exception caught: " << ex.what() << std::endl;
    }

#else
    (void)thread_func;
    std::cout << "You need C++20 std::stop_source in your C++ library for be able to run this demo." << std::endl;
#endif
}