add_executable(logrotate logrotate.cc cmdline.c)
target_link_libraries(logrotate ulog pthread zstd::libzstd_shared)
