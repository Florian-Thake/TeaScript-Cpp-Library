# TeaScript C++ Library
This is a snapshot of the latest Release of the **TeaScript C++ Library** for embed in C++ Applications.
Extend your applications dynamically during runtime with TeaScript.

This Library can be used as **header only** library and is **dependency free** for fully C++20 supporting compilers / C++ standard libraries.

Provided with this Library is a **demo app** for illustrating the C++ API usage and a basic way for execute TeaScript files.

More inforamtion about TeaScript is available here: https://tea-age.solutions/teascript/overview-and-highlights/

A Library bundle **with more example scripts** as well as a full featured **TeaScript Host Application** for execute **standalone** script files, 
an interactive shell, a REPL, debugging opions, time measurement and more can be downloaded here: <br>
https://tea-age.solutions/downloads/

# Example Usage

The first example will do the classic "Hello World" with the TeaScript C++ Library.
```cpp
// TeaScript includes
#include "teascript/Engine.hpp"

int main()
{
    // create a TeaScript default engine.
    teascript::Engine  engine;  
    
    // add a const string variable (just for illustrating variable usage)
    engine.AddConst( "mytext", "Hello, World!" );    // variable mytext is a const string.
    
    engine.ExecuteCode( "println( mytext )" ); // print the text to stdout - thats all!
    
    return 0;
}
```

The next example will use variables and do a computation.
```cpp
// TeaScript includes
#include "teascript/Engine.hpp"

// create a TeaScript default engine (e.g. in the main() function).
teascript::Engine  engine;

// add 2 integer variables a and b.
engine.AddVar( "a", 2 );   // variable a is mutable with value 2
engine.AddVar( "b", 3 );   // variable b is mutable with value 3    

// execute the script code passed as string. it computes new variable c based on values a and b.
// if c is greater 3 it prints a message to stdout.
// finally it returns the value of variable c.
auto const res = engine.ExecuteCode(    "const c := a + b\n"
                                        "if( c > 3 ) {\n"
                                        "    println( \"c is greater 3.\" )\n"
                                        "}\n"
                                        "c\n" 
                                    );
// print the result.
std::cout << "c is " << res.GetAsInteger() << std::endl;
```

The next example illustrates how to invoke TeaScript files stored on disk/storage.
```cpp
// std includes
#include <iostream>
#include <filesystem>

// TeaScript includes
#include "teascript/Engine.hpp"

// somewhere in the code...
// create a TeaScript default engine (e.g. in the main() function).
teascript::Engine  engine;

// and execute the script file.
auto const res = engine.ExecuteScript( std::filesystem::path( "/path/to/some/script.tea" ) );

// print the result...
std::cout << "result: " << res.PrintValue() << std::endl;
```

The last example shows how to use a C++ callback function from the script
```cpp
// TeaScript includes
#include "teascript/Engine.hpp"

// this is our simple callback function which we will call from TeaScript code.
// The callback function signature is always ValueObject (*)( Context & )
// We want to compute the squre of the passed parameter.
teascript::ValueObject calc_square( teascript::Context &rContext )
{
    if( rContext.CurrentParamCount() != 1 ) { // this check could be relaxed also...
        // in case of wrong parameter count, throw eval error with current source code location.
        throw teascript::exception::eval_error( rContext.GetCurrentSourceLocation(), "Calling calc_square: Wrong amount of parameters! Expecting 1.");
    }
    
    // get the operand for the calculation.
    auto const op = rContext.ConsumeParam();
    
    // calculate the square and return the result.
    return teascript::ValueObject( op.GetAsInteger() * op.GetAsInteger() );
}


// somewhere in the code...
// create a TeaScript default engine (e.g. in the main() function).
teascript::Engine  engine;

// register a function as a callback. can use arbitrary names.
engine.RegisterUserCallback( "squared", calc_square );

// execute the script which will call our function
auto const res = engine.ExecuteCode( "squared( 2 )" );

// print the result.
std::cout << "square is " << res.GetAsInteger() << std::endl;
```

More examples are in the teascript_demo.cpp of this repo.


# Supported compiler (tested with)

Visual Studio 2022 (17.2 or newer) 
- Visual Studio 2019 also works (starting from 16.11.14)

g++ 11.3
- untested if g++ 10 or 9 could work as well.

clang 14 (with libstdc++ or libc++)
- untested if clang 13 could work as well.

Newer compilers should work in general.
All compilers are compiling in **C++20** and for **x86_64**.

# Dependencies

**None** -- for fully C++20 supporting compilers / C++ standard libraries.

**Libfmt (as header only)** -- for gcc 11 / clang 14 (tested with **libfmt 9.1.0**)<br>
- Libfmt can be downloaded here https://fmt.dev/latest/index.html <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically.

HINT: For Windows with C++20 it is also recommended to use libfmt for the best possible Unicode support for print to stdout.

