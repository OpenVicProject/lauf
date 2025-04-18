# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.8)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()
project(lauf VERSION 0.0.0 LANGUAGES C CXX)

option(LAUF_DISPATCH_JUMP_TABLE "whether or not to use a jump table for dispatching bytecode instructions" ON)

add_subdirectory(src)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
        cmake_policy(SET CMP0135 NEW)
    endif()

    option(LAUF_BUILD_BENCHMARKS "whether or not the benchmarks should be built" ON)
    option(LAUF_BUILD_TOOLS      "whether or not the tools should be built" ON)
    option(LAUF_BUILD_TESTS      "whether or not tests should be built" ON)

    if(LAUF_BUILD_TESTS AND NOT LAUF_BUILD_TOOLS)
        message(WARNING "integration tests require tools")
    endif()

    if(LAUF_BUILD_BENCHMARKS)
        add_subdirectory(benchmarks)
    endif()
    if(LAUF_BUILD_TOOLS OR LAUF_BUILD_TESTS)
        add_subdirectory(tools)
    endif()
    if(LAUF_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

add_executable(lauf_prototype EXCLUDE_FROM_ALL example.cpp)
target_link_libraries(lauf_prototype PRIVATE foonathan::lauf)

