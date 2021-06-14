#ifndef _ULOG_COMMON_H
#define _ULOG_COMMON_H

#define _STR_COLOR(color) "\x1b[" color "m"

#define STR_RESET _STR_COLOR("0")
#define STR_GRAY _STR_COLOR("38;5;8")

#define STR_BLACK _STR_COLOR("30")
#define STR_RED _STR_COLOR("31")
#define STR_GREEN _STR_COLOR("32")
#define STR_YELLOW _STR_COLOR("33")
#define STR_BLUE _STR_COLOR("34")
#define STR_PURPLE _STR_COLOR("35")
#define STR_SKYBLUE _STR_COLOR("36")
#define STR_WHITE _STR_COLOR("37")

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#define __FILENAME__                                       \
  (strrchr(__FILE__, '/')    ? strrchr(__FILE__, '/') + 1  \
   : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 \
                             : __FILE__)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define _ATTRIBUTE_CHECK_FORMAT(m, n) __attribute__((format(printf, m, n)))
#else
#define _ATTRIBUTE_CHECK_FORMAT(m, n)
#endif

#define _OUT_LOG(logger, level, ...)                                      \
  ({                                                                      \
    logger_log(logger, level, __FILENAME__, __FUNCTION__, __LINE__, true, \
               true, ##__VA_ARGS__);                                      \
  })

#define _OUT_RAW(logger, fmt, ...) \
  ({ logger_raw(logger, true, fmt, ##__VA_ARGS__); })

#define _OUT_RAW_LOCKED(logger, fmt, ...) \
  logger_raw_no_format_check(logger, false, fmt, ##__VA_ARGS__)

#define _OUT_DEBUG_NOLOCK(logger, need_newline, ...)                      \
  logger_log_no_format_check(logger, ULOG_LEVEL_DEBUG, __FILENAME__,      \
                             __FUNCTION__, __LINE__, need_newline, false, \
                             ##__VA_ARGS__)

#define _GEN_TOKEN_FORMAT(logger, format)                           \
  logger_color_is_enabled(logger) ? STR_BLUE "%s " STR_RED          \
                                             "=> " STR_GREEN format \
                                  : "%s => " format

#define _GEN_STRING_TOKEN_FORMAT(color) STR_RED "\"" color "%s" STR_RED "\""

#ifdef __cplusplus
namespace ulog {
namespace _token {

extern "C" void logger_raw(struct ulog_s *logger, bool lock_and_flush,
                           const char *fmt, ...);

// void *
inline void print(struct ulog_s *logger, const char *name, const void *value) {
  logger_raw(logger, false, _GEN_TOKEN_FORMAT(logger, "%p"),
             name ? name : "unnamed", value);
}

// const char *
inline void print(struct ulog_s *logger, const char *name, const char *value) {
  logger_raw(logger, false,
             _GEN_TOKEN_FORMAT(logger, _GEN_STRING_TOKEN_FORMAT(STR_GREEN)),
             name ? name : "unnamed", value ? value : "");
}

inline void print(struct ulog_s *logger, const char *name,
                  const unsigned char *value) {
  print(logger, name, reinterpret_cast<const char *>(value));
}

// double/float
inline void print(struct ulog_s *logger, const char *name, double value) {
  logger_raw(logger, false, _GEN_TOKEN_FORMAT(logger, "%f"),
             name ? name : "unnamed", value);
}

inline void print(struct ulog_s *logger, const char *name, float value) {
  print(logger, name, static_cast<double>(value));
}

// signed integer
inline void print(struct ulog_s *logger, const char *name, long long value) {
  logger_raw(logger, false, _GEN_TOKEN_FORMAT(logger, "%" PRId64),
             name ? name : "unnamed", (int64_t)value);
}

inline void print(struct ulog_s *logger, const char *name, long value) {
  print(logger, name, static_cast<long long>(value));
}

inline void print(struct ulog_s *logger, const char *name, int value) {
  print(logger, name, static_cast<long long>(value));
}

inline void print(struct ulog_s *logger, const char *name, short value) {
  print(logger, name, static_cast<long long>(value));
}

inline void print(struct ulog_s *logger, const char *name, char value) {
  print(logger, name, static_cast<long long>(value));
}

// unsigned integer
inline void print(struct ulog_s *logger, const char *name,
                  unsigned long long value) {
  logger_raw(logger, false, _GEN_TOKEN_FORMAT(logger, "%" PRIu64),
             name ? name : "unnamed", (uint64_t)value);
}

inline void print(struct ulog_s *logger, const char *name,
                  unsigned long value) {
  print(logger, name, static_cast<unsigned long long>(value));
}

inline void print(struct ulog_s *logger, const char *name, unsigned int value) {
  print(logger, name, static_cast<unsigned long long>(value));
}

inline void print(struct ulog_s *logger, const char *name,
                  unsigned short value) {
  print(logger, name, static_cast<unsigned long long>(value));
}

inline void print(struct ulog_s *logger, const char *name,
                  unsigned char value) {
  print(logger, name, static_cast<unsigned long long>(value));
}

inline void print(struct ulog_s *logger, const char *name, bool value) {
  print(logger, name, static_cast<unsigned long long>(value));
}

}  // namespace _token
}  // namespace ulog

#define _OUT_TOKEN_IMPLEMENT(logger, token) \
  ulog::_token::print(logger, #token, (token))

#else
// C version: implemented through GCC extensionn
#if defined(__GNUC__) || defined(__clang__)
#define _IS_SAME_TYPE(var, type) \
  __builtin_types_compatible_p(typeof(var), typeof(type))
#else
#pragma message( \
    "LOG_TOKEN is not available, plese use c++11 or clang or gcc compiler.")
#define _IS_SAME_TYPE(var, type) false
#endif

#define _OUT_TOKEN_IMPLEMENT(logger, token)                                    \
  ({                                                                           \
    if (_IS_SAME_TYPE(token, float) || _IS_SAME_TYPE(token, double)) {         \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "%f"), #token, token); \
    } else if (_IS_SAME_TYPE(token, bool)) {                                   \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "%d"), #token,         \
                      (token) ? 1 : 0);                                        \
      /* Signed integer */                                                     \
    } else if (_IS_SAME_TYPE(token, char) ||                                   \
               _IS_SAME_TYPE(token, signed char) ||                            \
               _IS_SAME_TYPE(token, short) || _IS_SAME_TYPE(token, int) ||     \
               _IS_SAME_TYPE(token, long) ||                                   \
               _IS_SAME_TYPE(token, long long)) {                              \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "%" PRId64), #token,   \
                      (int64_t)(token));                                       \
      /* Unsigned integer */                                                   \
    } else if (_IS_SAME_TYPE(token, unsigned char) ||                          \
               _IS_SAME_TYPE(token, unsigned short) ||                         \
               _IS_SAME_TYPE(token, unsigned int) ||                           \
               _IS_SAME_TYPE(token, unsigned long) ||                          \
               _IS_SAME_TYPE(token, unsigned long long)) {                     \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "%" PRIu64), #token,   \
                      (uint64_t)(token));                                      \
    } else if (_IS_SAME_TYPE(token, char *) ||                                 \
               _IS_SAME_TYPE(token, const char *) ||                           \
               _IS_SAME_TYPE(token, signed char *) ||                           \
               _IS_SAME_TYPE(token, const signed char *) ||                           \
               _IS_SAME_TYPE(token, unsigned char *) ||                        \
               _IS_SAME_TYPE(token, const unsigned char *) ||                  \
               _IS_SAME_TYPE(token, char[]) ||                                 \
               _IS_SAME_TYPE(token, const char[]) ||                           \
               _IS_SAME_TYPE(token, unsigned char[]) ||                        \
               _IS_SAME_TYPE(token, const unsigned char[])) {                  \
      /* Arrays can be changed to pointer types by (var) +1, but this is not   \
       * compatible with (void *) types */                                     \
      const char *_ulog_value = (const char *)(uintptr_t)(token);              \
      _OUT_RAW_LOCKED(                                                         \
          logger,                                                              \
          _GEN_TOKEN_FORMAT(logger, _GEN_STRING_TOKEN_FORMAT(STR_GREEN)),      \
          #token, _ulog_value ? _ulog_value : "NULL");                         \
    } else if (_IS_SAME_TYPE(token, void *) ||                                 \
               _IS_SAME_TYPE(token, short *) ||                                \
               _IS_SAME_TYPE(token, unsigned short *) ||                       \
               _IS_SAME_TYPE(token, int *) ||                                  \
               _IS_SAME_TYPE(token, unsigned int *) ||                         \
               _IS_SAME_TYPE(token, long *) ||                                 \
               _IS_SAME_TYPE(token, unsigned long *) ||                        \
               _IS_SAME_TYPE(token, long long *) ||                            \
               _IS_SAME_TYPE(token, unsigned long long *) ||                   \
               _IS_SAME_TYPE(token, float *) ||                                \
               _IS_SAME_TYPE(token, double *)) {                               \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "%p"), #token, token); \
    } else {                                                                   \
      _OUT_RAW_LOCKED(logger, _GEN_TOKEN_FORMAT(logger, "(none)"), #token);    \
    }                                                                          \
  })
