#include "ulog/ulog.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ULOG_DEFAULT_FORMAT                                       \
  (ULOG_F_COLOR | ULOG_F_TIME | ULOG_F_LEVEL | ULOG_F_FILE_LINE | \
   ULOG_F_FUNCTION | ULOG_F_PROCESS_ID)

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
#include <pthread.h>
static inline long logger_get_tid() {
  return pthread_mach_thread_np(pthread_self());
}
#else  // defined(__APPLE__)
#include <sys/syscall.h>
static inline long logger_get_tid() { return syscall(SYS_gettid); }
#endif  // defined(__APPLE__)

struct ulog_s {
  // Internal data
  uint32_t log_evt_num_;

  // Private data set by the user will be passed to the output function
  void *user_data_;
  ulog_output_callback output_cb_;
  ulog_flush_callback flush_cb_;

  // Format configuration
  enum ulog_level_e log_level_;
  uint8_t format_;
  bool log_output_enabled_;
};

// Will be exported externally
static struct ulog_s global_logger_instance_ = {
    .log_evt_num_ = 1,

    // Logger default configuration
    .user_data_ = NULL,
    .output_cb_ = logger_printf,
    .flush_cb_ = NULL,

    .format_ = ULOG_DEFAULT_FORMAT,
    .log_output_enabled_ = true,
    .log_level_ = ULOG_LEVEL_TRACE,
};

struct ulog_s *ulog_global_logger = &global_logger_instance_;

static inline bool is_logger_valid(struct ulog_s *logger) {
  return logger && logger->output_cb_ && logger->log_output_enabled_;
}

struct ulog_s *logger_create() {
  struct ulog_s *logger = malloc(sizeof(struct ulog_s));
  if (!logger) return NULL;
  memset(logger, 0, sizeof(struct ulog_s));

  logger->log_evt_num_ = 1;

  logger->user_data_ = NULL;
  logger->output_cb_ = NULL;
  logger->flush_cb_ = NULL;

  logger->log_output_enabled_ = true;
  logger->format_ = ULOG_DEFAULT_FORMAT;
  logger->log_level_ = ULOG_LEVEL_TRACE;
  return logger;
}

void logger_destroy(struct ulog_s **logger_ptr) {
  if (!logger_ptr || !*logger_ptr) {
    return;
  }
  free(*logger_ptr);
  (*logger_ptr) = NULL;
}

#define ULOG_SET(logger, dest, source)   \
  ({                                     \
    if (logger) (logger)->dest = source; \
  })

void logger_set_user_data(struct ulog_s *logger, void *user_data) {
  ULOG_SET(logger, user_data_, user_data);
}

void logger_set_output_callback(struct ulog_s *logger,
                                ulog_output_callback output_callback) {
  ULOG_SET(logger, output_cb_, output_callback);
}

void logger_set_flush_callback(struct ulog_s *logger,
                               ulog_flush_callback flush_callback) {
  ULOG_SET(logger, flush_cb_, flush_callback);
}

void logger_enable_output(struct ulog_s *logger, bool enable) {
  (void)(logger && (logger->log_output_enabled_ = enable));
}

void logger_format_enable(struct ulog_s *logger, int32_t format) {
  (void)(logger && (logger->format_ |= format));
}

void logger_format_disable(struct ulog_s *logger, int32_t format) {
  (void)(logger && (logger->format_ &= ~format));
}

