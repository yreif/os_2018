cmake_minimum_required(VERSION 3.8)
project(os_ex2)

set(CMAKE_CXX_STANDARD 11)

set(ATOMIC ex2/uthreads.cpp ex2/uthreads.h)
set(BARRIER ex2/uthreads.cpp ex2/uthreads.h)
set(SAMPLE ex2/uthreads.cpp ex2/uthreads.h)
add_executable(test1 ${SOURCE_FILES} ex2/tests/test1.cpp)
add_executable(test2 ${SOURCE_FILES} ex2/tests/test2.cpp)
add_executable(test5 ${SOURCE_FILES} ex2/tests/test5fixed.cpp)
