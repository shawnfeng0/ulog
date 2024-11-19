add_subdirectory(benchmarks)

# ulog build test
add_executable(ulog_test ulog_test.c)
target_link_libraries(ulog_test ulog)
add_test(test_c_compile ulog_test)

add_executable(ulog_test_cpp ulog_test.cc)
target_link_libraries(ulog_test_cpp ulog)
add_test(test_cpp_compile ulog_test_cpp)

add_executable(ulog_unit_test bip_buffer_all_test.cc power_of_2_test.cc)
target_link_libraries(ulog_unit_test GTest::gtest_main ulog)
add_test(ulog_unit_test ulog_unit_test)
