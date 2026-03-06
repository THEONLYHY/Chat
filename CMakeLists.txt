cmake_minimum_required(VERSION 3.5)
project(Chat)

#
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

aux_source_directory(. SRC_LIST)

add_library(chat SHARED ${SRC_LIST})

target_include_directories(chat PUBLIC ${PROJECT_SOURCE_DIR})

add_executable(test_eventloop examples/test_eventloop.cc)
target_link_libraries(test_eventloop chat pthread)
target_include_directories(test_eventloop PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(test_runinloop examples/test_runinloop.cc)
target_link_libraries(test_runinloop chat pthread)
target_include_directories(test_runinloop PRIVATE ${PROJECT_SOURCE_DIR})