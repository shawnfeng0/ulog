add_executable(ulog_mpsc_benchmarks mpsc_benchmarks.cc)
target_link_libraries(ulog_mpsc_benchmarks GTest::gtest_main ulog)

