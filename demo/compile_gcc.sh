# for color + format support: -I../../fmt/fmt-10.1.1/include/
# for TOML support: -I../../tomlplusplus/tomlplusplus-3.4.0/include/

g++ -std=c++20 -Wall -O2 -I../../fmt/fmt-10.1.1/include/ -I../include/ teascript_demo.cpp -o teascript_demo
