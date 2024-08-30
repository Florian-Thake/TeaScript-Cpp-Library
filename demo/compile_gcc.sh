# for color + format support: -I../../fmt/fmt-11.0.2/include/
# for TOML support: -I../../tomlplusplus/tomlplusplus-3.4.0/include/

g++ -std=c++20 -Wall -O2 -DNDEBUG -I../../fmt/fmt-11.0.2/include/ -I../include/  \
    suspend_thread_demo.cpp           \
    coroutine_demo.cpp                \
    teascript_demo.cpp                \
    -o teascript_demo
