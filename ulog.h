#ifndef __ULOG_H__
#define __ULOG_H__

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
#define _logger_log(level, ...) \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
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
#define _TYPE_CMP(X, Y) (typeid(X) == typeid(Y))
#else
#define _TYPE_CMP(X, Y) __builtin_types_compatible_p(typeof(X), typeof(Y))
#endif

#define LOG_TOKEN(token)                                                      \
    do {                                                                      \
        if (_TYPE_CMP(token, float) || _TYPE_CMP(token, double)) {            \
            LOG_DEBUG("(float) %s => %f", #token, token);                     \
            /* Types with symbols below 64 bits */                            \
        } else if (_TYPE_CMP(token, char) ||                                  \
                   _TYPE_CMP(token, unsigned char) ||                         \
                   _TYPE_CMP(token, short) ||                                 \
                   _TYPE_CMP(token, unsigned short) ||                        \
                   _TYPE_CMP(token, int) || _TYPE_CMP(token, unsigned int) || \
                   _TYPE_CMP(token, long) || _TYPE_CMP(token, long long)) {   \
            LOG_DEBUG("(int) %s => %" PRId64, #token, (int64_t)(token));      \
            /* May be a 64-bit type */                                        \
        } else if (_TYPE_CMP(token, unsigned long) ||                         \
                   _TYPE_CMP(token, unsigned long long)) {                    \
            LOG_DEBUG("(int) %s => %" PRIu64, #token, (uint64_t)(token));     \
        } else if (_TYPE_CMP(token, char *) ||                                \
                   _TYPE_CMP(token, unsigned char *) ||                       \
                   _TYPE_CMP(token, char[]) ||                                \
                   _TYPE_CMP(token, unsigned char[])) {                       \
            LOG_DEBUG("(char*) %s => %s", #token, token);                     \
        } else if (_TYPE_CMP(token, void *) || _TYPE_CMP(token, short *) ||   \
                   _TYPE_CMP(token, unsigned short *) ||                      \
                   _TYPE_CMP(token, int *) ||                                 \
                   _TYPE_CMP(token, unsigned int *) ||                        \
                   _TYPE_CMP(token, long *) ||                                \
                   _TYPE_CMP(token, unsigned long *) ||                       \
                   _TYPE_CMP(token, long long *) ||                           \
                   _TYPE_CMP(token, unsigned long long *) ||                  \
                   _TYPE_CMP(token, float *) || _TYPE_CMP(token, double *)) { \
            LOG_DEBUG("(void *) %s => %x", #token, token);                    \
        } else {                                                              \
            LOG_DEBUG("(none) %s = (unknown)", #token);                       \
        }                                                                     \
    } while (0)

#define _LOG_TIME_FUNCTION_LENGTH 50

#define LOG_TIME_CODE(function)                                        \
    do {                                                               \
        struct timespec ulog_time_tsp1 = {};                           \
        struct timespec ulog_time_tsp2 = {};                           \
        clock_gettime(CLOCK_MONOTONIC, &ulog_time_tsp1);               \
        function;                                                      \
        clock_gettime(CLOCK_MONOTONIC, &ulog_time_tsp2);               \
        float ulog_time_timediff =                                     \
            (ulog_time_tsp2.tv_sec - ulog_time_tsp1.tv_sec) +          \
            (float)(ulog_time_tsp2.tv_nsec - ulog_time_tsp1.tv_nsec) / \
                (1000 * 1000 * 1000);                                  \
        char ulog_time_function_str[_LOG_TIME_FUNCTION_LENGTH];        \
        memset(ulog_time_function_str, 0, _LOG_TIME_FUNCTION_LENGTH);  \
        strncpy(ulog_time_function_str, #function,                     \
                _LOG_TIME_FUNCTION_LENGTH - 1);                        \
        LOG_DEBUG("time { %s }: %fs", ulog_time_function_str,          \
                  ulog_time_timediff);                                 \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*LogOutputCb)(const char *ptr);

typedef enum {
    ULOG_VERBOSE = 0,
    ULOG_DEBUG,
    ULOG_INFO,
    ULOG_WARN,
    ULOG_ERROR,
    ULOG_ASSERT,
    ULOG_LEVEL_NUMBER
} LoggerLevel;

void logger_enable_output(void);
void logger_disable_output(void);
void logger_enable_color(void);
void logger_disable_color(void);
void logger_set_output_level(LoggerLevel level);
void logger_init(LogOutputCb output_cb);
void logger_log(LoggerLevel level, const char *file, const char *func, int line,
                const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  //__ULOG_H__
