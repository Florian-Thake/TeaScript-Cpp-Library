# TeaScript C++ Library
This is a snapshot of the latest Release of the **TeaScript C++ Library** for embed in C++ Applications.
Extend your applications dynamically during runtime with TeaScript.

This Library can be used as **header only** library and is **dependency free** for fully C++20 supporting compilers / C++ standard libraries.

Provided with this Library is a **demo app** for illustrating the C++ API usage and a basic way for execute TeaScript files.

More inforamtion about TeaScript is available here: https://tea-age.solutions/teascript/overview-and-highlights/

A Library bundle **with more example scripts** as well as a full featured **TeaScript Host Application** for execute **standalone** script files, 
an interactive shell, a REPL, debugging opions, time measurement and more can be downloaded here: <br>
https://tea-age.solutions/downloads/

# About TeaScript
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

Newer compilers should work in general.<br>
All compilers are compiling in **C++20** and for **x86_64**.<br>
<br>
**Warning free** on Viusal Studio for Level 4 and gcc/clang for -Wall

# Dependencies

**None** -- for fully C++20 supporting compilers / C++ standard libraries.

**Libfmt (as header only)** -- for gcc 11 / clang 14 (tested with **libfmt 9.1.0**)<br>
- Libfmt can be downloaded here https://fmt.dev/latest/index.html <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically.

HINT: For Windows with C++20 it is also recommended to use libfmt for the best possible Unicode support for print to stdout.

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

TeaScript is pre-mature and many things will probably change in some new (pre-)release.<br>
However, the public methods of the `teascript::Engine` class as well as the public getters of the `teascript::ValueObject` class are considered semi-stable (if not otherwise documented in the code).<br> 
This means that I will try to ensure backward compatibility if possible or provide a smooth transition - except if major issues are detected or at very rare circumstances.<br>
This also counts for the headers version.h / Util.hpp / Exception.hpp.<br>
All other classes / structures / types (including all its methods and members), and free functions are considered unstable and may change often or might be even removed entirely.

Methods / Functions marked with 
- **DEPRECATED**: Try to migrate to the new available way as soon as possible. This method will be removed within some next (pre-)release.
- **INTERNAL**: Don't use these methods. They are reserved for internal usage and may render your program unstable / unusable. You cannot rely on the functionaity / behavior / existence.
- **EXPERIMENTAL**: This is a new functionality / technique and mighht be changed/modified after some feedback from practical usage before it becomes stable (or it might be removed/created in a new way from scratch, if some issuse raised up.)

## Can TeaScript be used for production already?

Yes, it can for certain and specific scenarios. I explain why and which:

First, although the 1.0 release is not done yet, every release has 3 levels of quality assurance:

- UnitTests – on C++ as well as on TeaScript level (TeaScript files testing TeaScript features).
- Functional tests with scripts.
- Manual testing.

This ensures a high level of quality already.

Second, use the high-level C++ API only (e.g. via class teascript::Engine ). This API will stay backward compatible already or a soft migration will be provided – except if major issues are detected or at very rare circumstances.

# License
The TeaScript C++ Library is licensed under the the TeaScript Library Standard License.<br>
Please, read the LICENSE.TXT carefully. You may find another copy also at https://tea-age.solutions/teascript/product-variants/ <br>
If the license does not fit for you, you can purchase a license with lesser restrictions via the contact form on<br>
https://tea-age.solutions<br>
or issue a request to <contact |at| tea-age.solutions><br>

Because of the pre-mature nature of TeaScript, special offerings, discounts, more free support and influence to the feature list and feature priority is possible when purchasing a license.

## Why a restrictive license?
I put a lot of work and time into TeaScript so far. If somebody is able to generate some income with the help of my work/time, I want something back.<br>
Because, otherwise, it would mean that I worked for free for others to make money. For me that is not fair. <br>
But if somebody don't make money and just enjoy TeaScript or provide something for free, then I'll do so as well.<br>
For develop TeaScript further I must be able to invest in time and resources. Otherwise a quick development (or a development at all) is not secured.

# Disclaimer
This software is provided “as-is” without any express or implied warranty. In no event shall the author be held liable for any damages arising from the use of this software.

See also the included LICENSE.TXT for usage conditions and permissions.

This software is a pre-release.
The behavior, API/ABI, command line arguments, contents of the package, usage conditions + permissions and any other aspects of the software and the package may change in a non backwards compatible or a non upgradeable way without prior notice with any new (pre-)release.

# Copyright
Copyright (C) 2023 Florian Thake <support |at| tea-age.solutions>. All rights reserved.
