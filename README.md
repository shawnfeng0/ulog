# ulog

Ulog is a micro log library suitable for use with lightweight embedded devices.

## Example

### Code

```C++
#include "ulog/ulog.h"
#include <stdio.h>

int put_str(const char *str) { return printf("%s", str); }

int main() {
    logger_init(put_str);

    // Different log levels
    LOG_VERBOSE("This is a test text.");
    LOG_DEBUG("This is a test text.");
    LOG_INFO("This is a test text.");
    LOG_WARN("This is a test text.");
    LOG_ERROR("This is a test text.");
    LOG_ASSERT("This is a test text.");

    // Output debugging expression
    double pi = 3.14;
    LOG_TOKEN(pi);
    LOG_TOKEN(pi * 50.f / 180.f);
    LOG_TOKEN(&pi);  // print address of pi

    // Output execution time of some statements
    LOG_TIME_CODE(usleep(10 * 1000));
    LOG_TIME_CODE(__uint32_t n = 1000 * 1000; while (n--););

    return 0;
}

```

### Output

[![2019-09-12-02-19.png](https://i.postimg.cc/Df4b5C8h/2019-09-12-02-19.png)](https://postimg.cc/qg0RvxsD)
