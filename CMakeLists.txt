cmake_minimum_required(VERSION 3.16)
project(Lab1 LANGUAGES CXX)

add_executable(Lab1 Lab1.cpp)
set_target_properties(Lab1 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(Lab1 PRIVATE /W4 /utf-8)
else()
    target_compile_options(Lab1 PRIVATE -Wall -Wextra -pedantic)
endif()
