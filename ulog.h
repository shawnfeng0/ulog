#ifndef ULOG_ULOG_H
#define ULOG_ULOG_H
#define ULOG_ENABLE
#ifdef __cplusplus
extern "C"
{
#endif

enum {
    LEVEL_VERBOSE,
    LEVEL_DEBUG,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
    LEVEL_ASSERT
};

#if defined(ULOG_ENABLE)

#define Log_verbose(...)    _Ulog_log(LEVEL_VERBOSE, __VA_ARGS__)
#define Log_debug(...)      _Ulog_log(LEVEL_DEBUG, __VA_ARGS__)
#define Log_info(...)       _Ulog_log(LEVEL_INFO, __VA_ARGS__)
#define Log_warning(...)    _Ulog_log(LEVEL_WARNING, __VA_ARGS__)
#define Log_error(...)      _Ulog_log(LEVEL_ERROR, __VA_ARGS__)
#define Log_assert(...)     _Ulog_log(LEVEL_ASSERT, __VA_ARGS__)

#define _Ulog_log(level, ...) Ulog_log(__FILE__, __LINE__, level, __VA_ARGS__)

#else

#define Log_info(fmt, ...)
#define Log_warning(fmt, ...)
#define Log_error(fmt, ...)

#endif

void Ulog_log(const char *file, int line, unsigned level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif //ULOG_ULOG_H
