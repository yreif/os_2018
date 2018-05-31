cmake_minimum_required(VERSION 3.8)
project(os_ex2)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES uthreads.h uthreads.cpp main_testing.c)
add_executable(untitled1 ${SOURCE_FILES} main_testing.c)