#define ULOG_ENABLE
#include "ulog.h"

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <stdio.h>

#ifndef LOG_OUTBUF_LEN
#define LOG_OUTBUF_LEN 168 /* Size of buffer used for log printout */
#endif

#if defined(ULOG_ENABLE)

static char Log_outBuf[LOG_OUTBUF_LEN];
static uint16_t Log_evtNum = 1;

#endif

void Ulog_log(const char *file, int line, unsigned level, const char *fmt, ...)
{
#if defined(ULOG_ENABLE)

#if LOG_OUTBUF_LEN < 64
#error "LOG_OUTBUF_LEN does not have enough size."
#endif

    char *bufPtr = Log_outBuf;
    char *bufEndPtr = Log_outBuf + LOG_OUTBUF_LEN - 3; // Less 3 for '\r', '\n', '\0'

#if defined(LOG_CLS)
    // first log
    if (Log_evtNum == 1)
    {
        char a[2] = {'\033', 'c'}; // clean screen
        QueueWrite(&a, 2);
    }
#endif

    /* Print serial number */
    snprintf(bufPtr, (bufEndPtr - bufPtr), "#%06u ", Log_evtNum++);
    bufPtr = Log_outBuf + strlen(Log_outBuf);

    uint32_t tstamp_cust = 0; //AONRTCCurrentCompareValueGet();
    uint16_t seconds = tstamp_cust >> 16;
    uint16_t ifraction = tstamp_cust & 0xFFFF;
    int fraction = (int)((double)ifraction / 65536 * 1000); // Get 3 decimals

    // Print time
    snprintf(bufPtr, (bufEndPtr - bufPtr), "[ %d.%03u ] ", seconds,
             fraction);
    bufPtr = Log_outBuf + strlen(Log_outBuf);

    // Print level, file and line
    char *infoStr = NULL;
    switch (level)
    {
        case LEVEL_ERROR:
            infoStr = "\x1b[31;1mERROR: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case LEVEL_WARNING:
            infoStr = "\x1b[33;1mWARNING: \x1b[30;1m(%s:%d) \x1b[0m";
            break;
        case LEVEL_INFO:
        default:
            infoStr = "\x1b[32;1mINFO: \x1b[30;1m(%s:%d) \x1b[0m";
    }
    snprintf(bufPtr, (bufEndPtr - bufPtr), infoStr, file, line);
    bufPtr = Log_outBuf + strlen(Log_outBuf);

    va_list ap;
    va_start(ap,fmt);
    vsnprintf(bufPtr, (bufEndPtr - bufPtr), fmt, ap);
    va_end(ap);
    bufPtr = Log_outBuf + strlen(Log_outBuf);

    *bufPtr++ = '\r';
    *bufPtr++ = '\n';
    *bufPtr++ = '\0';

    // TODO add mutex
    puts(Log_outBuf);

#endif
}
