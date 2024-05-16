# TeaScript C++ Library
This is a snapshot of the latest Release of the **TeaScript C++ Library** for embed in C++ Applications.<br><br>
Extend your applications dynamically during runtime with TeaScript.<br>
Once your application has customization points for TeaScript each installation/distribution can be extended/modified without expensive re-compile and deployment procedure with arbitrary and individual functionality.

TeaScript is an embeddable and standalone **Multi-Paradigm script language** close to C++ syntax but easier to use.<br>
More information about TeaScript is available here: https://tea-age.solutions/teascript/overview-and-highlights/

This Library can be used as a **header only** library and can be **dependency free** for fully C++20 supporting compilers / C++ standard libraries (optional features require to include other free software libraries).

Provided with this Library is a demo app for illustrating the C++ API usage and a basic way for execute TeaScript files.

The full featured **TeaScript Host Application** for execute standalone script files, an interactive shell, a REPL, debugging capabilities, time measurement and more can be downloaded **for free** as a **pre-compiled** Windows and Linux bundle and with included source code here:<br>
https://tea-age.solutions/downloads/<br>

Also, a Library bundle with more example scripts is available in the download section as well.

# About TeaScript
**What is new in TeaScript 0.14.0?** TeaScript 0.14 comes with the Tea StackVM, a Compiler, Dis-/Assembler, Suspend+Continue, Yield, improved debugging and more.<br>
<br>Get all infos in the **latest news** article:<br>
https://tea-age.solutions/2024/05/15/release-of-teascript-0-14-0/ <br>

## Summary of the latest release
- (automatically) **compile** and **execute** TeaScript files in the new **integrated Tea StackVM**.
- **Save** and **Load** compiled TeaScript programs as TeaScript Binary files (*.tsb).
- **Suspend** and **Continue** TeaScript programs at (nearly) any time by either themselves, by maximum time or instruction constraint or by an external request from another thread.
- Use TeaScript code similar like a **coroutine** in your C++ Application by yielding values from any place and **co-operative / preemptive execution** possibilities.
- Nerd feature: program directly in TeaScript **Assembler**.
- improved debugging capabilities with single stepping, view the callstack, view corresponding source code and more.
- Opt-out header only library for save includes and compile time

