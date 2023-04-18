# ulog build test
add_executable(ulog_test ulog_test.c)
target_link_libraries(ulog_test ulog)
add_test(test_c_compile ulog_test)

add_executable(ulog_test_cpp ulog_test.cc)
target_link_libraries(ulog_test_cpp ulog)
add_test(test_cpp_compile ulog_test_cpp)
