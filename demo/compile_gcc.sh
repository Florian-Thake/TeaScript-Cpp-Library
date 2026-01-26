# for color + format support: -I../../fmt/fmt-11.1.4/include/
# for TOML support:           -I../../tomlplusplus/tomlplusplus-3.4.0/include/
# extensions include          -I../extensions/include/
# modules include             -I../modules/include/
# nlohmann:                   -I../../nlohmann/nlohmann_3.11.3/include/
# Boost:                      -I../../boost_1_86_0/
# Rapid:                      -I../../rapidjson/include/
# for reflect cpp:            -I../../reflect-cpp-0.23.0/include/
#
# example with webpreview enabled:
# g++ -std=c++20 -Wall -O2 -DNDEBUG -DTEASCRIPT_ENGINE_USE_WEB_PREVIEW=1 -I../modules/include/ -I../../fmt/fmt-11.1.4/include/ -I../include/ ../modules/source/Webpreview.cpp <other .cpp>

g++ -std=c++20 -Wall -O2 -DNDEBUG -I../../fmt/fmt-11.1.4/include/ -I../extensions/include/ -I../include/  \
    suspend_thread_demo.cpp           \
    coroutine_demo.cpp                \
    reflectcpp_demo.cpp               \
    teascript_demo.cpp                \
    -o teascript_demo
