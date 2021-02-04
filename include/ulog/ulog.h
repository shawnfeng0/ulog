#ifndef __ULOG_H__
#define __ULOG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef int (*LogOutput)(void *private_data, const char *ptr);
typedef int (*LogMutexUnlock)(void *mutex);
typedef int (*LogMutexLock)(void *mutex);
typedef uint64_t (*LogGetTimeUs)(void);

typedef enum {
  ULOG_LEVEL_TRACE = 0,
  ULOG_LEVEL_DEBUG,
  ULOG_LEVEL_INFO,
  ULOG_LEVEL_WARN,
  ULOG_LEVEL_ERROR,
  ULOG_LEVEL_FATAL,
  ULOG_LEVEL_NUMBER
} LogLevel;

#ifdef __cplusplus
extern "C" {
#endif

extern struct ulog_s *ulog_global_instance_ptr;
#define ULOG_GLOBAL ulog_global_instance_ptr

// TODO: need document
struct ulog_s *logger_create(void *private_data, LogOutput output_cb);
void logger_destroy(struct ulog_s **logger);

/**
 * Enable log output, which is enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_output(struct ulog_s *logger, bool enable);

/**
 * Enable color output, which is enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_color(struct ulog_s *logger, bool enable);

/**
 * Determine if the current color output is on
 * @return True is enabled, false is disabled
 */
bool logger_color_is_enabled(struct ulog_s *logger);

/**
 * Enable log number output, disabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_number_output(struct ulog_s *logger, bool enable);

/**
 * Enable log time output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_time_output(struct ulog_s *logger, bool enable);

/**
 * Enable process and thread id output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_process_id_output(struct ulog_s *logger, bool enable);

/**
 * Enable log level output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_level_output(struct ulog_s *logger, bool enable);

/**
 * Enable log file line output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_file_line_output(struct ulog_s *logger, bool enable);

/**
 * Enable log function name output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_function_output(struct ulog_s *logger, bool enable);

/**
 * Set the log level. Logs below this level will not be output. The default
 * level is the lowest level, so logs of all levels are output.
 * @param level Minimum log output level
 */
void logger_set_output_level(struct ulog_s *logger, LogLevel level);

/**
 * Initialize the logger and set the string output callback function. The
 * simplest configuration is just to configure the output callback.
 * @param private_data Set by the user, each output will be passed to output_cb,
 * output can be more flexible.
 * @param output_cb Callback function to output string
 */
void logger_set_output_callback(struct ulog_s *logger, void *private_data,
                                LogOutput output_cb);

/**
 * Get time of clock_id::CLOCK_MONOTONIC
 * @return Returns the system startup time, in microseconds.
 */
uint64_t logger_monotonic_time_us();

/**
 * Get time of clock_id::CLOCK_REALTIME
 * @return Returns the real time, in microseconds.
 */
uint64_t logger_real_time_us();

#ifdef __cplusplus
}
#endif

// LOG related macros ----------------------------------------------------------

#include "ulog/ulog_private.h"

/**
 * Different levels of log output
 * @param fmt Format of the format string
 * @param ... Parameters in the format
 */
#define LOGGER_TRACE(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_TRACE(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

#define LOGGER_DEBUG(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_DEBUG(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#define LOGGER_INFO(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_INFO(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

#define LOGGER_WARN(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_WARN(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_WARN, fmt, ##__VA_ARGS__)

#define LOGGER_ERROR(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_ERROR(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#define LOGGER_FATAL(fmt, ...) \
  _OUT_LOG(ULOG_GLOBAL, ULOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_FATAL(logger, fmt, ...) \
  _OUT_LOG(logger, ULOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)

#define LOGGER_RAW(fmt, ...) _OUT_RAW(ULOG_GLOBAL, fmt, ##__VA_ARGS__)
#define LOGGER_LOCAL_RAW(logger, fmt, ...) _OUT_RAW(logger, fmt, ##__VA_ARGS__)

/**
 * Output various tokens (Requires C++ 11 or GNU extension)
 * example:
 *  double pi = 3.14;
 *  LOG_TOKEN(pi);
 *  LOG_TOKEN(pi * 50.f / 180.f);
 *  LOG_TOKEN(&pi);  // print address of pi
 * output:
 *  (float) pi => 3.140000
 *  (float) pi * 50.f / 180.f => 0.872222
 *  (void *) &pi => 7fff2f5568d8
 * @param token Can be float, double, [unsigned / signed] char / short / int /
 * long / long long and pointers of the above type
 */
#define LOGGER_TOKEN(token) _OUT_TOKEN(ULOG_GLOBAL, token)
#define LOGGER_LOCAL_TOKEN(logger, token) _OUT_TOKEN(logger, token)

/**
 * Output multiple tokens to one line (Requires C++ 11 or GNU extension)
 * example:
 *  time_t now = 1577232000; // 2019-12-25 00:00:00
 *  struct tm* lt = localtime(&now);
 *  LOG_MULTI_TOKEN(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
 * output:
 *  lt->tm_year + 1900 => 2019, lt->tm_mon + 1 => 12, lt->tm_mday => 25
 * @param token Same definition as LOG_TOKEN parameter, but can output up to 16
 * tokens at the same time
 */
#define LOGGER_MULTI_TOKEN(...) _OUT_MULTI_TOKEN(ULOG_GLOBAL, __VA_ARGS__)
#define LOGGER_LOCAL_MULTI_TOKEN(logger, ...) \
  _OUT_MULTI_TOKEN(logger, __VA_ARGS__)

/**
 * Statistics code running time,
 * example:
 * LOG_TIME_CODE(
 *
 * uint32_t n = 1000 * 1000;
 * while (n--);
 *
 * );
 * output:
 * time { uint32_t n = 1000 * 1000; while (n--); } => 1315us
 */
#define LOGGER_TIME_CODE(...) _TIME_CODE(ULOG_GLOBAL, __VA_ARGS__)
#define LOGGER_LOCAL_TIME_CODE(logger, ...) _TIME_CODE(logger, __VA_ARGS__)

/**
 * Display contents in hexadecimal and ascii.
 * Same format as "hexdump -C filename"
 * example:
 *  char str1[5] = "test";
 *  char str2[10] = "1234";
 *  LOG_HEX_DUMP(&str1, 20, 16);
 * output:
 *  hex_dump(data:&str1, length:20, width:8) =>
 *  7fff2f556921  74 65 73 74  00 31 32 33  |test.123|
 *  7fff2f556929  34 00 00 00  00 00 00 30  |4......0|
 *  7fff2f556931  d3 a4 9b a7               |....|
 *  7fff2f556935
 * @param data The starting address of the data to be displayed
 * @param length Display length starting from "data"
 * @param width How many bytes of data are displayed in each line
 */
#define LOGGER_HEX_DUMP(data, length, width) \
  _HEX_DUMP(ULOG_GLOBAL, data, length, width)
#define LOGGER_LOCAL_HEX_DUMP(logger, data, length, width) \
  _HEX_DUMP(logger, data, length, width)

#endif  //__ULOG_H__
