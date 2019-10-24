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

static char log_out_buf_[LOG_OUTBUF_LEN];
static uint32_t log_evt_num_ = 1;
static LogOutputCb output_cb_ = nullptr;
static pthread_mutex_t log_lock_;

// Logger configuration
static bool log_output_enable_ = true;

#if !defined(ULOG_DISABLE_COLOR)
static bool log_color_enable_ = true;
#else
static bool log_color_enable_ = false;
#endif

#if !defined(ULOG_DEFAULT_LEVEL)
static LoggerLevel log_output_level_ = ULOG_VERBOSE;
#else
static LoggerLevel log_output_level_ = ULOG_DEFAULT_LEVEL;
#endif

enum {
  INDEX_PRIMARY_COLOR = 0,
  INDEX_SECONDARY_COLOR,
  INDEX_LEVEL_MARK,
  INDEX_MAX,
};

static char *level_infos[ULOG_LEVEL_NUMBER][INDEX_MAX]{
    {(char *)STR_BOLD_WHITE, (char *)STR_WHITE, (char *)"V"},    // VERBOSE
    {(char *)STR_BOLD_BLUE, (char *)STR_BLUE, (char *)"D"},      // DEBUG
    {(char *)STR_BOLD_GREEN, (char *)STR_GREEN, (char *)"I"},    // INFO
    {(char *)STR_BOLD_YELLOW, (char *)STR_YELLOW, (char *)"W"},  // WARN
    {(char *)STR_BOLD_RED, (char *)STR_RED, (char *)"E"},        // ERROR
    {(char *)STR_BOLD_PURPLE, (char *)STR_PURPLE, (char *)"A"},  // ASSERT
};

class LockGuard {
 public:
  explicit LockGuard(pthread_mutex_t &_m) : mMutex(_m) {
    pthread_mutex_lock(&mMutex);
  }

  ~LockGuard() { pthread_mutex_unlock(&mMutex); }

 private:
  pthread_mutex_t &mMutex;
};

#endif  // !ULOG_DISABLE

void logger_enable_output(uint8_t enable) {
#if !defined(ULOG_DISABLE)
  log_output_enable_ = (enable != 0);
#endif
}

void logger_enable_color(uint8_t enable) {
#if !defined(ULOG_DISABLE)
  log_color_enable_ = (enable != 0);
#endif
}

void logger_set_output_level(LoggerLevel level) {
#if !defined(ULOG_DISABLE)
  log_output_level_ = level;
#endif
}

void logger_init(LogOutputCb output_cb) {
#if !defined(ULOG_DISABLE)

  pthread_mutex_init(&log_lock_, nullptr);
  output_cb_ = output_cb;

#if defined(ULOG_CLS)
  char clear_str[3] = {'\033', 'c', '\0'};  // clean screen
  if (output_cb) output_cb(clear_str);
#endif

#endif
}

void logger_log(LoggerLevel level, const char *file, const char *func, int line,
                const char *fmt, ...) {
#if !defined(ULOG_DISABLE)

  if (!output_cb_ || !fmt || level < log_output_level_ || !log_output_enable_)
    return;

  LockGuard lock = LockGuard(log_lock_);

  char *buf_ptr = log_out_buf_;
  // The last three characters are '\r', '\n', '\0'
  char *buf_end_ptr = log_out_buf_ + LOG_OUTBUF_LEN - 3;

  char *log_info_color = log_color_enable_
                             ? level_infos[level][INDEX_SECONDARY_COLOR]
                             : (char *)"";

  /* Print serial number */
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s#%06u ", log_info_color,
           log_evt_num_++);
  buf_ptr = log_out_buf_ + strlen(log_out_buf_);

  // Print time
  struct timespec tsp {};
  clock_gettime(CLOCK_MONOTONIC, &tsp);
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "[%lu.%03lu] ", tsp.tv_sec,
           tsp.tv_nsec / (1000 * 1000));
  buf_ptr = log_out_buf_ + strlen(log_out_buf_);

  // Print level, file, function and line
  char *level_mark = level_infos[level][INDEX_LEVEL_MARK];
  char *info_str_color = (char *)(log_color_enable_ ? STR_GRAY : "");
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s%s/(%s:%d %s) ", level_mark,
           info_str_color, file, line, func);
  output_cb_(log_out_buf_);
  buf_ptr = log_out_buf_;

  // Print log info
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s", log_info_color);
  buf_ptr = log_out_buf_ + strlen(log_out_buf_);

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf_ptr, (buf_end_ptr - buf_ptr), fmt, ap);
  buf_ptr = log_out_buf_ + strlen(log_out_buf_);
  va_end(ap);

  char *str_reset_color = (char *)(log_color_enable_ ? STR_RESET : "");
  snprintf(buf_ptr, (buf_end_ptr - buf_ptr), "%s", str_reset_color);
  buf_ptr = log_out_buf_ + strlen(log_out_buf_);

  *buf_ptr++ = '\r';
  *buf_ptr++ = '\n';
  *buf_ptr = '\0';

  output_cb_(log_out_buf_);
#endif
}
