#include "ulog/ulog.h"

#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef ULOG_OUTBUF_LEN
#define ULOG_OUTBUF_LEN 512 /* Size of buffer used for log printout */
#endif

#if ULOG_OUTBUF_LEN < 64
#pragma message("ULOG_OUTBUF_LEN is recommended to be greater than 64")
#endif

enum {
  INDEX_LEVEL_COLOR,
  INDEX_LEVEL_MARK,
  INDEX_MAX,
};

static const char *level_infos[ULOG_LEVEL_NUMBER][INDEX_MAX] = {
    {ULOG_STR_WHITE, "T"},   // TRACE
    {ULOG_STR_BLUE, "D"},    // DEBUG
    {ULOG_STR_GREEN, "I"},   // INFO
    {ULOG_STR_YELLOW, "W"},  // WARN
    {ULOG_STR_RED, "E"},     // ERROR
    {ULOG_STR_PURPLE, "F"},  // FATAL
};

static inline int logger_printf(void *unused, const char *str) {
  (void)unused;
  return printf("%s", str);
}

static inline int logger_get_pid() {
  static int process_id = -1;
  if (process_id == -1) {
    process_id = getpid();
  }
  return process_id;
}

// Get and thread id
#if defined(__APPLE__)
static inline long logger_get_tid() {
  return pthread_mach_thread_np(pthread_self());
}
#else  // defined(__APPLE__)
#include <sys/syscall.h>
static inline long logger_get_tid() { return syscall(SYS_gettid); }
#endif  // defined(__APPLE__)

struct ulog_s {
  // Internal data
  char log_out_buf_[ULOG_OUTBUF_LEN];
  char *cur_buf_ptr_;
  uint32_t log_evt_num_;
  pthread_mutex_t mutex_;

  // Private data set by the user will be passed to the output function
  void *user_data_;
  ulog_output_callback output_cb_;
  ulog_flush_callback flush_cb_;

  // Format configuration
  bool log_output_enabled_;
  bool log_process_id_enabled_;
  bool log_color_enabled_;
  bool log_number_enabled_;
  bool log_time_enabled_;
  bool log_level_enabled_;
  bool log_file_line_enabled_;
  bool log_function_enabled_;
  enum ulog_level_e log_level_;
};

// Will be exported externally
static struct ulog_s global_logger_instance_ = {
    .cur_buf_ptr_ = global_logger_instance_.log_out_buf_,
    .log_evt_num_ = 1,
    .mutex_ = PTHREAD_MUTEX_INITIALIZER,

    // Logger default configuration
    .user_data_ = NULL,
    .output_cb_ = logger_printf,
    .flush_cb_ = NULL,

    .log_process_id_enabled_ = true,
    .log_output_enabled_ = true,
    .log_color_enabled_ = true,
    .log_number_enabled_ = false,
    .log_time_enabled_ = true,
    .log_level_enabled_ = true,
    .log_file_line_enabled_ = true,
    .log_function_enabled_ = true,
    .log_level_ = ULOG_LEVEL_TRACE,
};

struct ulog_s *ulog_global_logger = &global_logger_instance_;

static inline int logger_vsnprintf(struct ulog_s *logger, const char *fmt,
                                   va_list ap) {
  char *buffer_end = logger->log_out_buf_ + sizeof(logger->log_out_buf_);
  ssize_t buffer_length = buffer_end - logger->cur_buf_ptr_;

  int expected_length =
      vsnprintf((logger)->cur_buf_ptr_, buffer_length, fmt, ap);

  if (expected_length < buffer_length) {
    logger->cur_buf_ptr_ += expected_length;
  } else {
    // The buffer is filled, pointing to terminating null byte ('\0')
    logger->cur_buf_ptr_ = buffer_end - 1;
  }
  return expected_length;
}

static inline int logger_snprintf(struct ulog_s *logger, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int expected_length = logger_vsnprintf(logger, fmt, ap);
  va_end(ap);
  return expected_length;
}

static inline bool is_logger_valid(struct ulog_s *logger) {
  return logger && logger->output_cb_ && logger->log_output_enabled_;
}

struct ulog_s *logger_create() {
  struct ulog_s *logger = malloc(sizeof(struct ulog_s));
  if (!logger) return NULL;
  memset(logger, 0, sizeof(struct ulog_s));

  logger->cur_buf_ptr_ = logger->log_out_buf_;
  logger->log_evt_num_ = 1;
  pthread_mutex_init(&logger->mutex_, NULL);

  logger->user_data_ = NULL;
  logger->output_cb_ = NULL;
  logger->flush_cb_ = NULL;

