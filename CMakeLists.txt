cmake_minimum_required(VERSION 3.5...3.31)

# Set cmake_policies
include(cmake/policy.cmake)
use_cmake_policy()

project(ulog)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ULOG_BUILD_EXAMPLES "Build examples" OFF)
option(ULOG_BUILD_TESTS "Build tests" OFF)
option(ULOG_BUILD_TOOLS "Build tools" ON)

# Generate git version info
include(cmake/git_version.cmake)
set_git_version(ULOG_GIT_TAG)
message(STATUS "ulog version: ${ULOG_GIT_TAG}")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_compile_options(-Wextra -Wall)

# ulog library
add_library(ulog src/ulog.c)
target_include_directories(ulog PUBLIC include)

if (ULOG_BUILD_EXAMPLES OR ULOG_BUILD_TOOLS)
    include(cmake/zstd.cmake)
endif ()

if (ULOG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (ULOG_BUILD_TESTS)
    include(cmake/googletest.cmake)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (ULOG_BUILD_TOOLS)
    add_subdirectory(tools)
    install(TARGETS logroller
            RUNTIME DESTINATION bin)
endif ()

# install
install(TARGETS ulog
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/ulog DESTINATION include)
