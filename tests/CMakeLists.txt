# ulog build test
enable_testing()
add_executable(ulog_test ulog_test.cpp)
target_link_libraries(ulog_test ulog)
add_test(test ulog_test)