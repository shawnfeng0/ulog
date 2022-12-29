cmake_minimum_required(VERSION 3.5)

project(ulog)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ULOG_BUILD_EXAMPLES "Build examples" OFF)
option(ULOG_BUILD_TESTS "Build tests" OFF)
option(ULOG_BUILD_TOOLS "Build tools" ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_compile_options(-Wextra -Wall)

# ulog library
add_library(ulog src/ulog.c)
target_include_directories(ulog PUBLIC include)

if (ULOG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (ULOG_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

if (ULOG_BUILD_TOOLS)
    add_subdirectory(tools)
endif ()

# install
install(TARGETS ulog
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/ulog DESTINATION include)
