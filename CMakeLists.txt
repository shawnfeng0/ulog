project(ulog_project)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ULOG_BUILD_EXAMPLES "Build examples" OFF)
option(ULOG_BUILD_TESTS "Build tests" OFF)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(include)

# ulog library
add_library(ulog src/ulog.c)

if (ULOG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (ULOG_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# install
install(TARGETS ulog
        ARCHIVE DESTINATION lib)
file(GLOB ULOG_BASE_HEADERS include/ulog/*.h)
install(FILES ${ULOG_BASE_HEADERS} DESTINATION include/ulog)