  logger->log_output_enabled_ = true;
  logger->log_process_id_enabled_ = true;
  logger->log_color_enabled_ = true;
  logger->log_number_enabled_ = false;
  logger->log_time_enabled_ = true;
  logger->log_level_enabled_ = true;
  logger->log_file_line_enabled_ = true;
  logger->log_function_enabled_ = true;
  logger->log_level_ = ULOG_LEVEL_TRACE;
  return logger;
}

void logger_destroy(struct ulog_s **logger_ptr) {
  if (!logger_ptr || !*logger_ptr) {
    return;
  }
  pthread_mutex_destroy(&(*logger_ptr)->mutex_);
  free(*logger_ptr);
  (*logger_ptr) = NULL;
}

#define LOCK_AND_SET(logger, dest, source) \
  ({                                       \
    if (!(logger)) return;                 \
    logger_output_lock(logger);            \
    (logger)->dest = source;               \
    logger_output_unlock(logger);          \
  })

void logger_set_user_data(struct ulog_s *logger, void *user_data) {
  LOCK_AND_SET(logger, user_data_, user_data);
}

void logger_set_output_callback(struct ulog_s *logger,
                                ulog_output_callback output_callback) {
  LOCK_AND_SET(logger, output_cb_, output_callback);
}

void logger_set_flush_callback(struct ulog_s *logger,
                               ulog_flush_callback flush_callback) {
  LOCK_AND_SET(logger, flush_cb_, flush_callback);
}

void logger_enable_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_output_enabled_ = enable));
}

void logger_enable_color(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_color_enabled_ = enable));
}

bool logger_color_is_enabled(struct ulog_s *logger) {
  return logger && logger->log_color_enabled_;
}

void logger_enable_number_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_number_enabled_ = enable));
}

void logger_enable_time_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_time_enabled_ = enable));
}

void logger_enable_process_id_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_process_id_enabled_ = enable));
}

void logger_enable_level_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_level_enabled_ = enable));
}

void logger_enable_file_line_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_file_line_enabled_ = enable));
}

void logger_enable_function_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_function_enabled_ = enable));
}

void logger_set_output_level(struct ulog_s *logger, enum ulog_level_e level) {
  (void)(logger && (logger->log_level_ = level));
}

uint64_t logger_real_time_us() {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t)(tp.tv_sec) * 1000 * 1000 + tp.tv_nsec / 1000;
}

uint64_t logger_monotonic_time_us() {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return (uint64_t)(tp.tv_sec) * 1000 * 1000 + tp.tv_nsec / 1000;
}

int logger_output_lock(struct ulog_s *logger) {
  return logger ? pthread_mutex_lock(&logger->mutex_) : -1;
}

int logger_output_unlock(struct ulog_s *logger) {
  return logger ? pthread_mutex_unlock(&logger->mutex_) : -1;
}

uintptr_t logger_nolock_hex_dump(struct ulog_s *logger, const void *data,
                                 size_t length, size_t width,
                                 uintptr_t base_address, bool tail_addr_out) {
  if (!data || width == 0 || !is_logger_valid(logger)) return 0;

  const uint8_t *data_raw = data;
  const uint8_t *data_cur = data;

  // The output fails, in order to avoid output confusion, the rest will not
  // be output
  if (logger_nolock_flush(logger) <= 0) {
    return 0;
  }

  bool out_break = false;
  while (length) {
    logger_snprintf(logger, "%08" PRIxPTR "  ",
                    data_cur - data_raw + base_address);
    for (size_t i = 0; i < width; i++) {
      if (i < length)
        logger_snprintf(logger, "%02" PRIx8 " %s", data_cur[i],
                        i == width / 2 - 1 ? " " : "");
      else
        logger_snprintf(logger, "   %s", i == width / 2 - 1 ? " " : "");
    }
    logger_snprintf(logger, " |");
    for (size_t i = 0; i < width && i < length; i++)
      logger_snprintf(logger, "%c", isprint(data_cur[i]) ? data_cur[i] : '.');
    logger_snprintf(logger, "|\r\n");

    // The output fails, in order to avoid output confusion, the rest will not
    // be output
    if (logger_nolock_flush(logger) <= 0) {
      out_break = true;
      break;
    }

    data_cur += length < width ? length : width;
    length -= length < width ? length : width;
  }

  if (out_break) {
    logger_snprintf(logger, "hex dump is break!\r\n");
  } else if (tail_addr_out) {
    logger_snprintf(logger, "%08" PRIxPTR "\r\n",
                    data_cur - data_raw + base_address);
  }
  logger_nolock_flush(logger);
  return data_cur - data_raw + base_address;
}

static void logger_raw_internal(struct ulog_s *logger, bool lock_and_flush,
                                const char *fmt, va_list ap) {
  if (!is_logger_valid(logger) || !fmt) return;

  if (lock_and_flush) logger_output_lock(logger);

  logger_vsnprintf(logger, fmt, ap);

  if (lock_and_flush) {
    logger_nolock_flush(logger);
    logger_output_unlock(logger);
  }
}

