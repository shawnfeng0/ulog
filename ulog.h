#ifndef __ULOG_H__
#define __ULOG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define _STR_COLOR(color) "\x1b[" color "m"

#define STR_RESET _STR_COLOR("0")
#define STR_GRAY _STR_COLOR("38;5;8")

#define STR_BLACK _STR_COLOR("0;30")
#define STR_RED _STR_COLOR("0;31")
#define STR_GREEN _STR_COLOR("0;32")
#define STR_YELLOW _STR_COLOR("0;33")
#define STR_BLUE _STR_COLOR("0;34")
#define STR_PURPLE _STR_COLOR("0;35")
#define STR_SKYBLUE _STR_COLOR("0;36")
#define STR_WHITE _STR_COLOR("0;37")

#define STR_BOLD_BLACK _STR_COLOR("30;1")
#define STR_BOLD_RED _STR_COLOR("31;1")
#define STR_BOLD_GREEN _STR_COLOR("32;1")
#define STR_BOLD_YELLOW _STR_COLOR("33;1")
#define STR_BOLD_BLUE _STR_COLOR("34;1")
#define STR_BOLD_PURPLE _STR_COLOR("35;1")
#define STR_BOLD_SKYBLUE _STR_COLOR("36;1")
#define STR_BOLD_WHITE _STR_COLOR("37;1")

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#define __FILENAME__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#if !defined(ULOG_DISABLE)
#define _LOGGER_LOG(level, ...)                                             \
  do {                                                                      \
    snprintf(0, 0, __VA_ARGS__);                                            \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while (0)
#else
#define _LOGGER_LOG(level, ...)
#endif

#define LOG_VERBOSE(fmt, ...) _LOGGER_LOG(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _LOGGER_LOG(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _LOGGER_LOG(ULOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _LOGGER_LOG(ULOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _LOGGER_LOG(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_ASSERT(fmt, ...) _LOGGER_LOG(ULOG_ASSERT, fmt, ##__VA_ARGS__)
#define _LOG_DEBUG(...) \
  logger_log(ULOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#if !defined(ULOG_DISABLE)

#ifdef __cplusplus
#include <typeinfo>
#define _TYPE_CMP(X, Y) (typeid(X) == typeid(Y))
#else
#define _TYPE_CMP(X, Y) __builtin_types_compatible_p(typeof(X), typeof(Y))
#endif

#define _LOG_TOKEN_FORMAT(prefix, suffix)                                 \
  logger_color_is_enabled() ? STR_GREEN prefix " " STR_BLUE "%s " STR_RED \
                                               "=> " STR_GREEN suffix     \
                            : prefix " %s => " suffix

#define LOG_TOKEN(token)                                                      \
  do {                                                                        \
    if (_TYPE_CMP(token, float) || _TYPE_CMP(token, double)) {                \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(float)", "%f"), #token, token);          \
      /* Types with symbols below 64 bits */                                  \
    } else if (_TYPE_CMP(token, char) || _TYPE_CMP(token, unsigned char) ||   \
               _TYPE_CMP(token, short) || _TYPE_CMP(token, unsigned short) || \
               _TYPE_CMP(token, int) || _TYPE_CMP(token, unsigned int) ||     \
               _TYPE_CMP(token, long) || _TYPE_CMP(token, long long)) {       \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(int)", "%" PRId64), #token,              \
                 (int64_t)(token));                                           \
      /* May be a 64-bit type */                                              \
    } else if (_TYPE_CMP(token, unsigned long) ||                             \
               _TYPE_CMP(token, unsigned long long)) {                        \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(int)", "%" PRIu64), #token,              \
                 (uint64_t)(token));                                          \
    } else if (_TYPE_CMP(token, char *) || _TYPE_CMP(token, const char *) ||  \
               _TYPE_CMP(token, unsigned char *) ||                           \
               _TYPE_CMP(token, const unsigned char *) ||                     \
               _TYPE_CMP(token, char[]) || _TYPE_CMP(token, const char[]) ||  \
               _TYPE_CMP(token, unsigned char[]) ||                           \
               _TYPE_CMP(token, const unsigned char[])) {                     \
      const char *token_value = (const char *)(uint64_t)(token);              \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(char*)[%" PRIu32 "]", "%s"),             \
                 (uint32_t)strlen(token_value), #token, token_value);         \
    } else if (_TYPE_CMP(token, void *) || _TYPE_CMP(token, short *) ||       \
               _TYPE_CMP(token, unsigned short *) ||                          \
               _TYPE_CMP(token, int *) || _TYPE_CMP(token, unsigned int *) || \
               _TYPE_CMP(token, long *) ||                                    \
               _TYPE_CMP(token, unsigned long *) ||                           \
               _TYPE_CMP(token, long long *) ||                               \
               _TYPE_CMP(token, unsigned long long *) ||                      \
               _TYPE_CMP(token, float *) || _TYPE_CMP(token, double *)) {     \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(void *)", "%" PRIx64), #token,           \
                 (uint64_t)(token));                                          \
    } else {                                                                  \
      _LOG_DEBUG(_LOG_TOKEN_FORMAT("(none)", "(unknown)"), #token);           \
    }                                                                         \
  } while (0)

#define _LOG_TIME_FUNCTION_LENGTH 50
#define _LOG_TIME_FORMAT                                                    \
  logger_color_is_enabled() ? STR_GREEN                                     \
      "time " STR_RED "{ " STR_BLUE "%s%s " STR_RED "} => " STR_GREEN "%fs" \
                            : "time { %s%s } => %fs"

#define LOG_TIME_CODE(...)                                                   \
  do {                                                                       \
    struct timespec tsp1 = {0, 0};                                           \
    struct timespec tsp2 = {0, 0};                                           \
    logger_get_time(&tsp1);                                                  \
    __VA_ARGS__;                                                             \
    logger_get_time(&tsp2);                                                  \
    float timediff =                                                         \
        (tsp2.tv_sec - tsp1.tv_sec) +                                        \
        (float)(tsp2.tv_nsec - tsp1.tv_nsec) / (1000 * 1000 * 1000);         \
    char function_str[_LOG_TIME_FUNCTION_LENGTH];                            \
    memset(function_str, 0, _LOG_TIME_FUNCTION_LENGTH);                      \
    strncpy(function_str, #__VA_ARGS__, _LOG_TIME_FUNCTION_LENGTH - 1);      \
    LOG_DEBUG(_LOG_TIME_FORMAT, function_str,                                \
              strncmp(#__VA_ARGS__, function_str, _LOG_TIME_FUNCTION_LENGTH) \
                  ? "..."                                                    \
                  : "",                                                      \
              timediff);                                                     \
  } while (0)

#else

#define LOG_TOKEN(token)
#define LOG_TIME_CODE(function)

#endif  // !defined(ULOG_DISABLE)

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*LogOutputCb)(const char *ptr);
typedef int (*LogMutexUnlock)(void *mutex);
typedef int (*LogMutexLock)(void *mutex);
typedef int (*LogGetTime)(struct timespec *tp);

typedef enum {
  ULOG_VERBOSE = 0,
  ULOG_DEBUG,
  ULOG_INFO,
  ULOG_WARN,
  ULOG_ERROR,
  ULOG_ASSERT,
  ULOG_LEVEL_NUMBER
} LogLevel;

void logger_enable_output(bool enable);
void logger_enable_color(bool enable);
bool logger_color_is_enabled();
void logger_enable_number_output(bool enable);
void logger_enable_time_output(bool enable);
void logger_enable_level_output(bool enable);
void logger_enable_file_line_output(bool enable);
void logger_enable_function_output(bool enable);
void logger_set_output_level(LogLevel level);
void logger_set_mutex_lock(void *mutex, LogMutexLock mutex_lock_cb,
                           LogMutexUnlock mutex_unlock_cb);
void logger_set_time_callback(LogGetTime get_time_cb);
void logger_init(LogOutputCb output_cb);
void logger_get_time(struct timespec *tp);
void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
