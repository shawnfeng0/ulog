# ulog

Ulog is a micro log library suitable for use with lightweight embedded devices.

## Example

### Code

```C++
#include <stdio.h>
#include <time.h>
#include "ulog/ulog.h"

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
#if defined(WIN32)
  timespec_get(&tp, TIME_UTC);
#elif defined(__unix__)
  clock_gettime(CLOCK_REALTIME, &tp);
#else
  // Need to implement a function to get time
#endif
  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
}

static int put_str(const char *str) {
#if defined(WIN32) || defined(__unix__)
  return printf("%s", str);
#else
  return 0;  // Need to implement a function to put string
#endif
}

int main() {
  double pi = 3.14159265;
  char *text = (char *)"Ulog is a micro log library.";

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

  // Hex dump
  LOG_HEX_DUMP(text, 45, 16);

  // Output execution time of some statements
  LOG_TIME_CODE(

      uint32_t n = 1000 * 1000; while (n--);

  );

  return 0;
}

```

### Output

[![2019-12-09-02-42.png](https://i.postimg.cc/FHyQSsDZ/2019-12-09-02-42.png)](https://postimg.cc/3kx657M4)
