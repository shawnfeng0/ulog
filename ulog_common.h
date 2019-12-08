#ifndef _ULOG_COMMON_H
#define _ULOG_COMMON_H

#include <stdbool.h>
#include <stdint.h>

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
#define __FILENAME__                \
  (strrchr(__FILE__, '/')           \
       ? strrchr(__FILE__, '/') + 1 \
       : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif

#if !defined(ULOG_DISABLE)
#define _LOGGER_LOG(level, ...)                                             \
  do {                                                                      \
    /* Causes the compiler to automatically check the format. */            \
    snprintf(0, 0, __VA_ARGS__);                                            \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while (0)
#else
#define _LOGGER_LOG(level, ...)
#endif

#if !defined(ULOG_DISABLE)

#ifdef __cplusplus
#include <typeinfo>
#define _TYPE_CMP(X, Y) (typeid(X) == typeid(Y))
#elif defined(__GNUC__) || defined(__clang__)
#define _TYPE_CMP(X, Y) __builtin_types_compatible_p(typeof(X), typeof(Y))
#else
#pragma message("LOG_TOKEN is not available, please use clang or gcc compiler.")
#define _TYPE_CMP(X, Y) false
#endif

#define _LOG_TOKEN_FORMAT(prefix, suffix)                                 \
  logger_color_is_enabled() ? STR_GREEN prefix " " STR_BLUE "%s " STR_RED \
                                               "=> " STR_GREEN suffix     \
                            : prefix " %s => " suffix
#define _LOG_DEBUG_NO_CHECK(...) \
  logger_log(ULOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define _LOG_TOKEN(token)                                                      \
  do {                                                                         \
    if (_TYPE_CMP(token, float) || _TYPE_CMP(token, double)) {                 \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(float)", "%f"), #token, token);  \
    } else if (_TYPE_CMP(token, bool)) {                                       \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(bool)", "%s"), #token,           \
                          token ? "true" : "false");                           \
      /* Types with symbols below 64 bits */                                   \
    } else if (_TYPE_CMP(token, char) || _TYPE_CMP(token, unsigned char) ||    \
               _TYPE_CMP(token, short) || _TYPE_CMP(token, unsigned short) ||  \
               _TYPE_CMP(token, int) || _TYPE_CMP(token, unsigned int) ||      \
               _TYPE_CMP(token, long) || _TYPE_CMP(token, long long)) {        \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(int)", "%" PRId64), #token,      \
                          (int64_t)(token));                                   \
      /* May be a 64-bit type */                                               \
    } else if (_TYPE_CMP(token, unsigned long) ||                              \
               _TYPE_CMP(token, unsigned long long)) {                         \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(int)", "%" PRIu64), #token,      \
                          (uint64_t)(token));                                  \
    } else if (_TYPE_CMP(token, char *) || _TYPE_CMP(token, const char *) ||   \
               _TYPE_CMP(token, unsigned char *) ||                            \
               _TYPE_CMP(token, const unsigned char *) ||                      \
               _TYPE_CMP(token, char[]) || _TYPE_CMP(token, const char[]) ||   \
               _TYPE_CMP(token, unsigned char[]) ||                            \
               _TYPE_CMP(token, const unsigned char[])) {                      \
      const char *token_value = (const char *)(uintptr_t)(token);              \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(char *)[%" PRIu32 "]", "%s"),    \
                          (uint32_t)strlen(token_value), #token, token_value); \
    } else if (_TYPE_CMP(token, void *) || _TYPE_CMP(token, short *) ||        \
               _TYPE_CMP(token, unsigned short *) ||                           \
               _TYPE_CMP(token, int *) || _TYPE_CMP(token, unsigned int *) ||  \
               _TYPE_CMP(token, long *) ||                                     \
               _TYPE_CMP(token, unsigned long *) ||                            \
               _TYPE_CMP(token, long long *) ||                                \
               _TYPE_CMP(token, unsigned long long *) ||                       \
               _TYPE_CMP(token, float *) || _TYPE_CMP(token, double *)) {      \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(void *)", "%" PRIx64), #token,   \
                          (uint64_t)(token));                                  \
    } else {                                                                   \
      _LOG_DEBUG_NO_CHECK(_LOG_TOKEN_FORMAT("(unknown)", "(none)"), #token);   \
    }                                                                          \
  } while (0)

#define _LOG_TIME_FUNCTION_LENGTH 50
#define _LOG_TIME_FORMAT                                                    \
  logger_color_is_enabled() ? STR_GREEN                                     \
      "time " STR_RED "{ " STR_BLUE "%s%s " STR_RED "} => " STR_GREEN "%fs" \
                            : "time { %s%s } => %fs"

#define _LOG_TIME_CODE(...)                                                  \
  do {                                                                       \
    uint64_t start_time_us = logger_get_time_us();                           \
    __VA_ARGS__;                                                             \
    uint64_t end_time_us = logger_get_time_us();                             \
    float timediff = (end_time_us - start_time_us) / 1000.f / 1000.f;        \
    char function_str[_LOG_TIME_FUNCTION_LENGTH];                            \
    memset(function_str, 0, _LOG_TIME_FUNCTION_LENGTH);                      \
    strncpy(function_str, #__VA_ARGS__, _LOG_TIME_FUNCTION_LENGTH - 1);      \
    LOG_DEBUG(_LOG_TIME_FORMAT, function_str,                                \
              strncmp(#__VA_ARGS__, function_str, _LOG_TIME_FUNCTION_LENGTH) \
                  ? "..."                                                    \
                  : "",                                                      \
              timediff);                                                     \
  } while (0)

#define _LOG_HEX_DUMP_COLOR(place1, place2, place3, place4) \
  STR_RED place1 STR_GREEN place2 STR_RED place3 STR_BLUE place4
#define _LOG_HEX_DUMP_FORMAT                                          \
  logger_color_is_enabled()                                           \
      ? _LOG_HEX_DUMP_COLOR(STR_GREEN "hex_dump(", "data", ":", "%s") \
            _LOG_HEX_DUMP_COLOR(", ", "length", ":", "%" PRIuMAX)     \
                _LOG_HEX_DUMP_COLOR(", ", "width", ":",               \
                                    "%" PRIuMAX STR_RED ") =>")       \
      : "hex_dump(data:%s, length:%" PRIuMAX ", width:%" PRIuMAX ")"
#define _LOG_HEX_DUMP(data, length, width)                       \
  do {                                                           \
    LOG_DEBUG(_LOG_HEX_DUMP_FORMAT, #data, (uintmax_t)length,    \
              (uintmax_t)width);                                 \
    logger_hex_dump(data, length, width, (uintptr_t)data, true); \
  } while (0)

#else

#define _LOG_TOKEN(token)
#define _LOG_TIME_CODE(...)

#endif  // !defined(ULOG_DISABLE)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ULOG_VERBOSE = 0,
  ULOG_DEBUG,
  ULOG_INFO,
  ULOG_WARN,
  ULOG_ERROR,
  ULOG_ASSERT,
  ULOG_LEVEL_NUMBER
} LogLevel;

void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  // _ULOG_COMMON_H