## General information
Get a very nice overview with the most **impressive highlights** here:<br>
https://tea-age.solutions/teascript/overview-and-highlights/ <br>
<br>
Read nice introductions of every new language and library feature in the release blog post collection:<br>
[Release blog posts](https://tea-age.solutions/teascript/downloads/#all_release_articles) <br>

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

The next example shows how to use a C++ callback function from the script
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
The last example is a code snippet from [coroutine_demo.cpp](demo/coroutine_demo.cpp) and shows one of the coroutine like usage possibilities.
```cpp
#include "teascript/CoroutineScriptEngine.hpp"

// coroutine like TeaScript code
// calculate faculty starting at 1!
constexpr char faculty_code[] = R"_SCRIPT_(
def fac := 1
def n   := 2
repeat {
    yield fac
    fac := fac * n
    n   := n + 1
}
)_SCRIPT_";

// somewhere in the code...
// setup the Coroutine engine with the faculty calculation coroutine.
teascript::CoroutineScriptEngine  coro_engine( teascript::CoroutineScriptEngine::Build( faculty_code, teascript::eOptimize::O1, "faculty" ) );

// lets calculate some values and print them...
std::cout << "next faculty number: " << coro_engine() << std::endl;
std::cout << "next faculty number: " << coro_engine() << std::endl;
std::cout << "next faculty number: " << coro_engine() << std::endl;
std::cout << "next faculty number: " << coro_engine() << std::endl;
std::cout << "next faculty number: " << coro_engine() << std::endl;
```

## More C++ examples

More examples are in the [teascript_demo.cpp](demo/teascript_demo.cpp), [suspend_thread_demo.cpp](demo/suspend_thread_demo.cpp) and [coroutine_demo.cpp](demo/coroutine_demo.cpp) of this repo.

# Example TeaScript Code

**Hint:** Better syntax highlighting is on the TeaScript home page or in Notepad++ with the provided [SyntaxHighlighting.xml](TeaScript_SyntaxHighlighting_Notepad%2B%2B.xml)

```cpp
def    age  := 42u8   // mutable variable of type u8
const  year := 2022   // const variable of type i64 (i64 is the default for Numbers)

// output: Thomas is 42 years old and born in 1980.
println( "Thomas is %(age) years old and born in %(year - age)." )

// or use the string format feature (must link with libfmt for be available!)
// all formatting options of libfmt are possible except named arguments.
println( format( "Thomas is {} years old and born in {}.", age, year - age ) )


// result of a loop can be direct assigned to a variable. (can do this with any code blocks, like if, etc.)
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


// === Tuples and Named Tuples as C-like structs ===

def tup := (1,3,5,7)     // tuple with 4 elements
tup.0                    // 1    (or use tup[0] or _tuple_val( tup, 0 ))
tup.1                    // 3
// can append with the Uniform Definition Syntax (or use _tuple_append())
def tup.4 := 9
tup.4                    // 9
// and delete as well (or use _tuple_remove())
undef tup.0              // now all remaining elements are down one index
tup.0                    // 3 now
tup.1 := 6               // 6 now    (or use tup[1] := 6 or _tuple_set( tup, 1, 6 ))

// types can be mixed (and nested)
def mixed_tup := (true, "Peter", 3.123, 0xffffu64, ("nested", 123u8))
mixed_tup.1              // "Peter"
mixed_tup[4][1]          // 123

// easy iterating over all tuple elements
forall( idx in mixed_tup ) {
    println( mixed_tup[ idx ] )
}


// -- NAMED TUPLES as C-like structs --

// most easy with the Uniform Definition Syntax
// (have a look at the battery of utility functions for more possibilities!)
def root := _tuple_create()           // start with empty tuple
def root.name  := "Peter"             // add element "name" with value "Peter"
def root.email := "peter@mail.com"    // add element "email"
def root.birth := _tuple_create()     // sub tuple
def root.birth.year  := 1980          // add element "year" to tuple "birth" with value 1980
def root.birth.month := 10            // add element "month" to tuple "birth"
def root.birth.day   := 27            // add element "day" to tuple "birth"

// PRO TIP: use a factory function for act as a constructor for same Named Tuple structures.

root.name       // "Peter"
root.1          // "peter@mail.com" (access by index still possible)
root.birth      // (1980, 10, 27)
root.birth.day  // 27
// alternatives:
root[0]         // "Peter"   (of course the Number could be a variable here)
root["name"]    // "Peter"   (of course the string could be a variable here)
root."name"     // "Peter"
// access way can be changed for each level, e.g.:
root["birth"].0 // 1980   (or root[2]."year", ... )

// There are more features and possibilities available for Tuples / Named Tuples
// which can be found in the various release blog posts (especially version 0.10.0 / 0.11.0) and documentation pages.

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
## More TeaScript code examples

More impressive **highlights** on:<br>
https://tea-age.solutions/teascript/overview-and-highlights/<br>
and in the provided example files in the demo directory:<br> 
[demo](demo/) <br>
as well as in the collection of release blog posts which are also introducing the new language and library features:<br>
[Release blog posts](https://tea-age.solutions/teascript/downloads/#all_release_articles)


# Supported compiler (tested with)

Visual Studio 2022 (17.2 or newer)<br><br>
Depending of the code size it might be required to add the /bigobj flag to the additional settings.<br>
For details see on [StackOverflow](https://stackoverflow.com/questions/15110580/penalty-of-the-msvs-compiler-flag-bigobj)
- Visual Studio 2019 also works (starting from 16.11.14)

g++ 11.3
- Use g++ 13 for all linux specific known issues are solved (see Known_Issues.txt)

clang 14 (with libstdc++ or libc++)
- There are clang and/or libc++ specific known issues (see Known_Issues.txt)

Newer compilers should work in general.<br>
All compilers are compiling in **C++20** and for **x86_64**.<br>
<br>
**Warning free** on Visual Studio for Level 4 and gcc/clang for -Wall

# Dependencies

**None** -- for fully C++20 supporting compilers / C++ standard libraries.

**Libfmt (as header only)** -- for gcc 11 / clang 14 (tested with **libfmt 10.2.1** and **libfmt 10.1.1**)<br>
- Libfmt can be downloaded here https://fmt.dev/latest/index.html <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically.

HINT: For Windows with C++20 it is also recommended to use libfmt for the best possible Unicode support for print to stdout.

**Optional Features:** <br>
**TOML Support** - for the integrated TOML Support you need the toml++ Library. (tested with **3.4.0**) <br>
You can find the toml++ library here: https://github.com/marzer/tomlplusplus <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically. <br>
See include/teascript/TomlSupport.hpp for some more details.

**Color and Format String Support** - for colorful output and the format string feature you need the libfmt library.<br>
See Libfmt section above.

# Building the demo app

**Windows:** Use the provided VS-project or the settings in compile.props.
If you make a new project, you only need to add the files teascript_demo.cpp, suspend_thread_demo.cpp and coroutine_demo.cpp and
set the include path to /include/

**Linux:** Use the compile_gcc.sh or compile_clang.sh with prior updated include path to libfmt.

**Other:** Try the way described for Linux. Maybe it will work.

You can test the demo app by invoking it via the provided gcd.tea:<br>
Windows:<br>
`./teascript_demo.exe gcd.tea 18 42`

Linux:<br>
`./teascript_demo gcd.tea 18 42`

If you see **6** as the result everything is functional.

## Opt-out header only usage

The TeaScript C++ Library will be compiled as a header only library per default.
For save includes and compile time it is possible to opt-out header only compilation.

Please, read the instruction in the related release blog post here:
https://tea-age.solutions/2024/05/15/release-of-teascript-0-14-0/#opt-out_header_only_usage

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
TeaScript is 100% Open Source and Free Software and the **C++ Library** is licensed under the Mozilla Public License 2.0.<br>

The Mozilla Public License can be read here https://www.mozilla.org/en-US/MPL/2.0/ <br>

This license has the following advantages for TeaScript and for its users:<br>

- it can be linked statically (if all conditions are fulfilled).
- it is explicit compatible with AGPL, GPL and LGPL.
- it is compatible with Apache, MIT, BSD, Boost licenses (and others).
- Larger works (means Applications using TeaScript) can be distributed closed source (or under a compatible license) as long as the conditions are fulfilled.
- For the upcoming module system it means that a new first- or third-party module for TeaScript may use any compatible license like MPL-2.0, (A)GPL, MIT, Apache and so on.
<br>
Many questions regarding the MPL are also answered in the official FAQ:<br>
https://www.mozilla.org/en-US/MPL/2.0/FAQ/<br>
<br>
If you have further questions regarding the license don’t hesitate to contact me.

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
Copyright (C) 2024 Florian Thake <contact |at| tea-age.solutions>.

This file:
Copyright (C) 2024 Florian Thake. All rights reserved.
