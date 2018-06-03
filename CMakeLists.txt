cmake_minimum_required(VERSION 3.8)
project(os_2018)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS -pthread)

set(ATOMIC ex3/atomicdemo/atomicdemo.cpp)
set(BARRIER ex3/barrierdemo/barrierdemo.cpp ex3/barrierdemo/Barrier.cpp ex3/barrierdemo/Barrier.h)
set(SAMPLE ex3/sampleclient/SampleClient.cpp)

set(FRAMEWORK ex3/MapReduceFramework.cpp ex3/MapReduceFramework.h ex3/MapReduceClient.h ex3/Barrier.cpp
        ex3/Barrier.h)

add_executable(atomic_demo ${ATOMIC})
add_executable(barrier_demo ${BARRIER})
add_executable(sample_client ${FRAMEWORK} ex3/SampleClient.cpp)