#endif

#define _OUT_TOKEN(logger, token)                \
  ({                                             \
    logger_output_lock(logger);                  \
    _OUT_DEBUG_NOLOCK(logger, false, "");        \
    _OUT_TOKEN_IMPLEMENT(logger, token);         \
    logger_raw(logger, false, "\r\n" STR_RESET); \
    logger_nolock_flush(logger);                 \
    logger_output_unlock(logger);                \
  })

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

#define _OUT_MULTI_TOKEN(logger, ...)                                         \
  ({                                                                          \
    logger_output_lock(logger);                                               \
    _OUT_DEBUG_NOLOCK(logger, false, "");                                     \
    _EXPAND(_MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))(logger,       \
                                                                __VA_ARGS__)) \
    logger_nolock_flush(logger);                                              \
    logger_output_unlock(logger);                                             \
  })

#define _OUT_TOKEN_WRAPPER_LOCKED(logger, token, left)            \
  ({                                                              \
    _OUT_TOKEN_IMPLEMENT(logger, token);                          \
    if (left)                                                     \
      logger_raw_no_format_check(                                 \
          logger, false,                                          \
          logger_color_is_enabled(logger) ? STR_RED ", " : ", "); \
    else if (logger_color_is_enabled(logger))                     \
      logger_raw_no_format_check(logger, false, STR_RESET);       \
  })

