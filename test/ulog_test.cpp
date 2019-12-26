#include "../src/ulog.h"
#include <stdio.h>
#include <time.h>

#if defined(WIN32)

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
  (void) timespec_get(&tp, TIME_UTC);
  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
}

#elif defined(__unix__) || defined(__APPLE__)

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
}

#else

// Need to implement a function to get time
static uint64_t get_time_us() { return 0; }

#endif

static int put_str(const char *str) {
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
  return printf("%s", str);
#else
  return 0;  // Need to implement a function to put string
#endif
}

int main() {
  double pi = 3.14159265;
  char *text =
      (char *) "Ulog is a micro log library.";

  // Initial logger
  logger_set_time_callback(get_time_us);
  logger_init(put_str);

  // Different log levels
  LOG_VERBOSE("PI = %.3f", pi);
  LOG_DEBUG("PI = %.3f", pi);
  LOG_INFO("PI = %.3f", pi);
  LOG_WARN("PI = %.3f", pi);
  LOG_ERROR("PI = %.3f", pi);
  LOG_ASSERT("PI = %.3f", pi);
  LOG_RAW("PI = %.3f\r\n", pi);

  // Output debugging expression
  LOG_TOKEN(pi);
  LOG_TOKEN(50 * pi / 180);
  LOG_TOKEN(&pi);  // print address of pi
  LOG_TOKEN((char *)text);

  time_t now = 1577232000; // 2019-12-25 00:00:00
  struct tm* lt = localtime(&now);
  LOG_MULTI_TOKEN(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);

  // Hex dump
  LOG_HEX_DUMP(text, 45, 16);

  // Output execution time of some statements
  LOG_TIME_CODE(

      uint32_t n = 1000 * 1000; while (n--);

  );

  return 0;
}
