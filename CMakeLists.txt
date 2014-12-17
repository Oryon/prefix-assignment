cmake_minimum_required(VERSION 2.8.8)

project(pa C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

add_definitions(-D_GNU_SOURCE -Wall -Wno-gnu -Wextra)

OPTION(COVERAGE "build with coverage" OFF)
if (${COVERAGE})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif (${COVERAGE})

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
enable_testing()
include_directories(src)

add_executable(test_pa_core src/pa_rules.c src/btrie.c src/bitops.c src/prefix.c test/test_pa_core.c)
target_link_libraries(test_pa_core ubox)
add_test(pa_core test_pa_core)
add_dependencies(check test_pa_core)

add_executable(test_pa_filters test/test_pa_filters.c src/bitops.c src/btrie.c)
target_link_libraries(test_pa_filters ubox)
add_test(pa_filters test_pa_filters)
add_dependencies(check test_pa_filters)
