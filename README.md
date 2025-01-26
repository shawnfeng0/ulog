# ulog

![Tests](https://github.com/ShawnFeng0/ulog/actions/workflows/tests.yml/badge.svg)

A library written in C/C ++ for printing logs of lightweight embedded devices.

## Platforms and Dependent

* Current version only supports unix-like platforms.
* LOGGER_TOKEN and LOGGER_MULTI_TOKEN requires C++11 or GCC extension support, but other functions are not needed.

## Features

* Different log levels
* Log color identification
* Print token (any basic type is automatically recognized and printed)
* Hex dump (print the hexadecimal content in the specified address)
* Statistics code running time

## Quick Demo

### Code

```C++
#include "ulog/ulog.h"

#include <stdio.h>
#include <time.h>

static int put_str(void *user_data, const char *str) {
  user_data = user_data; // unused
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
  return printf("%s", str);
#else
  return 0;  // Need to implement a function to put string
#endif
}

int main() {
  // Initial logger
  logger_set_output_callback(ULOG_GLOBAL, NULL, put_str);

  double pi = 3.14159265;
  // Different log levels
  LOGGER_TRACE("PI = %.3f", pi);
  LOGGER_DEBUG("PI = %.3f", pi);
  LOGGER_INFO("PI = %.3f", pi);
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

  return 0;
}

```

### Output

![example](doc/figures/example.png)

## Build and integration into the project

```bash
git submodule add https://github.com/ShawnFeng0/ulog.git
```

1. As a submodule or [Download](https://github.com/ShawnFeng0/ulog/archive/master.zip) the entire project.

If you use cmake (recommended):

2. Configure the following in the cmakelists.txt of the main project:

```cmake
add_subdirectory(ulog)
target_link_libraries(${PROJECT_NAME} PUBLIC ulog) # Link libulog.a and include ulog/include path
```

or:

2. Add **ulog/include** to the include path of your project

3. Add ulog / src / ulog.c to the project's source file list

## Install (Optional, Only in Unix)

If you want to install this library system-wide, you can do so via

```bash
mkdir build && cd build
cmake ../
sudo make install
```

## Document

* [How to use ulog to print logs(API usage overview)](doc/api_usage_overview.md)

## Extension tools

### logroller

[logroller](tools/logroller) is a command-line tool for circularly writing the output of other programs to some files via a pipe ("|").

It is written using ulog's asynchronous fifo and extended headers for file loop writing.

Usage:

```bash
your_program | logroller --file-path /tmp/example/log.txt --file-size=100000 --max-files=3
```
