cmake_minimum_required(VERSION 3.20)
project(Laboratory2_C__)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../date.txt")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../date.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

add_executable(Laboratory2_C__ main.cpp)
add_executable(Lab2InPlace main.cpp)
