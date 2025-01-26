add_executable(logroller logroller.cc cmdline.c)
target_link_libraries(logroller ulog pthread zstd::libzstd_shared)
