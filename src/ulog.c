#include "ulog.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef ULOG_OUTBUF_LEN
#define ULOG_OUTBUF_LEN 256 /* Size of buffer used for log printout */
#endif

#if ULOG_OUTBUF_LEN < 64
#pragma message("ULOG_OUTBUF_LEN is recommended to be greater than 64")
#endif

#if !defined(ULOG_DEFAULT_LEVEL)
#define ULOG_DEFAULT_LEVEL ULOG_VERBOSE;
#endif

// Lock the log mutex
static int logger_lock(void);

// Unlock the log mutex
static int logger_unlock(void);

#define LOGGER_LOCK_GUARD(...) \
  do {                         \
    logger_lock();             \
    __VA_ARGS__;               \
    logger_unlock();           \
  } while (0)

#if !defined(ULOG_DISABLE)

static char log_out_buf_[ULOG_OUTBUF_LEN];
static uint32_t log_evt_num_ = 1;

// Log mutex lock and time
#if defined(_LOG_UNIX_LIKE_PLATFORM)
#include <pthread.h>
#include <stdlib.h>  // For abort()
static int printf_wrapper(const char *str) { return printf("%s", str); }
static LogOutput output_cb_ = printf_wrapper;
static pthread_mutex_t log_pthread_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static void *mutex_ = &log_pthread_mutex_;
static LogMutexLock mutex_lock_cb_ = (LogMutexLock)pthread_mutex_lock;
static LogMutexUnlock mutex_unlock_cb_ = (LogMutexUnlock)pthread_mutex_unlock;
static uint64_t clock_gettime_wrapper(void) {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000;
}
static LogGetTimeUs get_time_us_cb_ = clock_gettime_wrapper;
static LogAssertHandlerCb assert_handler_cb_ = abort;
static bool log_process_id_enabled_ = true;
#else
static LogOutput output_cb_ = NULL;
static void *mutex_ = NULL;
static LogMutexLock mutex_lock_cb_ = NULL;
static LogMutexUnlock mutex_unlock_cb_ = NULL;
LogGetTimeUs get_time_us_cb_ = NULL;
LogAssertHandlerCb assert_handler_cb_ = NULL;
static bool log_process_id_enabled_ = false;
#endif
LogTimeFormat time_format_ = LOG_LOCAL_TIME_SUPPORT ? LOG_TIME_FORMAT_LOCAL_TIME
                                                    : LOG_TIME_FORMAT_TIMESTAMP;

// Logger configuration
static bool log_output_enabled_ = true;

#if !defined(ULOG_DISABLE_COLOR)
static bool log_color_enabled_ = true;
#else
static bool log_color_enabled_ = false;
#endif

static bool log_number_enabled_ = false;
static bool log_time_enabled_ = true;
static bool log_level_enabled_ = true;
static bool log_file_line_enabled_ = true;
static bool log_function_enabled_ = true;

static LogLevel log_level_ = ULOG_DEFAULT_LEVEL;

enum {
  INDEX_PRIMARY_COLOR = 0,
  INDEX_SECONDARY_COLOR,
  INDEX_LEVEL_MARK,
  INDEX_MAX,
};

static char *level_infos[ULOG_LEVEL_NUMBER][INDEX_MAX] = {
    {(char *)STR_BOLD_WHITE, (char *)STR_WHITE, (char *)"V"},    // VERBOSE
    {(char *)STR_BOLD_BLUE, (char *)STR_BLUE, (char *)"D"},      // DEBUG
    {(char *)STR_BOLD_GREEN, (char *)STR_GREEN, (char *)"I"},    // INFO
    {(char *)STR_BOLD_YELLOW, (char *)STR_YELLOW, (char *)"W"},  // WARN
    {(char *)STR_BOLD_RED, (char *)STR_RED, (char *)"E"},        // ERROR
    {(char *)STR_BOLD_PURPLE, (char *)STR_PURPLE, (char *)"A"},  // ASSERT
};

#endif  // !ULOG_DISABLE

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

void logger_enable_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_output_enabled_ = enable;
#endif
}

void logger_enable_color(bool enable) {
#if !defined(ULOG_DISABLE)
  log_color_enabled_ = enable;
#endif
}

bool logger_color_is_enabled(void) {
#if !defined(ULOG_DISABLE)
  return log_color_enabled_;
#else
  return false;
#endif
}

void logger_enable_number_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_number_enabled_ = enable;
#endif
}

