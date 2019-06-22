#include "ulog.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef LOG_OUTBUF_LEN
#define LOG_OUTBUF_LEN 128 /* Size of buffer used for log printout */
#endif

#if LOG_OUTBUF_LEN < 64
#error "LOG_OUTBUF_LEN does not have enough size."
#endif

#if !defined(ULOG_DISABLE)

static char log_out_buf[LOG_OUTBUF_LEN];
static uint16_t log_evt_num = 1;
static OutputCb output_cb = nullptr;
static pthread_mutex_t log_lock;

enum {
    INDEX_PRIMARY_COLOR = 0,
    INDEX_SECONDARY_COLOR,
    INDEX_LEVEL_MARK,
    INDEX_MAX,
};

static char *level_infos[ULOG_LEVEL_NUMBER][INDEX_MAX] {
    {(char *) STR_BOLD_WHITE, (char *) STR_WHITE, (char *) "V"}, // VERBOSE
    {(char *) STR_BOLD_BLUE, (char *) STR_BLUE, (char *) "D"}, // DEBUG
    {(char *) STR_BOLD_GREEN, (char *) STR_GREEN, (char *) "I"}, // INFO
    {(char *) STR_BOLD_YELLOW, (char *) STR_YELLOW, (char *) "W"}, // WARN
    {(char *) STR_BOLD_RED, (char *) STR_RED, (char *) "E"}, // ERROR
    {(char *) STR_BOLD_PURPLE, (char *) STR_PURPLE, (char *) "A"}, // ASSERT
};

class LockGuard {
   public:
    explicit LockGuard(pthread_mutex_t &_m) : mMutex(_m) {
        pthread_mutex_lock(&mMutex);
    }

    ~LockGuard() {
        pthread_mutex_unlock(&mMutex);
    }

   private:
    pthread_mutex_t &mMutex;
};

#endif

void uLogInit(OutputCb cb) {
#if !defined(ULOG_DISABLE)

    pthread_mutex_init(&log_lock, nullptr);
    output_cb = cb;

#if defined(ULOG_CLS)
    char clear_str[3] = {'\033', 'c', '\0'};  // clean screen
    if (output_cb) output_cb(clear_str);
#endif

#endif
}

void uLogLog(enum ULOG_LEVEL level, const char *file, const char *func, int line, const char *fmt, ...) {
#if !defined(ULOG_DISABLE)

    if (!output_cb || !fmt || level < ULOG_OUTPUT_LEVEL)
        return;

    LockGuard lock = LockGuard(log_lock);

    char *buf_ptr = log_out_buf;
    // The last three characters are '\r', '\n', '\0'
    char *buf_end_ptr = log_out_buf + LOG_OUTBUF_LEN - 3;

    char *primary_color = level_infos[level][INDEX_SECONDARY_COLOR];

    /* Print serial number */
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s#%06u ", primary_color, log_evt_num++);
    buf_ptr = log_out_buf + strlen(log_out_buf);

    // Print time
    struct timespec tsp {};
    clock_gettime(CLOCK_MONOTONIC, &tsp);
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "[%lu.%03lu] ", tsp.tv_sec,
             tsp.tv_nsec / (1000 * 1000));
    buf_ptr = log_out_buf + strlen(log_out_buf);

    // Print level, file, function and line
    char *level_mark = level_infos[level][INDEX_LEVEL_MARK];
    char *info_str_fmt = (char *) "%s" STR_GRAY "/%s(%s:%d) %s";
    char *log_info_color = level_infos[level][INDEX_SECONDARY_COLOR];
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), info_str_fmt,
            level_mark, file, func, line, log_info_color);
    output_cb(log_out_buf);
    buf_ptr = log_out_buf;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ap);
    va_end(ap);

    buf_ptr = log_out_buf + strlen(log_out_buf);

    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s", STR_RESET);

    buf_ptr = log_out_buf + strlen(log_out_buf);

    *buf_ptr++ = '\r';
    *buf_ptr++ = '\n';
    *buf_ptr = '\0';

    output_cb(log_out_buf);
#endif
}
