# How to use ulog to print logs(API usage overview)

Please refer to the examples in the [tests](tests) or [examples](examples) folder, there are examples of asynchronous
log output written in C++ and asynchronous output to files in the [examples](examples) directory.

Detailed documentation is described in ulog.h

## 1 Initialization

Unix platform has default configuration, you can use it directly without configuration

### 1.1 Initialize the logger and set the string output callback function

The simplest configuration is just to configure the output callback.

user_data: Set by the user, each output will be passed to output_callback, output can be more flexible.

```C
// Create a logger instance, the ULOG_GLOBAL is an existing global instance. If the system uses only one logger instance, there is no need to create it again.
struct ulog_s *logger_create(void);
void logger_destroy(struct ulog_s **logger_ptr);

// Set user data, each output will be passed to output_callback/flush_callback, making the output more flexible.
void logger_set_user_data(struct ulog_s *logger, void *user_data);

// Set the callback function for log output, the log is output through this function
typedef int (*ulog_output_callback)(void *user_data, const char *ptr);
void logger_set_output_callback(struct ulog_s *logger, ulog_output_callback output_callback);

// Set the callback function of log flush, which is executed when the log level is error
typedef void (*ulog_flush_callback)(void *user_data);
void logger_set_flush_callback(struct ulog_s *logger, ulog_flush_callback flush_callback);

// sample:
static int put_str(void *user_data, const char *str) {
  user_data = user_data; // unused
  return printf("%s", str);
}
int main(int argc, char *argv[]) {
  struct ulog_s *local_logger = logger_create();

  logger_set_output_callback(local_logger, put_str);
  logger_set_output_callback(ULOG_GLOBAL, put_str);

  logger_set_flush_callback(ULOG_GLOBAL, NULL);
  logger_set_flush_callback(local_logger, NULL);

  // ...

  logger_destroy(&local_logger);
}
```

## 2 Print log

The `LOGGER_XXX(fmt, ...)` just uses `ULOG_GLOBAL` for `LOGGER_LOCAL_XXXX`
example: `#define LOGGER_TRACE(fmt, ...) LOGGER_LOCAL_TRACE(ULOG_GLOBAL, fmt, ##__VA_ARGS__)`

### 2.1 Normal log

Same format as **printf**.

```C
double pi = 3.14159265;
LOGGER_TRACE("PI = %.3f", pi); // Use a separate logger: LOGGER_LOCAL_TRACE(local_logger, "PI = %.3f", pi);
LOGGER_DEBUG("PI = %.3f", pi);
LOGGER_INFO("PI = %.3f", pi);
LOGGER_WARN("PI = %.3f", pi);
LOGGER_ERROR("PI = %.3f", pi);
LOGGER_FATAL("PI = %.3f", pi);
LOGGER_RAW("PI = %.3f\r\n", pi);

/* Output: ---------------------------------------------------------------
[2020-02-05 18:48:55.111] 14890-14890 T/(ulog_test.cpp:47 main) PI = 3.142
[2020-02-05 18:48:55.111] 14890-14890 D/(ulog_test.cpp:48 main) PI = 3.142
[2020-02-05 18:48:55.111] 14890-14890 I/(ulog_test.cpp:49 main) PI = 3.142
[2020-02-05 18:48:55.111] 14890-14890 W/(ulog_test.cpp:50 main) PI = 3.142
[2020-02-05 18:48:55.111] 14890-14890 E/(ulog_test.cpp:51 main) PI = 3.142
[2020-02-05 18:48:55.111] 14890-14890 F/(ulog_test.cpp:52 main) PI = 3.142
*/
```

### 2.2 Print variable (Requires C++ 11 or GNU extension)

Output various tokens, the function will automatically recognize the type of token and print.

```C
/*
  @param token Can be float, double, [unsigned / signed] char / short / int / long / long long and pointers of the above type
 */
#define LOGGER_LOCAL_TOKEN(logger, token) ...
#define LOGGER_TOKEN(token) LOGGER_LOCAL_TOKEN(ULOG_GLOBAL, token)
```

Output multiple tokens to one line, each parameter can be a different type

