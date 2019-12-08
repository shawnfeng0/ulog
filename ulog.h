#ifndef __ULOG_H__
#define __ULOG_H__

#include "ulog_common.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * Different levels of log output
 * @param fmt Format of the format string
 * @param ... Parameters in the format
 */
#define LOG_VERBOSE(fmt, ...) _LOGGER_LOG(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _LOGGER_LOG(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _LOGGER_LOG(ULOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _LOGGER_LOG(ULOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _LOGGER_LOG(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_ASSERT(fmt, ...) _LOGGER_LOG(ULOG_ASSERT, fmt, ##__VA_ARGS__)
#define LOG_RAW(fmt, ...) logger_raw(fmt, ##__VA_ARGS__)

/**
 * Output various tokens
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
#define LOG_TOKEN(token) _LOG_TOKEN(token)

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
 * time { uint32_t n = 1000 * 1000; while (n--); } => 0.001315s
 */
#define LOG_TIME_CODE(...) _LOG_TIME_CODE(__VA_ARGS__)

/**
 * Display file contents in hexadecimal and ascii. Same format as "hexdump -C
 * filename"
 * example:
 *  char str1[5] = "test";
 *  char str2[10] = "1234";
 *  LOG_HEX_DUMP(&pi, 100, 16);
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
#define LOG_HEX_DUMP(data, length, width) _LOG_HEX_DUMP(data, length, width)

typedef int (*LogOutput)(const char *ptr);
typedef int (*LogMutexUnlock)(void *mutex);
typedef int (*LogMutexLock)(void *mutex);
typedef uint64_t (*LogGetTimeUs)();

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
 * Enable log number output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_number_output(bool enable);

/**
 * Enable log time output, enabled by default
 * @param enable True is enabled, false is disabled
 */
void logger_enable_time_output(bool enable);

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

/**
 * Set the callback to get the time. Generally, the system startup time is used
 * as the return value.
 * @param get_time_us_cb Callback function to get time
 */
void logger_set_time_callback(LogGetTimeUs get_time_us_cb);

/**
 * Initialize the logger and set the string output callback function. The
 * simplest configuration is just to configure the output callback.
 * @param output_cb Callback function to output string
 */
void logger_init(LogOutput output_cb);

/**
 * Call time callback function to get time
 * @return Returns the system startup time, in microseconds. If no callback
 * function is configured, the return value is 0.
 */
uint64_t logger_get_time_us(void);

/**
 * Display file contents in hexadecimal and ascii. Same format as "hexdump -C
 * filename"
 * @param data The starting address of the data to be displayed
 * @param length Display length starting from "data"
 * @param width How many bytes of data are displayed in each line
 * @param base_address Base address, the displayed address starts from this
 * value
 * @param tail_addr_out Tail address output, whether to output the last address
 * after output
 * @return
 */
uintptr_t logger_hex_dump(const void *data, size_t length, size_t width,
                          uintptr_t base_address, bool tail_addr_out);

/**
 * Raw data output, similar to printf
 * @param fmt Format of the format string
 * @param ... Parameters in the format
 */
void logger_raw(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
