cmake_minimum_required(VERSION 2.8.8)

project(pa C)

add_definitions(-D_GNU_SOURCE -Wall -Wno-gnu -Wextra)

add_executable(test_pa src/pa_core.c test/test_pa.c)
target_link_libraries(test_pa ubox)

