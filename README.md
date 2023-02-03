# TeaScript C++ Library
This is a snapshot of the latest Release of the **TeaScript C++ Library** for embed in C++ Applications.
Extend your applications dynamically during runtime with TeaScript.

This Library can be used as **header only** library and is **dependency free** for fully C++20 supporting compilers / C++ standard libraries.

Provided with this Library is a **dmeo app** for illustrating the usage and a basic way for execute TeaScript files.

More inforamtion about TeaScript is available here: https://tea-age.solutions/teascript/overview-and-highlights/

A bundle **with more example scripts** as well as a full featured **TeaScript Host Application** for execute **standalone** script files, 
an interactive shell, a REPL, debugging opions, time measurement and more can be downloaded here: <br>
https://tea-age.solutions/downloads/

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

**Libfmt** -- for gcc 11 / clang 14 (tested with **libfmt 9.1.0**)<br>
- Libfmt can be downloaded here https://fmt.dev/latest/index.html <br>
You only need to set the include path in your project(s) / for compilation. Detection is then done automatically.

HINT: For Windows with C++20 it is also recommended to use libfmt for the best possible Unicode support for print to stdout.

