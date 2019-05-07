#include "ulog.h"

#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef LOG_OUTBUF_LEN
#define LOG_OUTBUF_LEN 168 /* Size of buffer used for log printout */
#endif

#if defined(ULOG_ENABLE)

static char log_out_buf[LOG_OUTBUF_LEN];
static uint16_t log_evt_num = 1;
static OutputCb output_cb = NULL;

#endif

void uLogInit(OutputCb cb)
{
#if defined(ULOG_ENABLE)
    output_cb = cb;

#if defined(ULOG_CLS)
    char clear_str[3] = {'\033', 'c', '\0'}; // clean screen
    if (output_cb)
        output_cb(clear_str);
#endif

#endif
}

#include <driverlib/aon_rtc.h>
float GetSeconds() {
    uint32_t tstamp_cust = AONRTCCurrentCompareValueGet();
    uint16_t seconds = tstamp_cust >> 16;
    uint16_t ifraction = tstamp_cust & 0xFFFF;
    int fraction = (int)((double)ifraction / 65536 * 1000); // Get 3 decimals
    return (double)fraction/1000 + seconds;
}

void uLogLog(const char *file, int line, unsigned level, const char *fmt, ...)
{
#if defined(ULOG_ENABLE)

#if LOG_OUTBUF_LEN < 64
#error "LOG_OUTBUF_LEN does not have enough size."
#endif

    char *bufPtr = log_out_buf;
    char *bufEndPtr = log_out_buf + LOG_OUTBUF_LEN - 3; // Less 3 for '\r', '\n', '\0'

    /* Print serial number */
    snprintf(bufPtr, (bufEndPtr - bufPtr), "#%06u ", log_evt_num++);
    bufPtr = log_out_buf + strlen(log_out_buf);

    float fseconds = GetSeconds();

    // Print time
    snprintf(bufPtr, (bufEndPtr - bufPtr), "[ %d.%03u ] ", (int) fseconds,
             (int)(fseconds*1000) % 1000);
    bufPtr = log_out_buf + strlen(log_out_buf);

    // Print level, file and line
    char *infoStr = NULL;
    switch (level)
    {
        case ULOG_ERROR:
            infoStr = "\x1b[31;1mERROR: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case ULOG_WARN:
            infoStr = "\x1b[33;1mWARN: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case ULOG_INFO:
        default:
            infoStr = "\x1b[32;1mINFO: \x1b[30;1m(%s:%d) \x1b[0m";
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

    if (output_cb)
        output_cb(log_out_buf);
#endif
}
