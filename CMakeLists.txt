cmake_minimum_required(VERSION 3.8)
project(os_ex2)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES uthreads.cpp uthreads.h)
add_executable(test1 ${SOURCE_FILES} test1.cpp)
add_executable(test2 ${SOURCE_FILES} test2.cpp)
add_executable(test5 ${SOURCE_FILES} test5fixed.cpp)
add_executable(test14 ${SOURCE_FILES} test14.cpp)
add_executable(test42 ${SOURCE_FILES} test42.cpp)
add_executable(test132 ${SOURCE_FILES} test132.cpp)
add_executable(test429 ${SOURCE_FILES} test429.cpp)
add_executable(test1430 ${SOURCE_FILES} test1430.cpp)

add_executable(demo_jmp demo_jmp.c)