# TeaScript C++ Library
This is a snapshot of the latest Release of the **TeaScript C++ Library** for embed in C++ Applications.<br><br>
**Extend** your applications dynamically during runtime with TeaScript.<br>
Once your application has **customization points** for TeaScript each installation/distribution can be extended/modified **without** expensive re-compile and deployment procedure with arbitrary and individual functionality.

This Library can be used as **header only** library and is **dependency free** for fully C++20 supporting compilers / C++ standard libraries.

Provided with this Library is a **demo app** for illustrating the C++ API usage and a basic way for execute TeaScript files.

**TeaScript** is an embeddable and standalone **Multi-Paradigm script language** close to C++ syntax but easier to use.<br>
More information about TeaScript is available here: https://tea-age.solutions/teascript/overview-and-highlights/

A Library bundle with more example scripts as well as a full featured **TeaScript Host Application** for execute **standalone** script files, 
an interactive shell, a REPL, debugging options, time measurement and more can be downloaded for free here: <br>
https://tea-age.solutions/downloads/

# About TeaScript
**What is new in TeaScript 0.11.0?** Get all infos in the news article:<br>
https://tea-age.solutions/2023/12/11/release-of-teascript-0-11-0/ <br>
<br>
Get a very nice overview with the most **impressive highlights** here:<br>
https://tea-age.solutions/teascript/overview-and-highlights/ <br>
<br>
TeaScript language documentation:<br>
https://tea-age.solutions/teascript/teascript-language-documentation/ <br>
Integrated Core Library Documentation:<br>
https://tea-age.solutions/teascript/teascript-core-library-documentation/<br>
<br>
Read a comparison between TeaScript and other C++ scripting libraries:<br>
https://tea-age.solutions/2023/01/31/script-language-comparison-for-embed-in-cpp/

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
// We want to compute the square of the passed parameter.
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
// PRO TIPP: use a capturing lambda or std::bind to carry any user context with you.
engine.RegisterUserCallback( "squared", calc_square );

// execute the script which will call our function
auto const res = engine.ExecuteCode( "squared( 2 )" );

// print the result.
std::cout << "square is " << res.GetAsInteger() << std::endl;
```

More examples are in the [teascript_demo.cpp](demo/teascript_demo.cpp) of this repo.

# Example TeaScript Code

(Better syntax highlighting on the TeaScript home page.)

```cpp
def    age  := 42     // mutable variable of type i64
const  year := 2022   // const variable of type i64

// output: Thomas is 42 years old and born in 1980.
println( "Thomas is %(age) years old and born in %(year - age)." ) 


// result of a loop can be direct assigned to a variable. (can do this with any code blocks)
// illustrating this by computing the gcd (greatest common divisor) with a loop and store the result directly in a variable:
def x1 := 48
def x2 := 18
def gcd := repeat {         // assign the result of the repeat loop
    if( x1 == x2 ) {
        stop with x1        // the result of the loop
    } else if( x1 > x2 ) {
        x1 := x1 - x2
    } else /* x2 > x1 */ {
        x2 := x2 - x1
    }
}

// gcd will be 6


// easy loop control - loops are addressable
def c := 10
repeat "this" {         // loop is named "this"
    repeat "that" {     // loop is named "that"
        c := c – 1
        if( c > 6 ) {
            loop "that" // loop to the head (start) of the inner loop (the "that"-loop)
        }
        stop "this"     // stop the outer loop (the "this"-loop) directly from within the inner loop with one statement!
    }
    // This code will never be reached!
    c := 0
}
   
// c will be 6 here.


// === Lambdas and higher order functions:

// classical way to define a function.
func call( f, arg )
{
    f( arg ) // calling f. f must be sth. callable, e.g., a function
    // NOTE: return value of f will be implicit returned.
}

def squared := func ( x ) { x * x } // This is the Uniform Definition Syntax for define functions.

call( squared, 3 )  // passing function as parameter. result: 9

