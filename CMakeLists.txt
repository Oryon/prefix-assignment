cmake_minimum_required(VERSION 2.8.8)

project(pa C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

add_definitions(-D_GNU_SOURCE -Wall -Wno-gnu -Wextra)
add_definitions(-DPA_STDOUT)

add_executable(test_pa src/pa_core.c src/btrie.c src/prefix.c test/test_pa.c)
target_link_libraries(test_pa ubox)

