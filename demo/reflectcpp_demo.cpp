/* === TeaScript Demo Program ===
 * SPDX-FileCopyrightText:  Copyright (C) 2026 Florian Thake <contact |at| tea-age.solutions>.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/
 */


// === USAGE INSTRUCTION ===
// In order to get this example working, you need the reflectcpp library / source code (tested with version 0.23.0) https://github.com/getml/reflect-cpp
// 1) Add extensions/include of TeaScript to the include paths.
// 2) Add the 'include' directory of reflectpp to your include paths.
// 3) Add the 'src' directory of reflectcpp to your include paths.
// 4) Build the teascript_demo project and invoke it with ./teascript_demo reflect


// We opt-out for header only (must be in each TU (Translation Unit) where teascript includes are used, or project wide).
// NOTE: The default is header only, which does not require to do anything special.
#define TEASCRIPT_DISABLE_HEADER_ONLY       1


// you must add 'extensions/include' to the include path
#if __has_include("teascript/ext/Reflection.hpp")
# include "teascript/ext/Reflection.hpp"
#else
# define TEASCRIPT_EXTENSION_REFLECTCPP      0
# define TEASCRIPT_NO_REFLECTION_HPP         1
#endif

#include <iostream>
#include <string>
#include <vector>

#if TEASCRIPT_EXTENSION_REFLECTCPP

// If we have the source, pull in reflectcpp 
// as described here https://rfl.getml.com/install/#option-4-include-source-files-into-your-own-build
#if TEASCRIPT_EXTENSION_REFLECTCPP == 2

// we use Warning Level 4, but reflectcpp has warings there, so disable them...
#if defined(_MSC_VER )
# pragma warning( push )
# pragma warning( disable: 4324 )
#endif

//via Refection.hpp\\#include "rfl.hpp"
#include "reflectcpp.cpp"

#if defined(_MSC_VER )
# pragma warning( pop )
#endif

#endif // TEASCRIPT_EXTENSION_REFLECTCPP == 2

#include "teascript/Engine.hpp"

#endif  // TEASCRIPT_EXTENSION_REFLECTCPP

// some C++ struct (note the self reference in children)
struct Person
{
    std::string first_name;
    std::string last_name;
    int age{0};
    std::vector<Person> children;
};

void teascript_reflectcpp_demo()
{
#if TEASCRIPT_EXTENSION_REFLECTCPP == 2
    // create an example instance of the C++ struct.
    auto  homer = Person{.first_name = "Homer",
                         .last_name = "Simpson",
                         .age = 45};
    homer.children.emplace_back( Person{"Maggie", "Simpson", 1} );
    homer.children.emplace_back( Person{"Bart", "Simpson", 10} );

    
    // create the default teascript engine.
    teascript::Engine engine;

    // import the C++ struct instance into TeaScript.
    teascript::reflect::into_teascript( engine, "homer", homer );
    
    std::cout << "\nC++ struct Person instance 'homer' imported as TeaScript Tuple: " << std::endl;
    engine.ExecuteCode( "tuple_print( homer, \"homer\", 10 )" );

    std::cout << "\n\nadding Lisa as child and store a reference as variable 'lisa' ... ";
    engine.ExecuteCode( R"_SCRIPT_(
                        _tuple_append( homer.children, _tuple_named_create( ("first_name", "Lisa"), ("last_name", "Simpson"), ("age", 8), ("children", json_make_array() ) ) )
                        
                        def lisa @= homer.children[2]
                        )_SCRIPT_"  );

    std::cout << "done!\n\nprinting 'homer' again:" << std::endl;
    engine.ExecuteCode( "tuple_print( homer, \"homer\", 10 )" );
    std::cout << "\n\nprinting 'lisa' TeaScript Tuple: " << std::endl;
    engine.ExecuteCode( "tuple_print( lisa, \"lisa\", 10 )" );

    try {
        std::cout << "\nexporting 'lisa' as C++ struct Person via this code\nPerson lisa = teascript::reflect::from_teascript<Person>( engine, \"lisa\" );" << std::endl;
        
        // exporting from TeaScript into a new C++ struct instance!
        // !!!
        Person lisa = teascript::reflect::from_teascript<Person>( engine, "lisa" );
        // !!!
        
        std::cout << "\nSuccess!!! (you must debug the C++ code to see the Person object instance 'lisa')" << std::endl;

    } catch( std::exception const &ex ) {
        std::cout << "\nError occurred during export: " << ex.what() << std::endl;
    }

    // vvvv config error handling vvvv
#elif TEASCRIPT_EXTENSION_REFLECTCPP == 1
    std::cout << "Error: You must add the 'src' directory of reflectcpp to this project to run this example!" << std::endl;
#elif !defined( TEASCRIPT_NO_REFLECTION_HPP )
    std::cout << "Error: You must add the 'include' directory of reflectcpp to this project to run this example!" << std::endl;
#else
    std::cout << "Error: You must add the 'extensions/include/' directory of the TeaScript C++ Library to this project to run this example!" << std::endl;
#endif
}
