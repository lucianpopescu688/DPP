cmake_minimum_required(VERSION 4.3)
project(VectorsAddC__)

set(CMAKE_CXX_STANDARD 20)

add_executable(VectorsAddC__ main.cpp)

# Statically link standard libraries to avoid missing DLL errors when running outside of CLion
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(VectorsAddC__ PRIVATE "-static-libgcc" "-static-libstdc++")
endif()
