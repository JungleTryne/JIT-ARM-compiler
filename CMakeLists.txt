cmake_minimum_required(VERSION 3.17)
project(jit_compiler)

set(CMAKE_CXX_STANDARD 17)

add_executable(jit_compiler main.cpp src/JIT_compiler.cpp)