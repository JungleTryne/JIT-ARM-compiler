cmake_minimum_required(VERSION 3.17)
project(jit_compiler)

set(CMAKE_CXX_COMPILER /home/jungletryne/gcc-linaro-7.3.1-2018.05-i686_arm-linux-gnueabi/bin/arm-linux-gnueabi-g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -marm")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

set(CMAKE_CXX_STANDARD 17)

add_executable(jit_compiler main.cpp src/JIT_compiler.cpp)