#define _LOG_TOKEN_AUX(logger, _1, ...)                           \
  _OUT_TOKEN_WRAPPER_LOCKED(logger, _1, _ARG_COUNT(__VA_ARGS__)); \
  _MACRO_CONCAT(_TOKEN_AUX_, _ARG_COUNT(__VA_ARGS__))

#define _TOKEN_AUX_0(logger, ...) logger_raw(logger, false, "\r\n");
#define _TOKEN_AUX_1(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_2(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_3(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_4(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_5(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_6(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_7(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))
#define _TOKEN_AUX_8(logger, _1, ...) \
  _EXPAND(_LOG_TOKEN_AUX(logger, _1, __VA_ARGS__)(logger, __VA_ARGS__))

#define _FORMAT_FOR_TIME_CODE(logger, format, unit)                   \
  logger_color_is_enabled(logger) ? STR_GREEN                         \
      "time " STR_RED "{ " STR_BLUE "%s%s " STR_RED "} => " STR_GREEN \
      "%" format unit                                                 \
                                  : "time { %s%s } => %" format unit

#define _TIME_CODE(logger, ...)                                              \
  ({                                                                         \
    const int _CODE_LENGTH_MAX = 50;                                         \
    uint64_t _ulog_start_time_us = logger_monotonic_time_us();               \
    __VA_ARGS__;                                                             \
    uint64_t _ulog_end_time_us = logger_monotonic_time_us();                 \
    uint32_t _ulog_timediff_us = (_ulog_end_time_us - _ulog_start_time_us);  \
    char _ulog_function_str[_CODE_LENGTH_MAX];                               \
    memset(_ulog_function_str, 0, _CODE_LENGTH_MAX);                         \
    strncpy(_ulog_function_str, #__VA_ARGS__, _CODE_LENGTH_MAX - 1);         \
    LOGGER_DEBUG(_FORMAT_FOR_TIME_CODE(logger, PRIu32, STR_RED "us"),        \
                 _ulog_function_str,                                         \
                 strncmp(#__VA_ARGS__, _ulog_function_str, _CODE_LENGTH_MAX) \
                     ? "..."                                                 \
                     : "",                                                   \
                 _ulog_timediff_us);                                         \
  })

#define _GEN_COLOR_FORMAT_FOR_HEX_DUMP(place1, place2, place3, place4) \
  STR_RED place1 STR_GREEN place2 STR_RED place3 STR_BLUE place4

#define _HEX_DUMP_FORMAT(logger)                                           \
  logger_color_is_enabled(logger) ? STR_GREEN                              \
      "hex_dump" _GEN_COLOR_FORMAT_FOR_HEX_DUMP("(", "data", ":", "%s")    \
          _GEN_COLOR_FORMAT_FOR_HEX_DUMP(", ", "length", ":", "%" PRIuMAX) \
              _GEN_COLOR_FORMAT_FOR_HEX_DUMP(", ", "width", ":",           \
                                             "%" PRIuMAX STR_RED ") =>")   \
                                  : "hex_dump(data:%s, length:%" PRIuMAX   \
                                    ", width:%" PRIuMAX ")"

#define _HEX_DUMP(logger, data, length, width)                             \
  ({                                                                       \
    logger_output_lock(logger);                                            \
    _OUT_DEBUG_NOLOCK(logger, true, _HEX_DUMP_FORMAT(logger), #data,       \
                      (uintmax_t)(length), (uintmax_t)(width));            \
    logger_nolock_hex_dump(logger, data, length, width, (uintptr_t)(data), \
                           true);                                          \
    logger_output_unlock(logger);                                          \
  })

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lock the log mutex
 * @return
 */
int logger_output_lock(struct ulog_s *logger);

/**
 * Unlock the log mutex
 * @return
 */
int logger_output_unlock(struct ulog_s *logger);

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
 * @return Last output address
 */
uintptr_t logger_nolock_hex_dump(struct ulog_s *logger, const void *data,
                                 size_t length, size_t width,
                                 uintptr_t base_address, bool tail_addr_out);

/**
 * Raw data output, similar to printf
 * @param fmt Format of the format string
 * @param lock_and_flush Whether the output needs to be locked during output, it
 * is recommended to lock
 * @param ... Parameters in the format
 */
_ATTRIBUTE_CHECK_FORMAT(3, 4)
void logger_raw(struct ulog_s *logger, bool lock_and_flush, const char *fmt,
                ...);
void logger_raw_no_format_check(struct ulog_s *logger, bool lock_and_flush,
                                const char *fmt, ...);

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
_ATTRIBUTE_CHECK_FORMAT(8, 9)
void logger_log(struct ulog_s *logger, enum ulog_level_e level, const char *file,
                const char *func, uint32_t line, bool newline,
                bool lock_and_flush, const char *fmt, ...);
void logger_log_no_format_check(struct ulog_s *logger, enum ulog_level_e level,
                                const char *file, const char *func,
                                uint32_t line, bool newline,
                                bool lock_and_flush, const char *fmt, ...);

/**
 * Flush log buffer
 * @note: Internal function, please do not call.
 */
int logger_nolock_flush(struct ulog_s *logger);

#ifdef __cplusplus
}
#endif

#endif  // _ULOG_COMMON_H
