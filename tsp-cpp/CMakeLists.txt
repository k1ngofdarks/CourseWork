cmake_minimum_required(VERSION 3.22.1)
project(tsp)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

add_subdirectory(src)

add_custom_target(
   extern_lib
   COMMAND make
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../LKH-2.0.11
)

add_dependencies(tsp extern_lib)