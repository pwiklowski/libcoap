project(coap)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

INCLUDE_DIRECTORIES(../std/)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror=sign-compare")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
add_library(coap SHARED ${SRC_LIST})



