#ifndef _ULOG_COMMON_H
#define _ULOG_COMMON_H

#if !defined(__cplusplus)
#include <stdbool.h>
#endif
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

#define _CHECK_LOG_FORMAT(...)                                   \
  do {                                                           \
    /* Causes the compiler to automatically check the format. */ \
    char _ulog_c##__LINE__;                                      \
    snprintf(&_ulog_c##__LINE__, 0, __VA_ARGS__);                \
  } while (0)

#define _OUT_LOG(level, ...)                                            \
  do {                                                                  \
    _CHECK_LOG_FORMAT(__VA_ARGS__);                                     \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, true, true, \
               ##__VA_ARGS__);                                          \
  } while (0)

#define _OUT_RAW_WITH_LOCK(fmt, ...)       \
  do {                                     \
    _CHECK_LOG_FORMAT(fmt, ##__VA_ARGS__); \
    logger_raw(true, fmt, ##__VA_ARGS__);  \
  } while (0)

#define _OUT_RAW_LOCKED(fmt, ...) logger_raw(false, fmt, ##__VA_ARGS__)

#ifdef __cplusplus

namespace ulog {

template <typename T1, typename T2>
struct TYPE_IS_EQUAL {
  static const bool x = false;
};

template <typename T>
struct TYPE_IS_EQUAL<T, T> {
  static const bool x = true;
};

};  // namespace ulog

// "decltype" implementation in C++ 11, But msvc can be used after the 2010
// version, it is strange, but it can be used
#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER > 1600)
#define _IS_SAME_TYPE(var, type) ulog::TYPE_IS_EQUAL<decltype(var), type>::x

// "typeof" implementation in GCC extension
#elif defined(__GNUC__) || defined(__clang__)
#define _IS_SAME_TYPE(var, type) ulog::TYPE_IS_EQUAL<typeof(var), type>::x

#else
#pragma message( \
    "LOG_TOKEN is not available, plese use c++11 or clang or gcc compiler.")
#define _IS_SAME_TYPE(var, type) false
#endif  // __cplusplus >= 201103L

// C version: implemented through GCC extensionn
#elif defined(__GNUC__) || defined(__clang__)
#define _IS_SAME_TYPE(var, type) \
  __builtin_types_compatible_p(typeof(var), typeof(type))
#else
#pragma message( \
    "LOG_TOKEN is not available, plese use c++11 or clang or gcc compiler.")
#define _IS_SAME_TYPE(var, type) false
#endif

#define _GEN_TOKEN_FORMAT(prefix, suffix)                                      \
  logger_color_is_enabled() ? STR_GREEN prefix STR_BLUE "%s " STR_RED          \
                                                        "=> " STR_GREEN suffix \
                            : prefix "%s => " suffix

#define _OUT_TOKEN_CB(...)                                                 \
  logger_log(ULOG_LEVEL_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, true, \
             true, ##__VA_ARGS__)

#define _GEN_STRING_TOKEN_FORMAT(color) STR_RED "\"" color "%s" STR_RED "\""

#define _OUT_TOKEN(token, out_cb, info_out)                                   \
  do {                                                                        \
    if (_IS_SAME_TYPE(token, float) || _IS_SAME_TYPE(token, double)) {        \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(float) ", "%f"), #token, token) \
                 : out_cb(_GEN_TOKEN_FORMAT("", "%f"), #token, token);        \
    } else if (_IS_SAME_TYPE(token, bool)) {                                  \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(bool) ", "%s"), #token,         \
                          (token) ? "true" : "false")                         \
                 : out_cb(_GEN_TOKEN_FORMAT("", "%s"), #token,                \
                          (token) ? "true" : "false");                        \
      /* Types with symbols below 64 bits */                                  \
    } else if (_IS_SAME_TYPE(token, char) ||                                  \
               _IS_SAME_TYPE(token, unsigned char) ||                         \
               _IS_SAME_TYPE(token, short) ||                                 \
               _IS_SAME_TYPE(token, unsigned short) ||                        \
               _IS_SAME_TYPE(token, int) ||                                   \
               _IS_SAME_TYPE(token, unsigned int) ||                          \
               _IS_SAME_TYPE(token, long) ||                                  \
               _IS_SAME_TYPE(token, long long)) {                             \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(int) ", "%" PRId64), #token,    \
                          (int64_t)(token))                                   \
                 : out_cb(_GEN_TOKEN_FORMAT("", "%" PRId64), #token,          \
                          (int64_t)(token));                                  \
      /* May be a 64-bit type */                                              \
    } else if (_IS_SAME_TYPE(token, unsigned long) ||                         \
               _IS_SAME_TYPE(token, unsigned long long)) {                    \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(int) ", "%" PRIu64), #token,    \
                          (uint64_t)(token))                                  \
                 : out_cb(_GEN_TOKEN_FORMAT("", "%" PRIu64), #token,          \
                          (uint64_t)(token));                                 \
    } else if (_IS_SAME_TYPE(token, char *) ||                                \
               _IS_SAME_TYPE(token, const char *) ||                          \
               _IS_SAME_TYPE(token, unsigned char *) ||                       \
               _IS_SAME_TYPE(token, const unsigned char *) ||                 \
               _IS_SAME_TYPE(token, char[]) ||                                \
               _IS_SAME_TYPE(token, const char[]) ||                          \
               _IS_SAME_TYPE(token, unsigned char[]) ||                       \
               _IS_SAME_TYPE(token, const unsigned char[])) {                 \
      /* Arrays can be changed to pointer types by (var) +1, but this is not  \
       * compatible with (void *) types */                                    \
      const char *_ulog_value = (const char *)(uintptr_t)(token);             \
      if (!_ulog_value) {                                                     \
        out_cb(_GEN_TOKEN_FORMAT("(char *) ", "nullptr"), #token);            \
      } else if (info_out) {                                                  \
        out_cb(_GEN_TOKEN_FORMAT("(char *)[%" PRIu32 "] ",                    \
                                 _GEN_STRING_TOKEN_FORMAT(STR_GREEN)),        \
               (uint32_t)strlen(_ulog_value), #token, _ulog_value);           \
      } else {                                                                \
        out_cb(_GEN_TOKEN_FORMAT("", _GEN_STRING_TOKEN_FORMAT(STR_GREEN)),    \
               #token, _ulog_value);                                          \
      }                                                                       \
    } else if (_IS_SAME_TYPE(token, void *) ||                                \
               _IS_SAME_TYPE(token, short *) ||                               \
               _IS_SAME_TYPE(token, unsigned short *) ||                      \
               _IS_SAME_TYPE(token, int *) ||                                 \
               _IS_SAME_TYPE(token, unsigned int *) ||                        \
               _IS_SAME_TYPE(token, long *) ||                                \
               _IS_SAME_TYPE(token, unsigned long *) ||                       \
               _IS_SAME_TYPE(token, long long *) ||                           \
               _IS_SAME_TYPE(token, unsigned long long *) ||                  \
               _IS_SAME_TYPE(token, float *) ||                               \
               _IS_SAME_TYPE(token, double *)) {                              \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(void *) ", "%" PRIx64), #token, \
                          (uint64_t)(token))                                  \
                 : out_cb(_GEN_TOKEN_FORMAT("", "%" PRIx64), #token,          \
                          (uint64_t)(token));                                 \
    } else {                                                                  \
      (info_out) ? out_cb(_GEN_TOKEN_FORMAT("(unknown) ", "(none)"), #token)  \
                 : out_cb(_GEN_TOKEN_FORMAT("", "(none)"), #token);           \
    }                                                                         \
  } while (0)

#define _EXPAND(...) __VA_ARGS__
#define EAT_COMMA(...) , ##__VA_ARGS__
/**
 * Get the number of parameters of the function-like macro
 */
#define _ARG_COUNT(...)                                                       \
  _ARG_COUNT_PRIVATE(NULL EAT_COMMA(__VA_ARGS__), 64, 63, 62, 61, 60, 59, 58, \
                     57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44,  \
                     43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30,  \
                     29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,  \
                     15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _ARG_COUNT_PRIVATE(...) _EXPAND(_ARG_COUNT_PRIVATE_MSVC(__VA_ARGS__))
#define _ARG_COUNT_PRIVATE_MSVC(                                               \
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

#define _OUT_DEBUG_NOLOCK(need_newline, ...)                         \
  logger_log(ULOG_LEVEL_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, \
             need_newline, false, ##__VA_ARGS__)

#define _OUT_MULTI_TOKEN(...)                                                 \
  do {                                                                        \
    logger_output_lock();                                                     \
    _OUT_DEBUG_NOLOCK(false, "");                                             \
    _EXPAND(_MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)) \
    logger_nolock_flush();                                                    \
    logger_output_unlock();                                                   \
  } while (0)

#define _OUT_TOKEN_WRAPPER_LOCKED(token, left)                            \
  do {                                                                    \
    _OUT_TOKEN(token, _OUT_RAW_LOCKED, false);                            \
    if (left)                                                             \
      logger_raw(false, logger_color_is_enabled() ? STR_RED ", " : ", "); \
  } while (0)

#define _LOG_TOKEN_AUX(_1, ...)                           \
  _OUT_TOKEN_WRAPPER_LOCKED(_1, _ARG_COUNT(__VA_ARGS__)); \
  _MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))

#define _TOKEN_AUX_0(...) logger_raw(false, "\r\n");
#define _TOKEN_AUX_1(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_2(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_3(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_4(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_5(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_6(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_7(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_8(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_9(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_10(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_11(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_12(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_13(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_14(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_15(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))
#define _TOKEN_AUX_16(_1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(_1, __VA_ARGS__)(__VA_ARGS__))

#define _FORMAT_FOR_TIME_CODE(format, unit)                               \
  logger_color_is_enabled() ? STR_GREEN "time " STR_RED "{ " STR_BLUE     \
                                        "%s%s " STR_RED "} => " STR_GREEN \
                                        "%" format unit                   \
                            : "time { %s%s } => %" format unit

#define _TIME_CODE(...)                                                        \
  do {                                                                         \
    const int _CODE_LENGTH_MAX = 50;                                           \
    uint64_t _ulog_start_time_us = logger_get_time_us();                       \
    __VA_ARGS__;                                                               \
    uint64_t _ulog_end_time_us = logger_get_time_us();                         \
    uint32_t _ulog_timediff_us = (_ulog_end_time_us - _ulog_start_time_us);    \
    char _ulog_function_str[_CODE_LENGTH_MAX];                                 \
    memset(_ulog_function_str, 0, _CODE_LENGTH_MAX);                           \
    strncpy(_ulog_function_str, #__VA_ARGS__, _CODE_LENGTH_MAX - 1);           \
    LOG_DEBUG(_FORMAT_FOR_TIME_CODE(PRIu32, STR_RED "us"), _ulog_function_str, \
              strncmp(#__VA_ARGS__, _ulog_function_str, _CODE_LENGTH_MAX)      \
                  ? "..."                                                      \
                  : "",                                                        \
              _ulog_timediff_us);                                              \
  } while (0)

#define _GEN_COLOR_FORMAT_FOR_HEX_DUMP(place1, place2, place3, place4) \
  STR_RED place1 STR_GREEN place2 STR_RED place3 STR_BLUE place4

#define _HEX_DUMP_FORMAT                                                   \
  logger_color_is_enabled() ? STR_GREEN                                    \
      "hex_dump" _GEN_COLOR_FORMAT_FOR_HEX_DUMP("(", "data", ":", "%s")    \
          _GEN_COLOR_FORMAT_FOR_HEX_DUMP(", ", "length", ":", "%" PRIuMAX) \
              _GEN_COLOR_FORMAT_FOR_HEX_DUMP(", ", "width", ":",           \
                                             "%" PRIuMAX STR_RED ") =>")   \
                            : "hex_dump(data:%s, length:%" PRIuMAX         \
                              ", width:%" PRIuMAX ")"

#define _HEX_DUMP(data, length, width)                                    \
  do {                                                                    \
    logger_output_lock();                                                 \
    _OUT_DEBUG_NOLOCK(true, _HEX_DUMP_FORMAT, #data, (uintmax_t)(length), \
                      (uintmax_t)(width));                                \
    logger_nolock_flush();                                                \
    logger_hex_dump(data, length, width, (uintptr_t)(data), true, false); \
    logger_output_unlock();                                               \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ULOG_LEVEL_TRACE = 0,
  ULOG_LEVEL_DEBUG,
  ULOG_LEVEL_INFO,
  ULOG_LEVEL_WARN,
  ULOG_LEVEL_ERROR,
  ULOG_LEVEL_FATAL,
  ULOG_LEVEL_NUMBER
} LogLevel;

/**
 * Lock the log mutex
 * @return
 */
int logger_output_lock(void);

/**
 * Unlock the log mutex
 * @return
 */
int logger_output_unlock(void);

/**
 * Display contents in hexadecimal and ascii.
 * Same format as "hexdump -C filename"
 * @param data The starting address of the data to be displayed
 * @param length Display length starting from "data"
 * @param width How many bytes of data are displayed in each line
 * @param base_address Base address, the displayed address starts from this
 * value
 * @param tail_addr_out Tail address output, whether to output the last address
 * after output
 * @param need_lock Whether the output needs to be locked during output, it is
 * recommended to lock
 * @return Last output address
 */
uintptr_t logger_hex_dump(const void *data, size_t length, size_t width,
                          uintptr_t base_address, bool tail_addr_out,
                          bool need_lock);

/**
 * Raw data output, similar to printf
 * @param fmt Format of the format string
 * @param lock_and_flush Whether the output needs to be locked during output, it
 * is recommended to lock
 * @param ... Parameters in the format
 */
void logger_raw(bool lock_and_flush, const char *fmt, ...);

/**
 * Print log
 * Internal functions should not be called directly from outside, macros such as
 * LOG_DEBUG / LOG_INFO should be used
 * @param level Output level
 * @param file File name
 * @param func Function name
 * @param line Line number of the file
 * @param newline Whether to output a new line at the end
 * @param lock_and_flush Whether the output needs to be locked during output, it
 * is recommended to lock
 * @param fmt Format string, consistent with printf series functions
 * @param ...
 */
void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, bool newline, bool lock_and_flush,
                const char *fmt, ...);

/**
 * Flush log buffer
 * @note: Internal function, please do not call.
 */
int logger_nolock_flush(void);

#ifdef __cplusplus
}
#endif

#endif  // _ULOG_COMMON_H
