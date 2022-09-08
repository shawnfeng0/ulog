#ifndef __ULOG_H__
#define __ULOG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef int (*ulog_output_callback)(void *user_data, const char *ptr);
typedef void (*ulog_flush_callback)(void *user_data);

enum ulog_level_e {
  ULOG_LEVEL_TRACE = 0,
  ULOG_LEVEL_DEBUG,
  ULOG_LEVEL_INFO,
  ULOG_LEVEL_WARN,
  ULOG_LEVEL_ERROR,
  ULOG_LEVEL_FATAL,
  ULOG_LEVEL_NUMBER,

  // Raw data level means output is always available
  ULOG_LEVEL_RAW = ULOG_LEVEL_NUMBER
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct ulog_s *ulog_global_logger;
#define ULOG_GLOBAL ulog_global_logger

/**
 * Create a logger instance
 * @return Return a new ulog instance
 */
struct ulog_s *logger_create(void);

/**
 * Destroy logger instance
 * @param logger_ptr Pointer to logger pointer, it will be set to NULL pointer
 */
void logger_destroy(struct ulog_s **logger_ptr);

/**
 * Set user data, each output will be passed to output_callback/flush_callback,
 * making the output more flexible.
 *
 * @param user_data The user data pointer.
 */
void logger_set_user_data(struct ulog_s *logger, void *user_data);

/**
 * Set the callback function for log output, the log is output through this
 * function
 *
 * @param logger
 * @param output_callback Callback function to output string
 */
void logger_set_output_callback(struct ulog_s *logger,
                                ulog_output_callback output_callback);

/**
 * Set the callback function of log flush, which is executed when the log level
 * is error
 *
 * @param logger
 * @param flush_callback Callback function to flush the log
 */
void logger_set_flush_callback(struct ulog_s *logger,
                               ulog_flush_callback flush_callback);

/*****************************************************************************
 * log format configuration:
 */

/**
 * Set the minimum log level. Logs below this level will not be output. The
 * default level is the lowest level, so logs of all levels are output.
 */
void logger_set_output_level(struct ulog_s *logger, enum ulog_level_e level);

/**
 * Enable or disable log output, which is enabled by default
 */
void logger_enable_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable color output, which is enabled by default
 */
void logger_enable_color(struct ulog_s *logger, bool enable);

/**
 * Enable or disable log number output, disabled by default
 */
void logger_enable_number_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable log time output, enabled by default
 */
void logger_enable_time_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable process and thread id output, enabled by default
 */
void logger_enable_process_id_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable log level output, enabled by default
 */
void logger_enable_level_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable log file line output, enabled by default
 */
void logger_enable_file_line_output(struct ulog_s *logger, bool enable);

/**
 * Enable or disable log function name output, enabled by default
 */
void logger_enable_function_output(struct ulog_s *logger, bool enable);

#ifdef __cplusplus
}
#endif

/*****************************************************************************
 * log related macros:
 */

#include "ulog/ulog_private.h"

/**
 * Different levels of log output
 * @param fmt Format of the format string
 * @param ... Parameters in the format
 */
#define LOGGER_LOCAL_TRACE(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LOGGER_TRACE(fmt, ...) \
  LOGGER_LOCAL_TRACE(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_DEBUG(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGGER_DEBUG(fmt, ...) \
  LOGGER_LOCAL_DEBUG(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_INFO(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGGER_INFO(fmt, ...) LOGGER_LOCAL_INFO(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_WARN(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOGGER_WARN(fmt, ...) LOGGER_LOCAL_WARN(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_ERROR(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGGER_ERROR(fmt, ...) \
  LOGGER_LOCAL_ERROR(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_FATAL(logger, fmt, ...) \
  ULOG_OUT_LOG(logger, ULOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define LOGGER_FATAL(fmt, ...) \
  LOGGER_LOCAL_FATAL(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

#define LOGGER_LOCAL_RAW(logger, fmt, ...) \
  ULOG_OUT_RAW(logger, ULOG_LEVEL_RAW, fmt, ##__VA_ARGS__)
#define LOGGER_RAW(fmt, ...) LOGGER_LOCAL_RAW(ULOG_GLOBAL, fmt, ##__VA_ARGS__)

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
#define LOGGER_LOCAL_TOKEN(logger, token) ULOG_OUT_TOKEN(logger, token)
#define LOGGER_TOKEN(token) LOGGER_LOCAL_TOKEN(ULOG_GLOBAL, token)

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
#define LOGGER_LOCAL_MULTI_TOKEN(logger, ...) \
  ULOG_OUT_MULTI_TOKEN(logger, __VA_ARGS__)
#define LOGGER_MULTI_TOKEN(...) \
  LOGGER_LOCAL_MULTI_TOKEN(ULOG_GLOBAL, __VA_ARGS__)

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
#define LOGGER_LOCAL_TIME_CODE(logger, ...) ULOG_TIME_CODE(logger, __VA_ARGS__)
#define LOGGER_TIME_CODE(...) LOGGER_LOCAL_TIME_CODE(ULOG_GLOBAL, __VA_ARGS__)

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
#define LOGGER_LOCAL_HEX_DUMP(logger, data, length, width) \
  ULOG_HEX_DUMP(logger, data, length, width)
#define LOGGER_HEX_DUMP(data, length, width) \
  LOGGER_LOCAL_HEX_DUMP(ULOG_GLOBAL, data, length, width)

#endif  //__ULOG_H__
