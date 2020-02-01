cmake_minimum_required(VERSION 3.3)
project(ulog_test)

set(CMAKE_C_STANDARD 99)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#ADD_DEFINITIONS(-DULOG_CLS -DULOG_DISABLE_COLOR)
#ADD_DEFINITIONS(-DULOG_DISABLE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

add_executable(ulog_test test/ulog_test.cpp src/ulog.c src/ulog.h src/ulog_common.h)

enable_testing()
add_test(test ulog_test)
