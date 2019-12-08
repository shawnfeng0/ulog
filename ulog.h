#ifndef __ULOG_H__
#define __ULOG_H__

#include "ulog_common.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_VERBOSE(fmt, ...) _LOGGER_LOG(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _LOGGER_LOG(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _LOGGER_LOG(ULOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _LOGGER_LOG(ULOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _LOGGER_LOG(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_ASSERT(fmt, ...) _LOGGER_LOG(ULOG_ASSERT, fmt, ##__VA_ARGS__)
#define LOG_RAW(fmt, ...) logger_raw(fmt, ##__VA_ARGS__)

#define LOG_TOKEN(token) _LOG_TOKEN(token)
#define LOG_TIME_CODE(...) _LOG_TIME_CODE(__VA_ARGS__)
#define LOG_HEX_DUMP(data, length, width) _LOG_HEX_DUMP(data, length, width)

typedef int (*LogOutput)(const char *ptr);
typedef int (*LogMutexUnlock)(void *mutex);
typedef int (*LogMutexLock)(void *mutex);
typedef uint64_t (*LogGetTimeUs)();

#ifdef __cplusplus
extern "C" {
#endif

void logger_enable_output(bool enable);
void logger_enable_color(bool enable);
bool logger_color_is_enabled(void);
void logger_enable_number_output(bool enable);
void logger_enable_time_output(bool enable);
void logger_enable_level_output(bool enable);
void logger_enable_file_line_output(bool enable);
void logger_enable_function_output(bool enable);
void logger_set_output_level(LogLevel level);
void logger_set_mutex_lock(void *mutex, LogMutexLock mutex_lock_cb,
                           LogMutexUnlock mutex_unlock_cb);
void logger_set_time_callback(LogGetTimeUs get_time_us_cb);
void logger_init(LogOutput output_cb);
uint64_t logger_get_time_us(void);
uintptr_t logger_hex_dump(const void *data, size_t length, size_t width,
                          uintptr_t base_address, bool tail_addr_out);
void logger_raw(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
