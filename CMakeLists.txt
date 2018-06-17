cmake_minimum_required(VERSION 3.8)
project(os_2018)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS -pthread)


### ex3 ###
#set(ATOMIC ex3/atomicdemo/atomicdemo.cpp)
#
#set(FRAMEWORK ex3/MapReduceFramework.cpp ex3/MapReduceFramework.h ex3/MapReduceClient.h ex3/Barrier.cpp
#        ex3/Barrier.h)
#
##set(TEST_2 ex3/MapReduceFramework.cpp ex3/MapReduceFramework.h ex3/MapReduceClient.h ex3/Barrier.cpp
##        ex3/Barrier.h ex3/test_2.cpp)
#
#set(EUROVISION ex3/MapReduceFramework.cpp ex3/MapReduceFramework.h ex3/MapReduceClient.h ex3/Barrier.cpp
#        ex3/Barrier.h )
#
#add_executable(atomic_demo ${ATOMIC})
#add_executable(barrier_demo ${BARRIER})
#add_executable(sample_client ${FRAMEWORK} ex3/SampleClient.cpp)
#add_executable(eurovision ${FRAMEWORK} ex3/eurovisionClient.cpp)
#add_executable(little ${FRAMEWORK} ex3/littleClient.cpp)
#add_executable(big ${FRAMEWORK} ex3/bigClient.cpp)

### ex4 ###
set(WHATSAPP ex4/whatsappServer.cpp ex4/whatsappio.cpp ex4/whatsappClient.cpp)

add_executable(whatsappSerrver ${WHATSAPP})

add_executable(tester ex4/test.cpp)