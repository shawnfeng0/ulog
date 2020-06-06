#include "ulog/ulog.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef ULOG_OUTBUF_LEN
#define ULOG_OUTBUF_LEN 512 /* Size of buffer used for log printout */
#endif

#if ULOG_OUTBUF_LEN < 64
#pragma message("ULOG_OUTBUF_LEN is recommended to be greater than 64")
#endif

static char log_out_buf_[ULOG_OUTBUF_LEN];
static char *const kLogBufferStartIndex_ = log_out_buf_;
static char *const kLogBufferEndIndex_ = log_out_buf_ + sizeof(log_out_buf_);
static char *cur_buf_ptr_ = log_out_buf_;
static uint32_t log_evt_num_ = 1;

#define SNPRINTF_WRAPPER(fmt, ...)                                        \
  do {                                                                    \
    snprintf(cur_buf_ptr_, (kLogBufferEndIndex_ - cur_buf_ptr_), fmt,     \
             ##__VA_ARGS__);                                              \
    cur_buf_ptr_ = kLogBufferStartIndex_ + strlen(kLogBufferStartIndex_); \
  } while (0)

#define VSNPRINTF_WRAPPER(fmt, arg_list)                                  \
  do {                                                                    \
    vsnprintf(cur_buf_ptr_, (kLogBufferEndIndex_ - cur_buf_ptr_), fmt,    \
              arg_list);                                                  \
    cur_buf_ptr_ = kLogBufferStartIndex_ + strlen(kLogBufferStartIndex_); \
  } while (0)

// Log mutex lock and time
#if defined(_LOG_UNIX_LIKE_PLATFORM)
#include <pthread.h>
static int print(void *unused, const char *str) { return printf("%s", str); }
static LogOutput output_cb_ = print;
static pthread_mutex_t log_pthread_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static uint64_t clock_gettime_wrapper(void) {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t)(tp.tv_sec) * 1000 * 1000 + tp.tv_nsec / 1000;
}
static LogGetTimeUs get_time_us_cb_ = clock_gettime_wrapper;
static bool log_process_id_enabled_ = true;
#else
static LogOutput output_cb_ = NULL;
static void *mutex_ = NULL;
static LogMutexLock mutex_lock_cb_ = NULL;
static LogMutexUnlock mutex_unlock_cb_ = NULL;
static LogGetTimeUs get_time_us_cb_ = NULL;
static bool log_process_id_enabled_ = false;
#endif
// The private data set by the user will be passed to the output function
static void *external_private_data_ = NULL;

static LogTimeFormat time_format_ = LOG_LOCAL_TIME_SUPPORT
                                        ? LOG_TIME_FORMAT_LOCAL_TIME
                                        : LOG_TIME_FORMAT_TIMESTAMP;

// Logger default configuration
static bool log_output_enabled_ = true;
static bool log_color_enabled_ = true;
static bool log_number_enabled_ = false;
static bool log_time_enabled_ = true;
static bool log_level_enabled_ = true;
static bool log_file_line_enabled_ = true;
static bool log_function_enabled_ = true;
static LogLevel log_level_ = ULOG_LEVEL_TRACE;

enum {
  INDEX_PRIMARY_COLOR = 0,
  INDEX_SECONDARY_COLOR,
  INDEX_LEVEL_MARK,
  INDEX_MAX,
};

static char *level_infos[ULOG_LEVEL_NUMBER][INDEX_MAX] = {
    {(char *)STR_BOLD_WHITE, (char *)STR_WHITE, (char *)"T"},    // TRACE
    {(char *)STR_BOLD_BLUE, (char *)STR_BLUE, (char *)"D"},      // DEBUG
    {(char *)STR_BOLD_GREEN, (char *)STR_GREEN, (char *)"I"},    // INFO
    {(char *)STR_BOLD_YELLOW, (char *)STR_YELLOW, (char *)"W"},  // WARN
    {(char *)STR_BOLD_RED, (char *)STR_RED, (char *)"E"},        // ERROR
    {(char *)STR_BOLD_PURPLE, (char *)STR_PURPLE, (char *)"F"},  // FATAL
};

#if defined(_LOG_UNIX_LIKE_PLATFORM)
#include <unistd.h>
#define GET_PID() getpid()
#if defined(__APPLE__)
#include <pthread.h>
#define GET_TID() pthread_mach_thread_np(pthread_self())
#else  // defined(__APPLE__)
#include <sys/syscall.h>
#define GET_TID() syscall(SYS_gettid)
#endif  // defined(__APPLE__)

