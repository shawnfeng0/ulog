cmake_minimum_required(VERSION 3.10)
project(ulog_project)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set_property(TARGET ulog_test PROPERTY C_STANDARD 99)
#set_property(TARGET ulog_test PROPERTY CXX_STANDARD 11)
#set_property(TARGET ulog_test PROPERTY CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ADD_DEFINITIONS(-DULOG_V1_COMPATIBLE)

include_directories(include)

# ulog library
add_library(ulog src/ulog.c)

# ulog build test
add_executable(ulog_test test/ulog_test.cpp)
target_link_libraries(ulog_test ulog)

# ulog example: Asynchronous output
add_executable(ulog_example_asyn example/unix/ulog_example_asyn.cpp)
target_link_libraries(ulog_example_asyn ulog pthread)

# ulog example: Asynchronous output to file
add_executable(ulog_example_rotate_file example/unix/ulog_example_rotate_file.cpp)
target_link_libraries(ulog_example_rotate_file ulog pthread)

set_property(TARGET ulog PROPERTY PUBLIC_HEADER include/ulog/ulog.h include/ulog/ulog_private.h)

install(TARGETS ulog
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/ulog)

enable_testing()
add_test(test ulog_test)