call( func (z) { z + z }, 3 ) // passing lambda as parameter. result: 6
```
More impressive highlights on:<br>
https://tea-age.solutions/teascript/overview-and-highlights/


# Supported compiler (tested with)

Visual Studio 2022 (17.2 or newer) 
- Visual Studio 2019 also works (starting from 16.11.14)

g++ 11.3
- untested if g++ 10 or 9 could work as well.

clang 14 (with libstdc++ or libc++)
- untested if clang 13 could work as well.

Newer compilers should work in general.<br>
All compilers are compiling in **C++20** and for **x86_64**.<br>
<br>
**Warning free** on Visual Studio for Level 4 and gcc/clang for -Wall

# Dependencies

**None** -- for fully C++20 supporting compilers / C++ standard libraries.

**Libfmt (as header only)** -- for gcc 11 / clang 14 (tested with **libfmt 9.1.0**)<br>
- Libfmt can be downloaded here https://fmt.dev/latest/index.html <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically.

HINT: For Windows with C++20 it is also recommended to use libfmt for the best possible Unicode support for print to stdout.

**Optional Features:** <br>
**TOML Support** - for the integrated TOML Support you need the toml++ Library. (tested with **3.4.0**) <br>
You can find the toml++ library here: https://github.com/marzer/tomlplusplus <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically. <br>
See include/teascript/TomlSupport.hpp for some more details.

# Building the demo app

**Windows:** Use the provided VS-project or the settings in compile.props.
If you make a new project, you only need to add the teascript_demo.cpp file and
set the include path to /include/

**Linux:** Use the compile_gcc.sh or compile_clang.sh with prior updated include path to libfmt.

**Other:** Try the way described for Linux. Maybe it will work.

You can test the demo app by invoking it via the provided gcd.tea:<br>
Windows:<br>
`./teascript_demo.exe gcd.tea 18 42`

Linux:<br>
`./teascript_demo gcd.tea 18 42`

If you see **6** as the result everything is functional.

# API stability

TeaScript is pre-mature and many things will probably change in some new release.<br>
However, the public methods of the `teascript::Engine` class as well as the public getters of the `teascript::ValueObject` class are considered stable (if not otherwise documented in the code).<br> 
This means that I will try to ensure backward compatibility if possible or provide a smooth transition - except if major issues are detected or at very rare circumstances.<br>
This also counts for the headers version.h / Exception.hpp / SourceLocation.hpp.<br>
All other classes / structures / types (including all its methods and members), and free functions are considered unstable and may change often or might be even removed entirely.

Methods / Functions marked with 
- **DEPRECATED**: Try to migrate to the new available way as soon as possible. This method will be removed within some next (pre-)release.
- **INTERNAL**: Don't use these methods. They are reserved for internal usage and may render your program unstable / unusable. You cannot rely on the functionality / behavior / existence.
- **EXPERIMENTAL**: This is a new functionality / technique and might be changed/modified after some feedback from practical usage before it becomes stable (or it might be removed/created in a new way from scratch, if some issue raised up.)

## Can TeaScript be used for production already?

Yes, it can. I explain why and what is the best practice:

First, although the 1.0 release is not done yet, every release has 3 levels of quality assurance:

* UnitTests – on C++ as well as on TeaScript level (TeaScript files testing TeaScript features).
* Functional tests with scripts.
* Manual testing.

This ensures a high level of quality already.

Second, the TeaScript syntax and language features which are present already will stay compatible and are covered by the tests mentioned above. In new versions new syntax and new language features will be added, but old written scripts will still be functional in 99% of the cases (only if the script used some quirks or rely on broken functionality or if really big issues are addressed, this backward compatibility might not be hold.)

Third, usage of the high-level C++ API only. This API will stay backward compatible or a soft migration will be provided – except if major issues are detected or at very rare circumstances.
The high-level API consists of the classes teascript::Engine / teascript::EngineBase, all public getters in teascript::ValueObject as well as everything in Exception.hpp / SourceLocation.hpp and version.h (except if otherwise noted).

# License
The TeaScript C++ Library is 100% Open Source and Free Software and licensed under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 (AGPL-3.0): see LICENSE.TXT <br>

If you cannot or don’t want use this specific open source license, you may ask for a different license.

# Disclaimer
This software is provided “as-is” without any express or implied warranty. In no event shall the author be held liable for any damages arising from the use of this software.

See also the included COPYRIGHT.TXT and LICENSE.TXT for license ifnormation, usage conditions and permissions.

This software is a pre-release.
The behavior, API/ABI, command line arguments, contents of the package, usage conditions + permissions and any other aspects of the software and the package may change in a non backwards compatible or a non upgradeable way without prior notice with any new (pre-)release.

# Support my work
If you want to support my work, especially to further develop TeaScript and to run its website, you can do this by a donation of your choice via Paypal:

https://paypal.me/FlorianThake

The donation will be for covering the monthly and yearly costs as well as for free personal use. All donations will help me to put more time and effort into TeaScript and its webpage. Thank you very much! 🙂

# Copyright
Copyright (C) 2023 Florian Thake <contact |at| tea-age.solutions>.

This file:
Copyright (C) 2023 Florian Thake. All rights reserved.
