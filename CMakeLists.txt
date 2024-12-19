cmake_minimum_required(VERSION 3.10)
project(depsRetriever)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} dbghelp)
target_include_directories(${PROJECT_NAME} PRIVATE extern/includes)
target_compile_options(${PROJECT_NAME} PRIVATE -Wpedantic -Wall -Wextra)
target_compile_options(${PROJECT_NAME} PRIVATE -static)
target_link_options(${PROJECT_NAME} PRIVATE -static)
