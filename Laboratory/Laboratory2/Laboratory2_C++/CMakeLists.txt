cmake_minimum_required(VERSION 3.20)
project(Laboratory2_C__)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")

# Target 1: Full In-Place Engine (Variants 1b, 1f, 2b, 2f with O(M) aux vectors)
add_executable(Laboratory2_C__ main.cpp)
add_executable(Lab2InPlace main.cpp)

# Target 2: User's Standalone Variant 2B
add_executable(Lab2Simple main_simple.cpp)
