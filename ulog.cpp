#include "ulog.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#ifndef LOG_OUTBUF_LEN
#define LOG_OUTBUF_LEN 128 /* Size of buffer used for log printout */
#endif

#if !defined(ULOG_DISABLE)

static char log_out_buf[LOG_OUTBUF_LEN];
static uint16_t log_evt_num = 1;
static OutputCb output_cb = NULL;
static pthread_mutex_t log_lock;

class LockGuard {
   public:
       LockGuard(pthread_mutex_t& _m):mMutex(_m) {
           pthread_mutex_lock(&mMutex);
       }
       ~LockGuard() {
           pthread_mutex_unlock(&mMutex);
       }
   private:
       pthread_mutex_t& mMutex;
};

static uint32_t getRefTimeUs() {
    struct timespec tsp;
    clock_gettime(CLOCK_REALTIME, &tsp);
    return (int32_t) ((tsp.tv_sec % 1000) * 1e6 + (int32_t) (tsp.tv_nsec / 1000));
}

#endif


void uLogInit(OutputCb cb) {
#if !defined(ULOG_DISABLE)

  pthread_mutex_init(&log_lock, NULL);
  output_cb = cb;

#if defined(ULOG_CLS)
  char clear_str[3] = {'\033', 'c', '\0'};  // clean screen
  if (output_cb) output_cb(clear_str);
#endif

#endif
}

void uLogLog(const char *file, int line, unsigned level, const char *fmt, ...)
{
#if !defined(ULOG_DISABLE)
    LockGuard lock = LockGuard(log_lock);

#if LOG_OUTBUF_LEN < 64
#error "LOG_OUTBUF_LEN does not have enough size."
#endif
    if (!output_cb || !fmt)
        return;

    char *bufPtr = log_out_buf;
    char *bufEndPtr = log_out_buf + LOG_OUTBUF_LEN - 3; // Less 3 for '\r', '\n', '\0'

    /* Print serial number */
    snprintf(bufPtr, (bufEndPtr - bufPtr), "#%06u ", log_evt_num++);
    bufPtr = log_out_buf + strlen(log_out_buf);

    uint32_t ref_time_us = getRefTimeUs();

    // Print time
    snprintf(bufPtr, (bufEndPtr - bufPtr), "[ %d.%03u ] ", ref_time_us / 1000 / 1000,
              (ref_time_us / 1000) % 1000);
    bufPtr = log_out_buf + strlen(log_out_buf);

    // Print level, file and line
    char *infoStr = NULL;
    switch (level)
    {
        case ULOG_ERROR:
            infoStr = (char*)"\x1b[31;1mERROR: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case ULOG_WARN:
            infoStr = (char*)"\x1b[33;1mWARN: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case ULOG_INFO:
        default:
            infoStr = (char*)"\x1b[32;1mINFO: \x1b[30;1m(%s:%d) \x1b[0m";
    }
    snprintf(bufPtr, (bufEndPtr - bufPtr), infoStr, file, line);
    bufPtr = log_out_buf + strlen(log_out_buf);

    va_list ap;
    va_start(ap,fmt);
    vsnprintf(bufPtr, (bufEndPtr - bufPtr), fmt, ap);
    va_end(ap);
    bufPtr = log_out_buf + strlen(log_out_buf);

    *bufPtr++ = '\r';
    *bufPtr++ = '\n';
    *bufPtr++ = '\0';

    output_cb(log_out_buf);
#endif
}
