#ifndef _ULOG_COMMON_H
#define _ULOG_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#if defined(__unix__) || defined(__APPLE__)
#define _LOG_UNIX_LIKE_PLATFORM 1
#endif

#if defined(WIN32) || defined(_LOG_UNIX_LIKE_PLATFORM)
#define LOG_LOCAL_TIME_SUPPORT 1
#else
#define LOG_LOCAL_TIME_SUPPORT 0
#endif

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

#define _LOG_FORMAT_CHECK(...)                                   \
  do {                                                           \
    /* Causes the compiler to automatically check the format. */ \
    char c;                                                \
    snprintf(&c, 0, __VA_ARGS__);                   \
  } while (0)

#if !defined(ULOG_DISABLE)
#define _LOGGER_LOG(level, ...)                                   \
  do {                                                            \
    _LOG_FORMAT_CHECK(__VA_ARGS__);                               \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, true, \
               ##__VA_ARGS__);                                    \
  } while (0)

#define _LOGGER_RAW(fmt, ...)              \
  do {                                     \
    _LOG_FORMAT_CHECK(fmt, ##__VA_ARGS__); \
    logger_raw(fmt, ##__VA_ARGS__);        \
  } while (0)

#else

#define _LOGGER_LOG(level, ...)
#define _LOGGER_RAW(fmt, ...)

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

#define _LOG_TOKEN_FORMAT(prefix, suffix)                                      \
  logger_color_is_enabled() ? STR_GREEN prefix STR_BLUE "%s " STR_RED          \
                                                        "=> " STR_GREEN suffix \
                            : prefix "%s => " suffix

#define _LOG_DEBUG_NO_CHECK(...)                                     \
  logger_log(ULOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, true, \
             ##__VA_ARGS__)

#define _LOG_TOKEN(token, output_cb, info_out)                                 \
  do {                                                                         \
    if (_TYPE_CMP(token, float) || _TYPE_CMP(token, double)) {                 \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(float) ", "%f"), #token, token) \
               : output_cb(_LOG_TOKEN_FORMAT("", "%f"), #token, token);        \
    } else if (_TYPE_CMP(token, bool)) {                                       \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(bool) ", "%s"), #token,         \
                           token ? "true" : "false")                           \
               : output_cb(_LOG_TOKEN_FORMAT("", "%s"), #token,                \
                           token ? "true" : "false");                          \
      /* Types with symbols below 64 bits */                                   \
    } else if (_TYPE_CMP(token, char) || _TYPE_CMP(token, unsigned char) ||    \
               _TYPE_CMP(token, short) || _TYPE_CMP(token, unsigned short) ||  \
               _TYPE_CMP(token, int) || _TYPE_CMP(token, unsigned int) ||      \
               _TYPE_CMP(token, long) || _TYPE_CMP(token, long long)) {        \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(int) ", "%" PRId64), #token,    \
                           (int64_t)(token))                                   \
               : output_cb(_LOG_TOKEN_FORMAT("", "%" PRId64), #token,          \
                           (int64_t)(token));                                  \
      /* May be a 64-bit type */                                               \
    } else if (_TYPE_CMP(token, unsigned long) ||                              \
               _TYPE_CMP(token, unsigned long long)) {                         \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(int) ", "%" PRIu64), #token,    \
                           (uint64_t)(token))                                  \
               : output_cb(_LOG_TOKEN_FORMAT("", "%" PRIu64), #token,          \
                           (uint64_t)(token));                                 \
    } else if (_TYPE_CMP(token, char *) || _TYPE_CMP(token, const char *) ||   \
               _TYPE_CMP(token, unsigned char *) ||                            \
               _TYPE_CMP(token, const unsigned char *) ||                      \
               _TYPE_CMP(token, char[]) || _TYPE_CMP(token, const char[]) ||   \
               _TYPE_CMP(token, unsigned char[]) ||                            \
               _TYPE_CMP(token, const unsigned char[])) {                      \
      const char *token_value = (const char *)(uintptr_t)(token);              \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(char *)[%" PRIu32 "] ", "%s"),  \
                           (uint32_t)strlen(token_value), #token, token_value) \
               : output_cb(_LOG_TOKEN_FORMAT("", "%s"), #token, token_value);  \
    } else if (_TYPE_CMP(token, void *) || _TYPE_CMP(token, short *) ||        \
               _TYPE_CMP(token, unsigned short *) ||                           \
               _TYPE_CMP(token, int *) || _TYPE_CMP(token, unsigned int *) ||  \
               _TYPE_CMP(token, long *) ||                                     \
               _TYPE_CMP(token, unsigned long *) ||                            \
               _TYPE_CMP(token, long long *) ||                                \
               _TYPE_CMP(token, unsigned long long *) ||                       \
               _TYPE_CMP(token, float *) || _TYPE_CMP(token, double *)) {      \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(void *) ", "%" PRIx64), #token, \
                           (uint64_t)(token))                                  \
               : output_cb(_LOG_TOKEN_FORMAT("", "%" PRIx64), #token,          \
                           (uint64_t)(token));                                 \
    } else {                                                                   \
      info_out ? output_cb(_LOG_TOKEN_FORMAT("(unknown) ", "(none)"), #token)  \
               : output_cb(_LOG_TOKEN_FORMAT("", "(none)"), #token);           \
    }                                                                          \
  } while (0)

#define _EXPAND(...) __VA_ARGS__
#define EAT_COMMA(...) ,##__VA_ARGS__
/**
 * Get the number of parameters of the function-like macro
 */
#define _ARG_COUNT(...)                                                       \
  _ARG_COUNT_PRIVATE(NULL EAT_COMMA(__VA_ARGS__), 64, 63, 62, 61, 60, 59, 58, 57, 56, \
                     55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,  \
                     41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28,  \
                     27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14,  \
                     13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _ARG_COUNT_PRIVATE(...) _EXPAND(_ARG_COUNT_PRIVATE_MSVC(__VA_ARGS__))
#define _ARG_COUNT_PRIVATE_MSVC(                                                    \
    _none, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,    \
    _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
    _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, \
    _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
    _60, _61, _62, _63, _64, ...)                                              \
  _64

/**
 * String concatenation
 */
#define _MACRO_CONCAT_PRIVATE(l, r) l##r
#define _MACRO_CONCAT(l, r) _MACRO_CONCAT_PRIVATE(l, r)

#define _LOG_DEBUG_NO_NEWLINE(...)                                    \
  logger_log(ULOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, false, \
             ##__VA_ARGS__)

#define _LOG_MULTI_TOKEN(...)                                        \
  do {                                                               \
    _LOG_DEBUG_NO_NEWLINE("");                                       \
    _EXPAND(_MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)) \
  } while (0)

#define _TOKEN_OUTPUT_WRAPPER(token, left)                                 \
  do {                                                                     \
    _LOG_TOKEN(token, logger_raw, false);                                  \
    if (left) logger_raw(logger_color_is_enabled() ? STR_RED ", " : ", "); \
  } while (0)

#define _LOG_TOKEN_AUX(_1, ...)                       \
  _TOKEN_OUTPUT_WRAPPER(_1, _ARG_COUNT(__VA_ARGS__)); \
  _MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))

#define _TOKEN_AUX_0(...) logger_raw("\r\n");
#define _TOKEN_AUX_1(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_2(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_3(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_4(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_5(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_6(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_7(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_8(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_9(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_10(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_11(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_12(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_13(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_14(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_15(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_16(_1, ...) _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))

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

#define _LOG_HEX_DUMP_FORMAT                                             \
  logger_color_is_enabled()                                              \
      ? STR_GREEN "hex_dump" _LOG_HEX_DUMP_COLOR("(", "data", ":", "%s") \
            _LOG_HEX_DUMP_COLOR(", ", "length", ":", "%" PRIuMAX)        \
                _LOG_HEX_DUMP_COLOR(", ", "width", ":",                  \
                                    "%" PRIuMAX STR_RED ") =>")          \
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
  ULOG_TRACE = 0,
  ULOG_DEBUG,
  ULOG_INFO,
  ULOG_WARN,
  ULOG_ERROR,
  ULOG_FATAL,
  ULOG_LEVEL_NUMBER
} LogLevel;

void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, bool newline, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  // _ULOG_COMMON_H