#else  // defined(_LOG_UNIX_LIKE_PLATFORM)
#define GET_PID() 0
#define GET_TID() 0
#endif  // defined(_LOG_UNIX_LIKE_PLATFORM)

static bool is_logger_valid() { return output_cb_ && log_output_enabled_; }

void logger_enable_output(bool enable) { log_output_enabled_ = enable; }

void logger_enable_color(bool enable) { log_color_enabled_ = enable; }

bool logger_color_is_enabled(void) { return log_color_enabled_; }

void logger_enable_number_output(bool enable) { log_number_enabled_ = enable; }

void logger_enable_time_output(bool enable) { log_time_enabled_ = enable; }

#if defined(_LOG_UNIX_LIKE_PLATFORM)
void logger_enable_process_id_output(bool enable) {
  log_process_id_enabled_ = enable;
}
#endif

void logger_enable_level_output(bool enable) { log_level_enabled_ = enable; }

void logger_enable_file_line_output(bool enable) {
  log_file_line_enabled_ = enable;
}

void logger_enable_function_output(bool enable) {
  log_function_enabled_ = enable;
}

void logger_set_output_level(LogLevel level) { log_level_ = level; }

#if !defined(_LOG_UNIX_LIKE_PLATFORM)
void logger_set_mutex_lock(void *mutex, LogMutexLock mutex_lock_cb,
                           LogMutexUnlock mutex_unlock_cb) {
  mutex_ = mutex;
  mutex_lock_cb_ = mutex_lock_cb;
  mutex_unlock_cb_ = mutex_unlock_cb;
}
#endif

void logger_set_time_callback(LogGetTimeUs get_time_us_cb) {
  get_time_us_cb_ = get_time_us_cb;
}

void logger_set_time_format(LogTimeFormat time_format) {
  time_format_ = time_format;
}

void logger_init(void *private_data, LogOutput output_cb) {
  logger_output_lock();

  output_cb_ = output_cb;
  external_private_data_ = private_data;

  logger_output_unlock();
}

uint64_t logger_get_time_us(void) {
  return get_time_us_cb_ ? get_time_us_cb_() : 0;
}

int logger_output_lock(void) {
#if !defined(_LOG_UNIX_LIKE_PLATFORM)
  return (mutex_lock_cb_ && mutex_) ? mutex_lock_cb_(mutex_) : 0;
#else
  return pthread_mutex_lock(&log_pthread_mutex_);
#endif
}

int logger_output_unlock(void) {
#if !defined(_LOG_UNIX_LIKE_PLATFORM)
  return (mutex_unlock_cb_ && mutex_) ? mutex_unlock_cb_(mutex_) : 0;
#else
  return pthread_mutex_unlock(&log_pthread_mutex_);
#endif
}

uintptr_t logger_nolock_hex_dump(const void *data, size_t length, size_t width,
                                 uintptr_t base_address, bool tail_addr_out) {
  if (!data || width == 0 || !is_logger_valid()) return 0;

  const uint8_t *data_raw = data;
  const uint8_t *data_cur = data;

  // The output fails, in order to avoid output confusion, the rest will not
  // be output
  if (logger_nolock_flush() <= 0) {
    return 0;
  }

  bool out_break = false;
  while (length) {
    SNPRINTF_WRAPPER("%08" PRIxPTR "  ", data_cur - data_raw + base_address);
    for (size_t i = 0; i < width; i++) {
      if (i < length)
        SNPRINTF_WRAPPER("%02" PRIx8 " %s", data_cur[i],
                         i == width / 2 - 1 ? " " : "");
      else
        SNPRINTF_WRAPPER("   %s", i == width / 2 - 1 ? " " : "");
    }
    SNPRINTF_WRAPPER(" |");
    for (size_t i = 0; i < width && i < length; i++)
      SNPRINTF_WRAPPER("%c", isprint(data_cur[i]) ? data_cur[i] : '.');
    SNPRINTF_WRAPPER("|\r\n");

    // The output fails, in order to avoid output confusion, the rest will not
    // be output
    if (logger_nolock_flush() <= 0) {
      out_break = true;
      break;
    }

    data_cur += length < width ? length : width;
    length -= length < width ? length : width;
  }

  if (out_break) {
    SNPRINTF_WRAPPER("hex dump is break!\r\n");
  } else if (tail_addr_out) {
    SNPRINTF_WRAPPER("%08" PRIxPTR "\r\n", data_cur - data_raw + base_address);
  }
  logger_nolock_flush();
  return data_cur - data_raw + base_address;
}

