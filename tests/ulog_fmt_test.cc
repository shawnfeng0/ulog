#include "ulog/ulog_fmt.h"

#include <stdio.h>
#include <time.h>

static int put_str(void *user_data, const char *str) {
  (void)user_data;  // unused
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
  return printf("%s", str);
#else
  return 0;
#endif
}

int main() {
  struct ulog_s *local_logger = logger_create();
  logger_set_output_callback(local_logger, put_str);
  logger_set_output_callback(ULOG_GLOBAL, put_str);

  double pi = 3.14159265;

  // Global logger – fmt-style format strings
  LOGGER_FMT_TRACE("PI = {:.3f}", pi);
  LOGGER_FMT_DEBUG("PI = {:.3f}", pi);
  LOGGER_FMT_INFO("PI = {:.3f}", pi);
  LOGGER_FMT_WARN("PI = {:.3f}", pi);
  LOGGER_FMT_ERROR("PI = {:.3f}", pi);
  LOGGER_FMT_FATAL("PI = {:.3f}", pi);
  LOGGER_FMT_RAW("PI = {:.3f}\r\n", pi);

  // Local logger
  LOGGER_FMT_LOCAL_TRACE(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_DEBUG(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_INFO(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_WARN(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_ERROR(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_FATAL(local_logger, "PI = {:.3f}", pi);
  LOGGER_FMT_LOCAL_RAW(local_logger, "PI = {:.3f}\r\n", pi);

  // Various format specifiers
  LOGGER_FMT_INFO("int={} str={} hex={:#x}", 42, "hello", 255);
  LOGGER_FMT_INFO("width {:>10}", "right");
  LOGGER_FMT_INFO("no args");

  logger_destroy(&local_logger);
  return 0;
}