void logger_enable_time_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_time_enabled_ = enable;
#endif
}

#if defined(_LOG_UNIX_LIKE_PLATFORM)
void logger_enable_process_id_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_process_id_enabled_ = enable;
#endif
}
#endif

void logger_enable_level_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_level_enabled_ = enable;
#endif
}

void logger_enable_file_line_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_file_line_enabled_ = enable;
#endif
}

void logger_enable_function_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_function_enabled_ = enable;
#endif
}

void logger_set_output_level(LogLevel level) {
#if !defined(ULOG_DISABLE)
  log_level_ = level;
#endif
}

void logger_set_mutex_lock(void *mutex, LogMutexLock mutex_lock_cb,
                           LogMutexUnlock mutex_unlock_cb) {
#if !defined(ULOG_DISABLE)
  mutex_ = mutex;
  mutex_lock_cb_ = mutex_lock_cb;
  mutex_unlock_cb_ = mutex_unlock_cb;
#endif
}

void logger_set_time_callback(LogGetTimeUs get_time_us_cb) {
#if !defined(ULOG_DISABLE)
  get_time_us_cb_ = get_time_us_cb;
#endif
}

void logger_set_time_format(LogTimeFormat time_format) {
#if !defined(ULOG_DISABLE)
  time_format_ = time_format;
#endif
}

void logger_assert_handler(void) {
#if !defined(ULOG_DISABLE)
  if (assert_handler_cb_) assert_handler_cb_();
#endif
}

void logger_set_assert_callback(LogAssertHandlerCb assert_handler_cb) {
#if !defined(ULOG_DISABLE)
  assert_handler_cb_ = assert_handler_cb;
#endif
}

void logger_init(LogOutput output_cb) {
#if !defined(ULOG_DISABLE)
  output_cb_ = output_cb;

#if defined(ULOG_CLS)
  char clear_str[3] = {'\033', 'c', '\0'};  // clean screen
  if (output_cb) output_cb(clear_str);
#endif
#endif
}

uint64_t logger_get_time_us(void) {
#if !defined(ULOG_DISABLE)
  return get_time_us_cb_ ? get_time_us_cb_() : 0;
#else
  return 0;
#endif
}

// Lock the log mutex
static int logger_lock(void) {
  return (mutex_lock_cb_ && mutex_) ? mutex_lock_cb_(mutex_) : 0;
}

// Unlock the log mutex
static int logger_unlock(void) {
  return (mutex_unlock_cb_ && mutex_) ? mutex_unlock_cb_(mutex_) : 0;
}

uintptr_t logger_hex_dump(const void *data, size_t length, size_t width,
                          uintptr_t base_address, bool tail_addr_out) {
  if (!data || width == 0 || !output_cb_ || !log_output_enabled_) return 0;

  const uint8_t *data_raw = data;
  const uint8_t *data_cur = data;
  char *buf_ptr = log_out_buf_;
  // The last two characters are '\r', '\n'
  char *buf_end_ptr = log_out_buf_ + sizeof(log_out_buf_) - 2;

#define SNPRINTF_WRAPPER(fmt, ...)                                  \
  do {                                                              \
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ##__VA_ARGS__); \
    buf_ptr = log_out_buf_ + strlen(log_out_buf_);                  \
  } while (0)

  // Lock the log mutex
  LOGGER_LOCK_GUARD(

      while (length) {
        SNPRINTF_WRAPPER("%08" PRIxPTR "  ",
                         data_cur - data_raw + base_address);
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
        SNPRINTF_WRAPPER("|");

        strncpy(buf_ptr, "\r\n", 3);

        output_cb_(log_out_buf_);
        buf_ptr = log_out_buf_;

        data_cur += length < width ? length : width;
        length -= length < width ? length : width;
      }

      if (tail_addr_out) {
        SNPRINTF_WRAPPER("%08" PRIxPTR "\r\n",
                         data_cur - data_raw + base_address);
        output_cb_(log_out_buf_);
      }

  );  // LOGGER_LOCK_GUARD
  return data_cur - data_raw + base_address;
#undef SNPRINTF_WRAPPER
}

void logger_raw(const char *fmt, ...) {
  if (!output_cb_ || !fmt || !log_output_enabled_) return;

  char *buf_ptr = log_out_buf_;
  char *buf_end_ptr = log_out_buf_ + sizeof(log_out_buf_);

  LOGGER_LOCK_GUARD(

      va_list ap; va_start(ap, fmt);
      vsnprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ap); va_end(ap);
      output_cb_(log_out_buf_);

  );  // LOGGER_LOCK_GUARD()
}