```C
/**
 * @param token Same definition as LOGGER_TOKEN parameter, but can output up to 16
 * tokens at the same time
 */
#define LOGGER_LOCAL_MULTI_TOKEN(logger, ...) ...
#define LOGGER_MULTI_TOKEN(...) LOGGER_LOCAL_MULTI_TOKEN(ULOG_GLOBAL, __VA_ARGS__)
```

Example:

```C
double pi = 3.14;
LOGGER_TOKEN(pi);
LOGGER_TOKEN(pi * 50.f / 180.f);
LOGGER_TOKEN(&pi);  // print address of pi

time_t now = 1577232000; // 2019-12-25 00:00:00
struct tm* lt = localtime(&now);
LOGGER_MULTI_TOKEN(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);

/* Output: ---------------------------------------------------------
(float) pi => 3.140000
(float) pi * 50.f / 180.f => 0.872222
(void *) &pi => 7fff2f5568d8
lt->tm_year + 1900 => 2019, lt->tm_mon + 1 => 12, lt->tm_mday => 25
*/
```

### 2.3 Hex dump

Display contents in hexadecimal and ascii. Same format as "hexdump -C filename"

```C
/*
 * @param data The starting address of the data to be displayed
 * @param length Display length starting from "data"
 * @param width How many bytes of data are displayed in each line
 */
#define LOGGER_LOCAL_HEX_DUMP(logger, data, length, width) ...
#define LOGGER_HEX_DUMP(data, length, width) LOGGER_LOCAL_HEX_DUMP(ULOG_GLOBAL, data, length, width)
```

Example:

```C
char str1[5] = "test";
char str2[10] = "1234";
LOGGER_HEX_DUMP(&str1, 20, 16);

/* Output: ---------------------------------------
hex_dump(data:&str1, length:20, width:8) =>
7fff2f556921  74 65 73 74  00 31 32 33  |test.123|
7fff2f556929  34 00 00 00  00 00 00 30  |4......0|
7fff2f556931  d3 a4 9b a7               |....|
7fff2f556935
*/
```

### 2.4 Statistics code running time

```C
/*
 * Statistics code running time
 * @param Code snippet
 */
#define LOGGER_LOCAL_TIME_CODE(logger, ...) ...
#define LOGGER_TIME_CODE(...) LOGGER_LOCAL_TIME_CODE(ULOG_GLOBAL, __VA_ARGS__)
```

Example:

```C
LOGGER_TIME_CODE(

uint32_t n = 1000 * 1000;
while (n--);

);

/* Output: -----------------------------------------------
time { uint32_t n = 1000 * 1000; while (n--); } => 1315us
*/
```

## 3 Output customization

```C
// Enable log output, which is enabled by default
void logger_enable_output(struct ulog_s *logger, bool enable);
```

```C
#define ULOG_F_COLOR 1 << 0       // Enable color output (default=on)
#define ULOG_F_NUMBER 1 << 1      // Enable line number output (default=off)
#define ULOG_F_TIME 1 << 2        // Enable time output (default=on)
#define ULOG_F_PROCESS_ID 1 << 3  // Enable process id output (default=on)
#define ULOG_F_LEVEL 1 << 4       // Enable log level output (default=on)
#define ULOG_F_FILE_LINE 1 << 5   // Enable file line output (default=on)
#define ULOG_F_FUNCTION 1 << 6    // Enable function name output (default=on)

/**
* Enable log format, can use (ULOG_F_XXX | ULOG_F_XXX) combination
*/
void logger_format_enable(struct ulog_s *logger, int32_t format);

/**
* Disable log format, can use (ULOG_F_XXX | ULOG_F_XXX) combination
*/
void logger_format_disable(struct ulog_s *logger, int32_t format);
```

```C
// Set the log level. Logs below this level will not be output
// The default level is the lowest level, so logs of all levels are output.

// Different levels:
// ULOG_LEVEL_TRACE
// ULOG_LEVEL_DEBUG
// ULOG_LEVEL_INFO
// ULOG_LEVEL_WARN
// ULOG_LEVEL_ERROR
// ULOG_LEVEL_FATAL
void logger_set_output_level(struct ulog_s *logger, enum ulog_level_e level);
```
