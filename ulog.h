#ifndef __ULOG_H__
#define __ULOG_H__

#include <time.h>
#include <string.h>

#if !defined(ULOG_OUTPUT_LEVEL)
#define ULOG_OUTPUT_LEVEL ULOG_VERBOSE
#endif

#if !defined(ULOG_NO_COLOR)
#define _STR_COLOR(color) "\x1b[" #color "m"
#else
#define _STR_COLOR(color) ""
#endif

#define STR_RESET _STR_COLOR(0)
#define STR_GRAY _STR_COLOR(38;5;8)

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
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#if !defined(ULOG_DISABLE)
#define _logger_log(level, ...) logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define _logger_log(level, ...)
#endif

#define LOG_VERBOSE(fmt, ...) _logger_log(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _logger_log(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _logger_log(ULOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _logger_log(ULOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _logger_log(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_ASSERT(fmt, ...) _logger_log(ULOG_ASSERT, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
#include <typeinfo>
#define __TYPE_CMP(X, Y) (typeid(X) == typeid(Y))
#else
#define __TYPE_CMP(X, Y) __builtin_types_compatible_p(typeof(X), typeof(Y))
#endif

#define LOG_TOKEN(token) do { \
    char *__ulog_token_fmt = (char *) "(none) %s = (unknown)"; \
    if (__TYPE_CMP(token, float) || __TYPE_CMP(token, double)) { \
        __ulog_token_fmt = (char *) "(double) %s => %f";\
    } else if (__TYPE_CMP(token, short) || __TYPE_CMP(token, unsigned short)) { \
        __ulog_token_fmt = (char *) "(short) %s => %hd";\
    } else if (__TYPE_CMP(token, unsigned short)) { \
        __ulog_token_fmt = (char *) "(unsigned short) %s => %hu";\
    } else if (__TYPE_CMP(token, int)) { \
        __ulog_token_fmt = (char *) "(int) %s => %d";\
    } else if (__TYPE_CMP(token, unsigned int)) { \
        __ulog_token_fmt = (char *) "(unsigned int) %s => %u";\
    } else if (__TYPE_CMP(token, long)) { \
        __ulog_token_fmt = (char *) "(long) %s => %ld";\
    } else if (__TYPE_CMP(token, unsigned long)) { \
        __ulog_token_fmt = (char *) "(unsigned long) %s => %lu";\
    } else if (__TYPE_CMP(token, long long)) { \
        __ulog_token_fmt = (char *) "(long long) %s => %lld";\
    } else if (__TYPE_CMP(token, unsigned long long)) { \
        __ulog_token_fmt = (char *) "(unsigned long long) %s => %llu";\
    } else if (__TYPE_CMP(token, char) || __TYPE_CMP(token, unsigned char)) { \
        __ulog_token_fmt = (char *) "(char) %s => %c";\
    } else if (__TYPE_CMP(token, char *) || __TYPE_CMP(token, unsigned char*) \
        || __TYPE_CMP(token, char[]) || __TYPE_CMP(token, unsigned char[]) \
        ) { \
        __ulog_token_fmt = (char *) "(char*) %s => %s";\
    } else if (__TYPE_CMP(token, void *) \
        || __TYPE_CMP(token, float*) || __TYPE_CMP(token, double*) \
        || __TYPE_CMP(token, int*) || __TYPE_CMP(token, unsigned int*) \
        || __TYPE_CMP(token, short*) || __TYPE_CMP(token, unsigned short*) \
        || __TYPE_CMP(token, long*) || __TYPE_CMP(token, unsigned long*) \
        || __TYPE_CMP(token, long long*) || __TYPE_CMP(token, unsigned long long*) \
        ) { \
        __ulog_token_fmt = (char *) "(void*) %s => %x";\
    } \
    LOG_DEBUG(__ulog_token_fmt, #token, token); \
} while (0)

#define __LOG_TIME_FUNCTION_LENGTH 50

#define LOG_TIME_CODE(function) do { \
    struct timespec __ulog_time_tsp1 = {}; \
    struct timespec __ulog_time_tsp2 = {}; \
    clock_gettime(CLOCK_MONOTONIC, &__ulog_time_tsp1); \
    function; \
    clock_gettime(CLOCK_MONOTONIC, &__ulog_time_tsp2); \
    float __ulog_time_timediff = (__ulog_time_tsp2.tv_sec - __ulog_time_tsp1.tv_sec) \
        + (float) (__ulog_time_tsp2.tv_nsec - __ulog_time_tsp1.tv_nsec) / (1000 * 1000 * 1000); \
    char __ulog_time_function_str[__LOG_TIME_FUNCTION_LENGTH]; \
    memset(__ulog_time_function_str, 0, __LOG_TIME_FUNCTION_LENGTH); \
    strncpy(__ulog_time_function_str, #function, __LOG_TIME_FUNCTION_LENGTH - 1); \
    LOG_DEBUG("time { %s }: %fs", __ulog_time_function_str, __ulog_time_timediff); \
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*LogOutputCb)(const char *ptr);

enum LoggerLevel {
    ULOG_VERBOSE = 0,
    ULOG_DEBUG,
    ULOG_INFO,
    ULOG_WARN,
    ULOG_ERROR,
    ULOG_ASSERT,
    ULOG_LEVEL_NUMBER
};

void logger_init(LogOutputCb output_cb);
void logger_log(enum LoggerLevel level, const char *file, const char *func, int line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
