cmake_minimum_required(VERSION 3.20)
project(ConvolutionMatrixC__)

set(CMAKE_CXX_STANDARD 17)

add_executable(ConvolutionDynamic main.cpp)
add_executable(ConvolutionStatic main_static.cpp)
add_executable(ConvolutionMatrixC__ main.cpp)
