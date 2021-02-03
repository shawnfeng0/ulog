#include "ulog/ulog.h"

#include <stdio.h>
#include <time.h>

static int put_str(void *private_data, const char *str) {
  private_data = private_data;  // unused
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
  return printf("%s", str);
#else
  return 0;  // Need to implement a function to put string
#endif
}

int main() {
  struct ulog_s *local_logger = logger_create(NULL, put_str);
  // Initial logger
  logger_set_output_callback(ULOG_GLOBAL, NULL, put_str);

  // Different log levels
  double pi = 3.14159265;

  LOGGER_TRACE("PI = %.3f", pi);
  LOGGER_LOCAL_TRACE(local_logger, "PI = %.3f", pi);

  LOGGER_DEBUG("PI = %.3f", pi);
  LOGGER_LOCAL_DEBUG(local_logger, "PI = %.3f", pi);

  LOGGER_INFO("PI = %.3f", pi);
  LOGGER_LOCAL_INFO(local_logger, "PI = %.3f", pi);

  LOGGER_WARN("PI = %.3f", pi);
  LOGGER_WARN("PI = %.3f", pi);
  LOGGER_ERROR("PI = %.3f", pi);
  LOGGER_FATAL("PI = %.3f", pi);
  LOGGER_RAW("PI = %.3f\r\n", pi);

  // Output debugging expression
  LOGGER_TOKEN(pi);
  LOGGER_TOKEN(50 * pi / 180);
  LOGGER_TOKEN(&pi);  // print address of pi

  char *text = (char *)"Ulog is a micro log library.";
  LOGGER_TOKEN((char *)text);

  // Hex dump
  LOGGER_HEX_DUMP(text, 45, 16);

  // Output multiple tokens to one line
  time_t now = 1577259816;
  struct tm lt = *localtime(&now);
  LOGGER_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  LOGGER_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);

  // Output execution time of some statements
  LOGGER_TIME_CODE(

      uint32_t n = 1000 * 1000; while (n--);

  );

  logger_destroy(&local_logger);
  return 0;
}