void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, bool newline, const char *fmt, ...) {
#if !defined(ULOG_DISABLE)

  if (!output_cb_ || !fmt || level < log_level_ || !log_output_enabled_) return;

  char *buf_ptr = log_out_buf_;

  // The last two characters are '\r', '\n'
  char *buf_end_ptr = log_out_buf_ + sizeof(log_out_buf_) - 2;

#define SNPRINTF_WRAPPER(fmt, ...)                                  \
  do {                                                              \
    snprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ##__VA_ARGS__); \
    buf_ptr = log_out_buf_ + strlen(log_out_buf_);                  \
  } while (0)

#define VSNPRINTF_WRAPPER(fmt, ...)                                  \
  do {                                                               \
    vsnprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ##__VA_ARGS__); \
    buf_ptr = log_out_buf_ + strlen(log_out_buf_);                   \
  } while (0)

  LOGGER_LOCK_GUARD(

      // Color
      if (log_number_enabled_ || log_time_enabled_ || log_level_enabled_)
          SNPRINTF_WRAPPER("%s", log_color_enabled_
                                     ? level_infos[level][INDEX_SECONDARY_COLOR]
                                     : "");

      // Print serial number
      if (log_number_enabled_)
          SNPRINTF_WRAPPER("#%06" PRIu32 " ", log_evt_num_++);

      // Print time
      if (log_time_enabled_) {
        uint64_t time_ms = logger_get_time_us() / 1000;
        if (time_format_ == LOG_TIME_FORMAT_LOCAL_TIME) {
          time_t time_s = time_ms / 1000;
          struct tm *ts = localtime(&time_s);
          SNPRINTF_WRAPPER("[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                           ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday,
                           ts->tm_hour, ts->tm_min, ts->tm_sec,
                           (int)(time_ms % 1000));
        } else {
          SNPRINTF_WRAPPER("[%" PRId64 ".%03" PRId64 "] ", time_ms / 1000,
                           time_ms % 1000);
        }
      }

      // Print process and thread id
      if (log_process_id_enabled_) SNPRINTF_WRAPPER(
          "%" PRId32 "-%" PRId32 " ", (int32_t)GET_PID(), (int32_t)GET_TID());

      // Print level
      if (log_level_enabled_)
          SNPRINTF_WRAPPER("%s", level_infos[level][INDEX_LEVEL_MARK]);

      // Print gray color
      if (log_level_enabled_ || log_file_line_enabled_ || log_function_enabled_)
          SNPRINTF_WRAPPER("%s", log_color_enabled_ ? STR_GRAY : "");

      // Print '/'
      if (log_level_enabled_) SNPRINTF_WRAPPER("/");

      // Print '('
      if (log_file_line_enabled_ || log_function_enabled_)
          SNPRINTF_WRAPPER("(");

      // Print file and line
      if (log_file_line_enabled_) SNPRINTF_WRAPPER("%s:%" PRIu32, file, line);

      // Print function
      if (log_function_enabled_)
          SNPRINTF_WRAPPER("%s%s", log_file_line_enabled_ ? " " : "", func);

      // Print ')'
      if (log_file_line_enabled_ || log_function_enabled_)
          SNPRINTF_WRAPPER(")");

      // Print ' '
      if (log_level_enabled_ || log_file_line_enabled_ || log_function_enabled_)
          SNPRINTF_WRAPPER(" ");

      // Reset output pointer if auxiliary information is output
      if (buf_ptr != log_out_buf_) {
        output_cb_(log_out_buf_);
        buf_ptr = log_out_buf_;
      }

      // Print log info
      SNPRINTF_WRAPPER("%s", log_color_enabled_
                                 ? level_infos[level][INDEX_SECONDARY_COLOR]
                                 : "");

      va_list ap; va_start(ap, fmt); VSNPRINTF_WRAPPER(fmt, ap); va_end(ap);

      SNPRINTF_WRAPPER("%s", log_color_enabled_ ? STR_RESET : "");

      if (newline) strncpy(buf_ptr, "\r\n", 3);

      output_cb_(log_out_buf_);

  );  // LOGGER_LOCK_GUARD

#undef SNPRINTF_WRAPPER
#undef VSNPRINTF_WRAPPER
#endif
}
