cmake_minimum_required(VERSION 3.10)
project(ulog_test)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set_property(TARGET ulog_test PROPERTY C_STANDARD 99)
#set_property(TARGET ulog_test PROPERTY CXX_STANDARD 11)
#set_property(TARGET ulog_test PROPERTY CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#ADD_DEFINITIONS(-DULOG_CLS -DULOG_DISABLE_COLOR)
#ADD_DEFINITIONS(-DULOG_DISABLE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

include_directories(include)

add_library(ulog src/ulog.c)

add_executable(ulog_test test/ulog_test.cpp)
target_link_libraries(ulog_test ulog)

set_property(TARGET ulog PROPERTY PUBLIC_HEADER include/ulog/ulog.h include/ulog/ulog_common.h)

install(TARGETS ulog
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/ulog)

enable_testing()
add_test(test ulog_test)
