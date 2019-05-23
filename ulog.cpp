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

    char *bufPtr = log_out_buf;
    // The last three characters are '\r', '\n', '\0'
    char *bufEndPtr = log_out_buf + LOG_OUTBUF_LEN - 3;

    /* Print serial number */
    snprintf(bufPtr, (bufEndPtr - bufPtr), "#%06u ", log_evt_num++);
    bufPtr = log_out_buf + strlen(log_out_buf);

    // Print time
    struct timespec tsp {};
    clock_gettime(CLOCK_MONOTONIC, &tsp);
    snprintf(bufPtr, (bufEndPtr - bufPtr), "[ %ld.%03ld ] ", tsp.tv_sec,
             tsp.tv_nsec / (1000 * 1000));
    bufPtr = log_out_buf + strlen(log_out_buf);

    // Print level, file and line
    char *infoStr = nullptr;
    switch (level) {
        case ULOG_DEBUG:
            infoStr = (char *)STR_BLUE "DEBUG: " STR_BLACK "(%s:%d) " STR_RESET;
            break;
        case ULOG_INFO:
            infoStr = (char *)STR_GREEN "INFO: " STR_BLACK "(%s:%d) " STR_RESET;
            break;
        case ULOG_WARN:
            infoStr = (char *)STR_YELLOW "WARN: " STR_BLACK "(%s:%d) " STR_RESET;
            break;
        case ULOG_ERROR:
            infoStr = (char *)STR_RED "ERROR: " STR_BLACK "(%s:%d) " STR_RESET;
            break;
        case ULOG_ASSERT:
            infoStr = (char *)STR_PURPLE "ASSERT: " STR_BLACK "(%s:%d) " STR_RESET;
            break;
        case ULOG_VERBOSE:
        default:
            infoStr = (char *)STR_WHITE "VERBOSE: " STR_BLACK "(%s:%d) " STR_RESET;
    }
    snprintf(bufPtr, (bufEndPtr - bufPtr), infoStr, file, line);
    bufPtr = log_out_buf + strlen(log_out_buf);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(bufPtr, (bufEndPtr - bufPtr), fmt, ap);
    va_end(ap);

    bufPtr = log_out_buf + strlen(log_out_buf);

    *bufPtr++ = '\r';
    *bufPtr++ = '\n';
    *bufPtr = '\0';

    output_cb(log_out_buf);
#endif
}