void logger_raw(bool lock_and_flush, const char *fmt, ...) {
  if (!is_logger_valid() || !fmt) return;

  if (lock_and_flush) logger_output_lock();

  va_list ap;
  va_start(ap, fmt);
  VSNPRINTF_WRAPPER(fmt, ap);
  va_end(ap);

  if (lock_and_flush) {
    logger_nolock_flush();
    logger_output_unlock();
  }
}

int logger_nolock_flush(void) {
  int ret = 0;
  if (cur_buf_ptr_ != kLogBufferStartIndex_) {
    ret = output_cb_(external_private_data_, kLogBufferStartIndex_);
    cur_buf_ptr_ = kLogBufferStartIndex_;
  }
  return ret;
}

void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, bool newline, bool lock_and_flush,
                const char *fmt, ...) {
  if (!is_logger_valid() || !fmt || level < log_level_) return;

  if (lock_and_flush) logger_output_lock();

  // Color
  if (log_number_enabled_ || log_time_enabled_ || log_level_enabled_)
    SNPRINTF_WRAPPER("%s", log_color_enabled_
                               ? level_infos[level][INDEX_SECONDARY_COLOR]
                               : "");

  // Print serial number
  if (log_number_enabled_) SNPRINTF_WRAPPER("#%06" PRIu32 " ", log_evt_num_++);

  // Print time
  if (log_time_enabled_) {
    uint64_t time_ms = logger_get_time_us() / 1000;
    if (time_format_ == LOG_TIME_FORMAT_LOCAL_TIME) {
      time_t time_s = time_ms / 1000;
      struct tm lt = *localtime(&time_s);
      SNPRINTF_WRAPPER("[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                       lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                       lt.tm_min, lt.tm_sec, (int)(time_ms % 1000));
    } else {
      SNPRINTF_WRAPPER("[%" PRId64 ".%03" PRId64 "] ", time_ms / 1000,
                       time_ms % 1000);
    }
  }

  // Print process and thread id
  if (log_process_id_enabled_)
    SNPRINTF_WRAPPER("%" PRId32 "-%" PRId32 " ", (int32_t)GET_PID(),
                     (int32_t)GET_TID());

  // Print level
  if (log_level_enabled_)
    SNPRINTF_WRAPPER("%s", level_infos[level][INDEX_LEVEL_MARK]);

  // Print gray color
  if (log_level_enabled_ || log_file_line_enabled_ || log_function_enabled_)
    SNPRINTF_WRAPPER("%s", log_color_enabled_ ? STR_GRAY : "");

  // Print '/'
  if (log_level_enabled_) SNPRINTF_WRAPPER("/");

  // Print '('
  if (log_file_line_enabled_ || log_function_enabled_) SNPRINTF_WRAPPER("(");

  // Print file and line
  if (log_file_line_enabled_) SNPRINTF_WRAPPER("%s:%" PRIu32, file, line);

  // Print function
  if (log_function_enabled_)
    SNPRINTF_WRAPPER("%s%s", log_file_line_enabled_ ? " " : "", func);

  // Print ')'
  if (log_file_line_enabled_ || log_function_enabled_) SNPRINTF_WRAPPER(")");

  // Print ' '
  if (log_level_enabled_ || log_file_line_enabled_ || log_function_enabled_)
    SNPRINTF_WRAPPER(" ");

  // Print log info
  SNPRINTF_WRAPPER("%s", log_color_enabled_
                             ? level_infos[level][INDEX_SECONDARY_COLOR]
                             : "");

  va_list ap;
  va_start(ap, fmt);
  VSNPRINTF_WRAPPER(fmt, ap);
  va_end(ap);

  SNPRINTF_WRAPPER("%s", log_color_enabled_ ? STR_RESET : "");

  if (newline) SNPRINTF_WRAPPER("\r\n");

  if (lock_and_flush) {
    logger_nolock_flush();
    logger_output_unlock();
  }
}