bool logger_check_format(struct ulog_s *logger, int32_t format) {
  return logger && (logger->format_ & format);
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

static inline int logger_flush(struct ulog_s *logger,
                               struct ulog_buffer_s *log_buffer) {
  int ret = 0;
  if (is_logger_valid(logger) &&
      log_buffer->cur_buf_ptr_ != log_buffer->log_out_buf_) {
    ret = logger->output_cb_(logger->user_data_, log_buffer->log_out_buf_);
    log_buffer->cur_buf_ptr_ = log_buffer->log_out_buf_;
  }
  return ret;
}

uintptr_t logger_hex_dump(struct ulog_s *logger, const void *data,
                          size_t length, size_t width, uintptr_t base_address,
                          bool tail_addr_out) {
  if (!data || width == 0 || !is_logger_valid(logger)) return 0;

  struct ulog_buffer_s log_buffer;
  loggger_buffer_init(&log_buffer);

  const uint8_t *data_raw = data;
  const uint8_t *data_cur = data;

  bool out_break = false;
  while (length) {
    logger_snprintf(&log_buffer, "%08" PRIxPTR "  ",
                    data_cur - data_raw + base_address);
    for (size_t i = 0; i < width; i++) {
      if (i < length)
        logger_snprintf(&log_buffer, "%02" PRIx8 " %s", data_cur[i],
                        i == width / 2 - 1 ? " " : "");
      else
        logger_snprintf(&log_buffer, "   %s", i == width / 2 - 1 ? " " : "");
    }
    logger_snprintf(&log_buffer, " |");
    for (size_t i = 0; i < width && i < length; i++)
      logger_snprintf(&log_buffer, "%c",
                      isprint(data_cur[i]) ? data_cur[i] : '.');
    logger_snprintf(&log_buffer, "|\r\n");

    // The output fails, in order to avoid output confusion, the rest will not
    // be output
    if (logger_flush(logger, &log_buffer) <= 0) {
      out_break = true;
      break;
    }

    data_cur += length < width ? length : width;
    length -= length < width ? length : width;
  }

  if (out_break) {
    logger_snprintf(&log_buffer, "hex dump is break!\r\n");
  } else if (tail_addr_out) {
    logger_snprintf(&log_buffer, "%08" PRIxPTR "\r\n",
                    data_cur - data_raw + base_address);
  }
  logger_flush(logger, &log_buffer);
  return data_cur - data_raw + base_address;
}

void logger_raw(struct ulog_s *logger, enum ulog_level_e level, const char *fmt,
                ...) {
  if (!is_logger_valid(logger) || !fmt || level < logger->log_level_) return;

  struct ulog_buffer_s log_buffer;
  loggger_buffer_init(&log_buffer);

  va_list ap;
  va_start(ap, fmt);
  logger_vsnprintf(&log_buffer, fmt, ap);
  va_end(ap);

  logger_flush(logger, &log_buffer);
}

void logger_log_with_header(struct ulog_s *logger, enum ulog_level_e level,
                            const char *file, const char *func, uint32_t line,
                            bool newline, bool flush, const char *fmt, ...) {
  if (!is_logger_valid(logger) || !fmt || level < logger->log_level_) return;

  struct ulog_buffer_s log_buffer;
  loggger_buffer_init(&log_buffer);

  // Color
  if (logger_check_format(logger, ULOG_F_NUMBER | ULOG_F_TIME | ULOG_F_LEVEL))
    logger_snprintf(&log_buffer, "%s",
                    logger_check_format(logger, ULOG_F_COLOR)
                        ? level_infos[level][INDEX_LEVEL_COLOR]
                        : "");

  // Print serial number
  if (logger_check_format(logger, ULOG_F_NUMBER))
    logger_snprintf(&log_buffer, "#%06" PRIu32 " ", logger->log_evt_num_++);

  // Print time
  if (logger_check_format(logger, ULOG_F_TIME)) {
    uint64_t time_ms = logger_real_time_us() / 1000;
    time_t time_s = (time_t)(time_ms / 1000);
    struct tm lt = *localtime(&time_s);
    logger_snprintf(&log_buffer, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                    lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                    lt.tm_min, lt.tm_sec, (int)(time_ms % 1000));
  }

  // Print process and thread id
  if (logger_check_format(logger, ULOG_F_PROCESS_ID))
    logger_snprintf(&log_buffer, "%" PRId32 "-%" PRId32 " ",
                    (int32_t)logger_get_pid(), (int32_t)logger_get_tid());

  // Print level
  if (logger_check_format(logger, ULOG_F_LEVEL))
    logger_snprintf(&log_buffer, "%s", level_infos[level][INDEX_LEVEL_MARK]);

  // Print gray color
  if (logger_check_format(logger,
                          ULOG_F_LEVEL | ULOG_F_FILE_LINE | ULOG_F_FUNCTION))
    logger_snprintf(
        &log_buffer, "%s",
        logger_check_format(logger, ULOG_F_COLOR) ? ULOG_STR_GRAY : "");

  if (logger_check_format(logger, ULOG_F_LEVEL))
    logger_snprintf(&log_buffer, " ");

  // Print '('
  if (logger_check_format(logger, ULOG_F_FILE_LINE | ULOG_F_FUNCTION))
    logger_snprintf(&log_buffer, "(");

  // Print file and line
  if (logger_check_format(logger, ULOG_F_FILE_LINE))
    logger_snprintf(&log_buffer, "%s:%" PRIu32, file, line);

  // Print function
  if (logger_check_format(logger, ULOG_F_FUNCTION))
    logger_snprintf(&log_buffer, "%s%s",
                    logger_check_format(logger, ULOG_F_FILE_LINE) ? " " : "",
                    func);

  // Print ')'
  if (logger_check_format(logger, ULOG_F_FILE_LINE | ULOG_F_FUNCTION))
    logger_snprintf(&log_buffer, ")");

  // Print ' '
  if (logger_check_format(logger,
                          ULOG_F_LEVEL | ULOG_F_FILE_LINE | ULOG_F_FUNCTION))
    logger_snprintf(&log_buffer, " ");

  // Print log info
  logger_snprintf(&log_buffer, "%s",
                  logger_check_format(logger, ULOG_F_COLOR)
                      ? level_infos[level][INDEX_LEVEL_COLOR]
                      : "");

  va_list ap;
  va_start(ap, fmt);
  logger_vsnprintf(&log_buffer, fmt, ap);
  va_end(ap);

  logger_snprintf(
      &log_buffer, "%s",
      logger_check_format(logger, ULOG_F_COLOR) ? ULOG_STR_RESET : "");

  if (newline) logger_snprintf(&log_buffer, "\r\n");

  if (flush) {
    logger_flush(logger, &log_buffer);

    if (level == ULOG_LEVEL_FATAL && logger->flush_cb_) {
      logger->flush_cb_(logger->user_data_);
    }
  }
}
