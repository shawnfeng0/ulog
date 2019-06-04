#ifndef ULOG_ULOG_H
#define ULOG_ULOG_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ULOG_OUTPUT_LEVEL)
#define ULOG_OUTPUT_LEVEL ULOG_VERBOSE
#endif

typedef int (*OutputCb)(const char *ptr);

enum {
    ULOG_VERBOSE,
    ULOG_DEBUG,
    ULOG_INFO,
    ULOG_WARN,
    ULOG_ERROR,
    ULOG_ASSERT
};

#if !defined(ULOG_NO_COLOR)
#define _STR_COLOR(color) "\x1b[" #color "m"
#else
#define _STR_COLOR(color) ""
#endif

#define STR_RESET _STR_COLOR(0)
#define STR_BLACK _STR_COLOR(0;30)
#define STR_RED _STR_COLOR(0;31)
#define STR_GREEN _STR_COLOR(0;32)
#define STR_YELLOW _STR_COLOR(0;33)
#define STR_BLUE _STR_COLOR(0;34)
#define STR_PURPLE _STR_COLOR(0;35)
#define STR_SKYBLUE _STR_COLOR(0;36)
#define STR_WHITE _STR_COLOR(0;37)

#define STR_BOLD_BLACK _STR_COLOR(30;1)
#define STR_BOLD_RED _STR_COLOR(31;1)
#define STR_BOLD_GREEN _STR_COLOR(32;1)
#define STR_BOLD_YELLOW _STR_COLOR(33;1)
#define STR_BOLD_BLUE _STR_COLOR(34;1)
#define STR_BOLD_PURPLE _STR_COLOR(35;1)
#define STR_BOLD_SKYBLUE _STR_COLOR(36;1)
#define STR_BOLD_WHITE _STR_COLOR(37;1)

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#include <string.h>
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#if !defined(ULOG_DISABLE)
#define _uLogLog(level, ...) uLogLog(__FILENAME__, __LINE__, level, ##__VA_ARGS__)
#else
#define _uLogLog(level, ...)
#endif

#define Log_verbose(fmt, ...) _uLogLog(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define Log_debug(fmt, ...) _uLogLog(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define Log_info(fmt, ...) _uLogLog(ULOG_INFO, fmt, ##__VA_ARGS__)
#define Log_warn(fmt, ...) _uLogLog(ULOG_WARN, fmt, ##__VA_ARGS__)
#define Log_error(fmt, ...) _uLogLog(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define Log_assert(fmt, ...) _uLogLog(ULOG_ASSERT, fmt, ##__VA_ARGS__)

#define Log_token(token) Log_debug(#token " = %f", (float) token)

void uLogInit(OutputCb cb);
void uLogLog(const char *file, int line, unsigned level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //ULOG_ULOG_H
