#include "ulog.h"
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

static char log_out_buf_[LOG_OUTBUF_LEN];
static uint32_t log_evt_num_ = 1;
static LogOutputCb output_cb_ = NULL;

// Log mutex lock and time
#if defined(__unix__)
#include <pthread.h>
static pthread_mutex_t log_pthread_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static void *mutex_ = &log_pthread_mutex_;
static LogMutexLock mutex_lock_cb_ = (LogMutexLock)pthread_mutex_lock;
static LogMutexUnlock mutex_unlock_cb_ = (LogMutexUnlock)pthread_mutex_unlock;
static int clock_gettime_wrapper(struct timespec *tp) {
  return clock_gettime(CLOCK_MONOTONIC, tp);
}
LogGetTime get_time_cb_ = clock_gettime_wrapper;
#else
static void *mutex_ = NULL;
static LogMutexLock mutex_lock_cb_ = NULL;
static LogMutexUnlock mutex_unlock_cb_ = NULL;
LogGetTime get_time_cb_ = NULL;
#endif

// Logger configuration
static bool log_output_enable_ = true;

#if !defined(ULOG_DISABLE_COLOR)
static bool log_color_enable_ = true;
#else
static bool log_color_enable_ = false;
#endif

static bool log_number_output_ = true;
static bool log_time_output_ = true;
static bool log_level_output_ = true;
static bool log_file_line_output_ = true;
static bool log_function_output_ = true;

#if !defined(ULOG_DEFAULT_LEVEL)
static LogLevel log_output_level_ = ULOG_VERBOSE;
#else
static LogLevel log_output_level_ = ULOG_DEFAULT_LEVEL;
#endif

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

void logger_enable_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_output_enable_ = enable;
#endif
}

void logger_enable_color(bool enable) {
#if !defined(ULOG_DISABLE)
  log_color_enable_ = enable;
#endif
}

void logger_enable_number_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_number_output_ = enable;
#endif
}

void logger_enable_time_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_time_output_ = enable;
#endif
}

void logger_enable_level_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_level_output_ = enable;
#endif
}

void logger_enable_file_line_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_file_line_output_ = enable;
#endif
}

void logger_enable_function_output(bool enable) {
#if !defined(ULOG_DISABLE)
  log_function_output_ = enable;
#endif
}

void logger_set_output_level(LogLevel level) {
#if !defined(ULOG_DISABLE)
  log_output_level_ = level;
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

void logger_set_time_callback(LogGetTime get_time_cb) {
#if !defined(ULOG_DISABLE)
  get_time_cb_ = get_time_cb;
#endif
}

void logger_init(LogOutputCb output_cb) {
#if !defined(ULOG_DISABLE)
  output_cb_ = output_cb;

#if defined(ULOG_CLS)
  char clear_str[3] = {'\033', 'c', '\0'};  // clean screen
  if (output_cb) output_cb(clear_str);
#endif
#endif
}

void logger_get_time(struct timespec *tp) {
#if !defined(ULOG_DISABLE)
  if (get_time_cb_) get_time_cb_(tp);
#endif
}

void logger_log(LogLevel level, const char *file, const char *func,
                uint32_t line, const char *fmt, ...) {
#if !defined(ULOG_DISABLE)

  if (!output_cb_ || !fmt || level < log_output_level_ || !log_output_enable_)
    return;

  // Lock the log mutex
  if (mutex_lock_cb_ && mutex_) mutex_lock_cb_(mutex_);

  char *buf_ptr = log_out_buf_;
  // The last three characters are '\r', '\n', '\0'
  char *buf_end_ptr = log_out_buf_ + LOG_OUTBUF_LEN - 3;

  char *log_info_color = log_color_enable_
                             ? level_infos[level][INDEX_SECONDARY_COLOR]
                             : (char *)"";
#define SNPRINTF_WRAPPER(fmt, ...) \
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ##__VA_ARGS__);
#define VSNPRINTF_WRAPPER(fmt, ...) \
  vsnprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ##__VA_ARGS__);

  // Color
  if (log_number_output_ || log_time_output_ || log_level_output_) {
    buf_ptr += SNPRINTF_WRAPPER("%s", log_info_color);
  }

  // Print serial number
  if (log_number_output_) {
    buf_ptr += SNPRINTF_WRAPPER("#%06" PRIu32 " ", log_evt_num_++);
  }

  // Print time
  if (log_time_output_) {
    struct timespec tsp = {0, 0};
    logger_get_time(&tsp);
    buf_ptr += SNPRINTF_WRAPPER("[%ld.%03ld] ", tsp.tv_sec,
                                tsp.tv_nsec / (1000 * 1000));
  }

  // Print level
  if (log_level_output_) {
    char *level_mark = level_infos[level][INDEX_LEVEL_MARK];
    char *info_str_color = (char *)(log_color_enable_ ? STR_GRAY : "");
    buf_ptr += SNPRINTF_WRAPPER("%s%s%s", level_mark, info_str_color,
                                log_level_output_ ? "/" : "");
  }

  // Print color
  if (log_file_line_output_ || log_function_output_) {
    buf_ptr += SNPRINTF_WRAPPER("(");
  }

  // Print file and line
  if (log_file_line_output_) {
    buf_ptr += SNPRINTF_WRAPPER("%s:%" PRIu32, file, line);
  }

  // Print function
  if (log_function_output_) {
    buf_ptr += SNPRINTF_WRAPPER("%s%s", log_file_line_output_ ? " " : "", func);
  }

  if (log_file_line_output_ || log_function_output_) {
    buf_ptr += SNPRINTF_WRAPPER(") ");
  }

  if (buf_ptr != log_out_buf_) {
    output_cb_(log_out_buf_);
    buf_ptr = log_out_buf_;
  }

  // Print log info
  buf_ptr += SNPRINTF_WRAPPER("%s", log_info_color);

  va_list ap;
  va_start(ap, fmt);
  buf_ptr += VSNPRINTF_WRAPPER(fmt, ap);
  va_end(ap);

  char *str_reset_color = (char *)(log_color_enable_ ? STR_RESET : "");
  buf_ptr += SNPRINTF_WRAPPER("%s", str_reset_color);

  *buf_ptr++ = '\r';
  *buf_ptr++ = '\n';
  *buf_ptr = '\0';

  output_cb_(log_out_buf_);

  // Unlock the log mutex
  if (mutex_unlock_cb_ && mutex_) mutex_unlock_cb_(mutex_);
#endif
}