void logger_raw(struct ulog_s *logger, bool lock_and_flush, const char *fmt,
                ...) {
  va_list ap;
  va_start(ap, fmt);
  logger_raw_internal(logger, lock_and_flush, fmt, ap);
  va_end(ap);
}

void logger_raw_no_format_check(struct ulog_s *logger, bool lock_and_flush,
                                const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logger_raw_internal(logger, lock_and_flush, fmt, ap);
  va_end(ap);
}

int logger_nolock_flush(struct ulog_s *logger) {
  int ret = 0;
  if (is_logger_valid(logger) && logger->cur_buf_ptr_ != logger->log_out_buf_) {
    ret = logger->output_cb_(logger->user_data_, logger->log_out_buf_);
    logger->cur_buf_ptr_ = logger->log_out_buf_;
  }
  return ret;
}

static void logger_log_internal(struct ulog_s *logger, enum ulog_level_e level,
                                const char *file, const char *func,
                                uint32_t line, bool newline,
                                bool lock_and_flush, const char *fmt,
                                va_list ap) {
  if (!is_logger_valid(logger) || !fmt || level < logger->log_level_) return;

  if (lock_and_flush) logger_output_lock(logger);

  // Color
  if (logger->log_number_enabled_ || logger->log_time_enabled_ ||
      logger->log_level_enabled_)
    logger_snprintf(logger, "%s",
                    logger->log_color_enabled_
                        ? level_infos[level][INDEX_LEVEL_COLOR]
                        : "");

  // Print serial number
  if (logger->log_number_enabled_)
    logger_snprintf(logger, "#%06" PRIu32 " ", logger->log_evt_num_++);

  // Print time
  if (logger->log_time_enabled_) {
    uint64_t time_ms = logger_real_time_us() / 1000;
    time_t time_s = time_ms / 1000;
    struct tm lt = *localtime(&time_s);
    logger_snprintf(logger, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                    lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                    lt.tm_min, lt.tm_sec, (int)(time_ms % 1000));
  }

  // Print process and thread id
  if (logger->log_process_id_enabled_)
    logger_snprintf(logger, "%" PRId32 "-%" PRId32 " ",
                    (int32_t)logger_get_pid(), (int32_t)logger_get_tid());

  // Print level
  if (logger->log_level_enabled_)
    logger_snprintf(logger, "%s", level_infos[level][INDEX_LEVEL_MARK]);

  // Print gray color
  if (logger->log_level_enabled_ || logger->log_file_line_enabled_ ||
      logger->log_function_enabled_)
    logger_snprintf(logger, "%s",
                    logger->log_color_enabled_ ? ULOG_STR_GRAY : "");

  // Print '/'
  if (logger->log_level_enabled_) logger_snprintf(logger, "/");

  // Print '('
  if (logger->log_file_line_enabled_ || logger->log_function_enabled_)
    logger_snprintf(logger, "(");

  // Print file and line
  if (logger->log_file_line_enabled_)
    logger_snprintf(logger, "%s:%" PRIu32, file, line);

  // Print function
  if (logger->log_function_enabled_)
    logger_snprintf(logger, "%s%s", logger->log_file_line_enabled_ ? " " : "",
                    func);

  // Print ')'
  if (logger->log_file_line_enabled_ || logger->log_function_enabled_)
    logger_snprintf(logger, ")");

  // Print ' '
  if (logger->log_level_enabled_ || logger->log_file_line_enabled_ ||
      logger->log_function_enabled_)
    logger_snprintf(logger, " ");

  // Print log info
  logger_snprintf(
      logger, "%s",
      logger->log_color_enabled_ ? level_infos[level][INDEX_LEVEL_COLOR] : "");

  logger_vsnprintf(logger, fmt, ap);

  logger_snprintf(logger, "%s",
                  logger->log_color_enabled_ ? ULOG_STR_RESET : "");

  if (newline) logger_snprintf(logger, "\r\n");

  if (lock_and_flush) {
    logger_nolock_flush(logger);

    if (level >= ULOG_LEVEL_ERROR && logger->flush_cb_) {
      logger->flush_cb_(logger->user_data_);
    }

    logger_output_unlock(logger);
  }
}

void logger_log_no_format_check(struct ulog_s *logger, enum ulog_level_e level,
                                const char *file, const char *func,
                                uint32_t line, bool newline,
                                bool lock_and_flush, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logger_log_internal(logger, level, file, func, line, newline, lock_and_flush,
                      fmt, ap);
  va_end(ap);
}

void logger_log(struct ulog_s *logger, enum ulog_level_e level,
                const char *file, const char *func, uint32_t line, bool newline,
                bool lock_and_flush, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logger_log_internal(logger, level, file, func, line, newline, lock_and_flush,
                      fmt, ap);
  va_end(ap);
}
