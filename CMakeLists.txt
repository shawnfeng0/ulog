cmake_minimum_required(VERSION 3.3)
project(ulog_test)

set(CMAKE_C_STANDARD 99)

#ADD_DEFINITIONS(-DULOG_CLS -DULOG_DISABLE_COLOR)
#ADD_DEFINITIONS(-DULOG_DISABLE)

add_executable(ulog_test test/ulog_test.c src/ulog.c src/ulog.h)

enable_testing()
add_test(test ulog_test)
