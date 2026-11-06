cmake_minimum_required(VERSION 3.15)
project(kvdb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wconversion)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# Core library
add_library(kvdb_core STATIC
    src/storage/format.cpp
    src/storage/file.cpp
    src/storage/index.cpp
    src/storage/storage_engine.cpp
    src/recovery/recovery.cpp
    src/concurrency/compactor.cpp
    src/database.cpp
)

target_include_directories(kvdb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI executable
add_executable(kvdb
    src/cli/main.cpp
    src/cli/batch_parser.cpp
)
target_link_libraries(kvdb PRIVATE kvdb_core)

# Torture testing tool
add_executable(torture_tool
    tests/torture/torture_main.cpp
)
target_link_libraries(torture_tool PRIVATE kvdb_core)

# Basic unit test
add_executable(unit_tests
    tests/unit/basic_test.cpp
)
target_link_libraries(unit_tests PRIVATE kvdb_core)

# Storage engine unit & scale tests
add_executable(storage_tests
    tests/unit/storage_test.cpp
)
target_link_libraries(storage_tests PRIVATE kvdb_core)
