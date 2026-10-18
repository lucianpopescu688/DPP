cmake_minimum_required(VERSION 3.20)
project(ConvolutionMatrixC__)

# C++20 is required for std::jthread
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ConvolutionDynamic main.cpp)
add_executable(ConvolutionStatic main_static.cpp)
add_executable(ConvolutionMatrixC__ main.cpp)
