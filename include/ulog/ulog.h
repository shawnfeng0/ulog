#ifndef __ULOG_H__
#define __ULOG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ulog_private.h"

/**
 * Different levels of log output
 * @param fmt Format of the format string
 * @param ... Parameters in the format
 */
#define LOGGER_TRACE(fmt, ...) _OUT_LOG(ULOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LOGGER_DEBUG(fmt, ...) _OUT_LOG(ULOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGGER_INFO(fmt, ...) _OUT_LOG(ULOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGGER_WARN(fmt, ...) _OUT_LOG(ULOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOGGER_ERROR(fmt, ...) _OUT_LOG(ULOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGGER_FATAL(fmt, ...) _OUT_LOG(ULOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define LOGGER_RAW(fmt, ...) _OUT_RAW(fmt, ##__VA_ARGS__)

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
#define LOGGER_TOKEN(token) _OUT_TOKEN(token, _OUT_TOKEN_CB, true)

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
#define LOGGER_MULTI_TOKEN(...) _OUT_MULTI_TOKEN(__VA_ARGS__)

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
#define LOGGER_TIME_CODE(...) _TIME_CODE(__VA_ARGS__)

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
#define LOGGER_HEX_DUMP(data, length, width) _HEX_DUMP(data, length, width)

typedef int (*LogOutput)(void *private_data, const char *ptr);
typedef int (*LogMutexUnlock)(void *mutex);
typedef int (*LogMutexLock)(void *mutex);
typedef uint64_t (*LogGetTimeUs)(void);
typedef void (*LogAssertHandlerCb)(void);

typedef enum LogTimeFormat {
  LOG_TIME_FORMAT_TIMESTAMP,
  LOG_TIME_FORMAT_LOCAL_TIME,
} LogTimeFormat;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enable log output, which is enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_output(bool enable);

/**
 * Enable color output, which is enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_color(bool enable);

/**
 * Determine if the current color output is on
 * @return True is enabled, false is disabled
 */
bool logger_color_is_enabled(void);

/**
 * Enable log number output, disabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_number_output(bool enable);

/**
 * Enable log time output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_time_output(bool enable);

#if defined(_LOG_UNIX_LIKE_PLATFORM)
/**
 * Enable process and thread id output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_process_id_output(bool enable);
#endif

/**
 * Enable log level output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_level_output(bool enable);

/**
 * Enable log file line output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_file_line_output(bool enable);

/**
 * Enable log function name output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_function_output(bool enable);

/**
 * Set the log level. Logs below this level will not be output. The default
 * level is the lowest level, so logs of all levels are output.
 * @param level Minimum log output level
 */
void logger_set_output_level(LogLevel level);

#if !defined(_LOG_UNIX_LIKE_PLATFORM)
/**
 * Set the log mutex. The log library uses the same buffer and log number
 * variable, so be sure to set this if you use it in different threads. It
 * should be set before the log library is used.
 * @param mutex Mutex pointer
 * @param mutex_lock_cb Mutex lock callback
 * @param mutex_unlock_cb Mutex unlock callback
 */
void logger_set_mutex_lock(void *mutex, LogMutexLock mutex_lock_cb,
                           LogMutexUnlock mutex_unlock_cb);
#endif

/**
 * Set the callback to get the time. Generally, the system startup time is used
 * as the return value.
 * @param get_time_us_cb Callback function to get time
 */
void logger_set_time_callback(LogGetTimeUs get_time_us_cb);

/**
 * Set time output format
 * @param time_format
 * LOG_TIME_FORMAT_TIMESTAMP: Output like this: 1576886405.225
 * LOG_TIME_FORMAT_LOCAL_TIME: Output like this: 2019-01-01 17:45:22.564
 */
void logger_set_time_format(LogTimeFormat time_format);

/**
 * Initialize the logger and set the string output callback function. The
 * simplest configuration is just to configure the output callback.
 * @param private_data Set by the user, each output will be passed to output_cb,
 * output can be more flexible.
 * @param output_cb Callback function to output string
 */
void logger_init(void *private_data, LogOutput output_cb);

/**
 * Call time callback function to get time
 * @return Returns the system startup time, in microseconds. If no callback
 * function is configured, the return value is 0.
 */
uint64_t logger_get_time_us(void);

#ifdef ULOG_V1_COMPATIBLE
#define LOG_TRACE LOGGER_TRACE
#define LOG_DEBUG LOGGER_DEBUG
#define LOG_INFO LOGGER_INFO
#define LOG_WARN LOGGER_WARN
#define LOG_ERROR LOGGER_ERROR
#define LOG_FATAL LOGGER_FATAL

#define LOG_RAW LOGGER_RAW

#define LOG_TOKEN LOGGER_TOKEN
#define LOG_MULTI_TOKEN LOGGER_MULTI_TOKEN

#define LOG_TIME_CODE LOGGER_TIME_CODE

#define LOG_HEX_DUMP LOGGER_HEX_DUMP
#endif

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
