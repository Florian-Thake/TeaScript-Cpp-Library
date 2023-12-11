#   -stdlib=libc++ 
#   -stdlib=libstdc++
#
#   for TOML support: -I../../tomlplusplus/tomlplusplus-3.4.0/include/

clang++-14 -std=c++20 -stdlib=libc++ -Wall -O2 -I../../fmt/fmt-9.1.0/include/ -I../include/ teascript_demo.cpp -o teascript_demo
