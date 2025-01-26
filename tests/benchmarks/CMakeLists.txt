add_executable(ulog_benchmarks mpsc_benchmarks.cc)
target_link_libraries(ulog_benchmarks GTest::gtest_main ulog)
