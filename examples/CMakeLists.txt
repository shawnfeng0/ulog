# ulog example: Asynchronous output
add_executable(ulog_example_asyn unix/ulog_example_asyn.cc)
target_link_libraries(ulog_example_asyn ulog pthread)

# ulog example: Asynchronous output to file
add_executable(ulog_example_rotate_file unix/ulog_example_rotate_file.cc)
target_link_libraries(ulog_example_rotate_file ulog pthread)
