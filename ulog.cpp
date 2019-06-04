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

void uLogLog(const char *file, int line, unsigned level, const char *fmt, ...) {
#if !defined(ULOG_DISABLE)

    if (!output_cb || !fmt || level < ULOG_OUTPUT_LEVEL)
        return;

    LockGuard lock = LockGuard(log_lock);

    char *buf_ptr = log_out_buf;
    // The last three characters are '\r', '\n', '\0'
    char *buf_end_ptr = log_out_buf + LOG_OUTBUF_LEN - 3;

    /* Print serial number */
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), STR_RESET "#%06u ", log_evt_num++);
    buf_ptr = log_out_buf + strlen(log_out_buf);

    // Print time
    struct timespec tsp {};
    clock_gettime(CLOCK_MONOTONIC, &tsp);
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "[ %ld.%03ld ] ", tsp.tv_sec,
             tsp.tv_nsec / (1000 * 1000));
    buf_ptr = log_out_buf + strlen(log_out_buf);

    // Print level, file and line
    char *info_str = nullptr;
    switch (level) {
#define  __FUNC_LINE_FORMAT__ STR_BLACK "/%s:%d "
        case ULOG_DEBUG:
            info_str = (char *)STR_BOLD_BLUE "D" __FUNC_LINE_FORMAT__ STR_BLUE ;
            break;
        case ULOG_INFO:
            info_str = (char *)STR_BOLD_GREEN "I" __FUNC_LINE_FORMAT__ STR_GREEN;
            break;
        case ULOG_WARN:
            info_str = (char *)STR_BOLD_YELLOW "W" __FUNC_LINE_FORMAT__ STR_YELLOW;
            break;
        case ULOG_ERROR:
            info_str = (char *)STR_BOLD_RED "E" __FUNC_LINE_FORMAT__ STR_RED;
            break;
        case ULOG_ASSERT:
            info_str = (char *)STR_BOLD_PURPLE "A" __FUNC_LINE_FORMAT__ STR_PURPLE;
            break;
        case ULOG_VERBOSE:
        default:
            info_str = (char *)STR_BOLD_WHITE "V" __FUNC_LINE_FORMAT__ STR_WHITE;
#undef __FUNC_LINE_FORMAT__
    }
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), info_str, file, line);
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
