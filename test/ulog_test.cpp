#include "ulog/ulog.h"

#include <stdio.h>
#include <time.h>

static uint64_t get_time_us() {
#if defined(WIN32)
  struct timespec tp = {0, 0};
  (void)timespec_get(&tp, TIME_UTC);
  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
#elif defined(__unix__) || defined(__APPLE__)
  struct timespec tp = {0, 0};
  clock_gettime(CLOCK_REALTIME, &tp);
  return static_cast<uint64_t>(tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000);
#else
  // Need to implement a function to get time
  return 0;
#endif
}

static int put_str(const char *str) {
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
  return printf("%s", str);
#else
  return 0;  // Need to implement a function to put string
#endif
}

int main() {
  // Initial logger
  logger_set_time_callback(get_time_us);
  logger_init(put_str);

  double pi = 3.14159265;
  // Different log levels
  LOG_TRACE("PI = %.3f", pi);
  LOG_DEBUG("PI = %.3f", pi);
  LOG_INFO("PI = %.3f", pi);
  LOG_WARN("PI = %.3f", pi);
  LOG_ERROR("PI = %.3f", pi);
  LOG_FATAL("PI = %.3f", pi);
  LOG_RAW("PI = %.3f\r\n", pi);

  // Output debugging expression
  LOG_TOKEN(pi);
  LOG_TOKEN(50 * pi / 180);
  LOG_TOKEN(&pi);  // print address of pi

  char *text = (char *)"Ulog is a micro log library.";
  LOG_TOKEN((char *)text);

  // Hex dump
  LOG_HEX_DUMP(text, 45, 16);

  // Output multiple tokens to one line
  time_t now = 1577259816;
  struct tm lt = *localtime(&now);

  LOG_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  LOG_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);

  // Output execution time of some statements
  LOG_TIME_CODE(

      uint32_t n = 1000 * 1000; while (n--);

  );

  return 0;
}
