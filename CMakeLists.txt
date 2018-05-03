cmake_minimum_required(VERSION 3.8)
project(os_ex2)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES demo_singInt_handler.c)
add_executable(os_ex2 ${SOURCE_FILES})