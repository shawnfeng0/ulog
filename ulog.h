#ifndef ULOG_ULOG_H
#define ULOG_ULOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int  (*OutputCb)(const char *ptr);

enum {
    ULOG_VERBOSE,
    ULOG_DEBUG,
    ULOG_INFO,
    ULOG_WARN,
    ULOG_ERROR,
    ULOG_ASSERT
};

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#include <string.h>
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define Log_verbose(fmt, ...)    _uLogLog(ULOG_VERBOSE, fmt, ##__VA_ARGS__)
#define Log_debug(fmt, ...)      _uLogLog(ULOG_DEBUG, fmt, ##__VA_ARGS__)
#define Log_info(fmt, ...)       _uLogLog(ULOG_INFO, fmt, ##__VA_ARGS__)
#define Log_warn(fmt, ...)       _uLogLog(ULOG_WARN, fmt, ##__VA_ARGS__)
#define Log_error(fmt, ...)      _uLogLog(ULOG_ERROR, fmt, ##__VA_ARGS__)
#define Log_assert(fmt, ...)     _uLogLog(ULOG_ASSERT, fmt, ##__VA_ARGS__)

#if !defined(ULOG_DISABLE)

#define _uLogLog(level, ...) uLogLog(__FILENAME__, __LINE__, level, ##__VA_ARGS__)

#else

#define _uLogLog(level, ...)

#endif

void uLogInit(OutputCb cb);
void uLogLog(const char *file, int line, unsigned level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif //ULOG_ULOG_H
