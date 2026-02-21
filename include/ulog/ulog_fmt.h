#pragma once

// C++ logging frontend for ulog.
//
// Provides a Logger class and free functions with {fmt}-style format strings.
// Source location (file, line, function) is captured automatically at each
// call site without requiring any macros.
//
// Usage:
//   ulog::info("Hello {}", name);
//   ulog::get_default_logger().set_output_callback(my_cb);
//
//   ulog::Logger my_logger;
//   my_logger.set_output_callback(my_cb);
//   my_logger.debug("x={:.2f}", value);
//
// CMake: link against the `ulog_fmt` interface target.

#ifdef __cplusplus

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <type_traits>

// {fmt} library – either bundled (header-only) or system installation
#include <fmt/format.h>

// Platform includes for PID / TID
#include <unistd.h>
#if defined(__APPLE__)
#  include <pthread.h>
#else
#  include <sys/syscall.h>
#endif

namespace ulog {

// ---------------------------------------------------------------------------
// Log level
// ---------------------------------------------------------------------------
enum class level : int {
  trace = 0,
  debug = 1,
  info  = 2,
  warn  = 3,
  error = 4,
  fatal = 5,
  off   = 6,
};

// ---------------------------------------------------------------------------
// Output format flags (mirror the C-API flags so existing callback code stays
// compatible, but the C API itself is not used by this header)
// ---------------------------------------------------------------------------
constexpr int kFormatColor    = 1 << 0;
constexpr int kFormatNumber   = 1 << 1;
constexpr int kFormatTime     = 1 << 2;
constexpr int kFormatPid      = 1 << 3;
constexpr int kFormatLevel    = 1 << 4;
constexpr int kFormatFileLine = 1 << 5;
constexpr int kFormatFunction = 1 << 6;

constexpr int kDefaultFormat =
    kFormatColor | kFormatTime | kFormatPid |
    kFormatLevel | kFormatFileLine | kFormatFunction;

// ---------------------------------------------------------------------------
// Callback types (same ABI as the C API so existing callbacks can be reused)
// ---------------------------------------------------------------------------
using output_callback_t = int (*)(void*, const char*);
using flush_callback_t  = void (*)(void*);

// ---------------------------------------------------------------------------
// detail namespace – internal helpers, not part of the public API
// ---------------------------------------------------------------------------
namespace detail {

// Prevents Args... from being deduced via the first parameter, so that Args
// is deduced solely from the trailing arguments.
template <typename T>
struct identity {
  using type = T;
};
template <typename T>
using non_deducible = typename identity<T>::type;

// Wraps a fmt format string and captures the caller's source location via
// __builtin_FILE / __builtin_LINE / __builtin_FUNCTION default parameters.
// These builtins are evaluated at the point where loc_fmt_str is constructed
// (i.e. the actual call site), giving correct file/line/func without macros.
template <typename... Args>
struct loc_fmt_str {
  fmt::format_string<Args...> fmt;
  const char* file;
  int line;
  const char* func;

  // Explicit copy / move constructors prevent the forwarding-reference
  // constructor below from being selected when this type is passed between
  // functions.
  constexpr loc_fmt_str(const loc_fmt_str&) = default;
  constexpr loc_fmt_str(loc_fmt_str&&) = default;

  // The primary constructor: builds from a string literal (or any type
  // implicitly convertible to fmt::format_string) and captures location.
  // enable_if ensures this is not selected when S is the same loc_fmt_str
  // specialisation (copy/move ctors above take priority).
  template <typename S,
            typename = std::enable_if_t<
                !std::is_same<std::decay_t<S>, loc_fmt_str>::value>>
  constexpr loc_fmt_str(S&& s,
                        const char* f  = __builtin_FILE(),
                        int l          = __builtin_LINE(),
                        const char* fn = __builtin_FUNCTION())
      : fmt(std::forward<S>(s)), file(f), line(l), func(fn) {}
};

// ANSI color codes
constexpr const char* kColorReset  = "\x1b[0m";
constexpr const char* kColorGray   = "\x1b[38;5;8m";
constexpr const char* kColorWhite  = "\x1b[37m";
constexpr const char* kColorBlue   = "\x1b[34m";
constexpr const char* kColorGreen  = "\x1b[32m";
constexpr const char* kColorYellow = "\x1b[33m";
constexpr const char* kColorRed    = "\x1b[31m";
constexpr const char* kColorPurple = "\x1b[35m";

struct level_info {
  const char* color;
  const char* mark;
};

constexpr level_info kLevelTable[] = {
    {kColorWhite,  "T"},  // trace
    {kColorBlue,   "D"},  // debug
    {kColorGreen,  "I"},  // info
    {kColorYellow, "W"},  // warn
    {kColorRed,    "E"},  // error
    {kColorPurple, "F"},  // fatal
};

inline int get_pid() noexcept {
  static const int pid = static_cast<int>(::getpid());
  return pid;
}

inline long get_tid() noexcept {
#if defined(__APPLE__)
  return static_cast<long>(pthread_mach_thread_np(pthread_self()));
#else
  return static_cast<long>(syscall(SYS_gettid));
#endif
}

inline const char* basename(const char* path) noexcept {
  const char* s = strrchr(path, '/');
  if (!s) s = strrchr(path, '\\');
  return s ? s + 1 : path;
}

inline uint64_t real_time_us() noexcept {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return static_cast<uint64_t>(tp.tv_sec) * 1000000ULL +
         static_cast<uint64_t>(tp.tv_nsec) / 1000ULL;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Logger class
// ---------------------------------------------------------------------------
class Logger {
 public:
  Logger() = default;
  ~Logger() = default;

  Logger(const Logger&)            = delete;
  Logger& operator=(const Logger&) = delete;

  // --- Configuration ---

  void set_output_callback(output_callback_t cb,
                           void* user_data = nullptr) noexcept {
    output_cb_ = cb;
    user_data_ = user_data;
  }

  void set_flush_callback(flush_callback_t cb) noexcept { flush_cb_ = cb; }

  void set_level(level lvl) noexcept { level_ = lvl; }

  void enable_format(int flags) noexcept { format_ |= flags; }
  void disable_format(int flags) noexcept { format_ &= ~flags; }
  bool check_format(int flags) const noexcept {
    return (format_ & flags) != 0;
  }
  void enable_output(bool enable) noexcept { output_enabled_ = enable; }

  // --- Logging methods ---
  // Each method uses a loc_fmt_str wrapper as its first parameter.  The
  // wrapper's constructor default-parameter builtins are evaluated at the
  // call site, capturing file / line / func without any macros.

  template <typename... Args>
  void trace(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::trace, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void debug(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::debug, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void info(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::info, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void warn(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::warn, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void error(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::error, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void fatal(
      detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
      Args&&... args) {
    log_(level::fatal, lf.file, lf.line, lf.func,
         fmt::format(lf.fmt, std::forward<Args>(args)...));
  }

  // raw: outputs the formatted message without any log header
  template <typename... Args>
  void raw(level lvl, fmt::format_string<Args...> fmt_str, Args&&... args) {
    if (!is_enabled(lvl)) return;
    auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);
    write_(msg.c_str());
  }

 private:
  static int default_output(void*, const char* s) {
    return ::printf("%s", s);
  }

  output_callback_t     output_cb_      = default_output;
  flush_callback_t      flush_cb_       = nullptr;
  void*                 user_data_      = nullptr;
  level                 level_          = level::trace;
  int                   format_         = kDefaultFormat;
  bool                  output_enabled_ = true;
  std::atomic<uint32_t> log_num_{1};

  bool is_enabled(level lvl) const noexcept {
    return output_enabled_ && output_cb_ &&
           static_cast<int>(lvl) >= static_cast<int>(level_);
  }

  void write_(const char* s) noexcept {
    if (output_cb_) output_cb_(user_data_, s);
  }

  void log_(level lvl, const char* file, int line, const char* func,
            std::string msg) {
    if (!is_enabled(lvl)) return;

    const int li   = static_cast<int>(lvl);
    const bool col = check_format(kFormatColor);
    const auto& lv = detail::kLevelTable[li];

    std::string out;
    out.reserve(256);

    // Color prefix for header
    if (check_format(kFormatNumber | kFormatTime | kFormatLevel) && col)
      out += lv.color;

    // Serial number
    if (check_format(kFormatNumber)) {
      const uint32_t n =
          log_num_.fetch_add(1, std::memory_order_relaxed);
      out += fmt::format("#{:06} ", n);
    }

    // Timestamp
    if (check_format(kFormatTime)) {
      const uint64_t us = detail::real_time_us();
      const time_t   s  = static_cast<time_t>(us / 1000000);
      const int      ms = static_cast<int>((us % 1000000) / 1000);
      struct tm      lt = *localtime(&s);  // NOLINT: same approach as C core
      out += fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} ",
                         lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                         lt.tm_hour, lt.tm_min, lt.tm_sec, ms);
    }

    // PID-TID
    if (check_format(kFormatPid))
      out += fmt::format("{}-{} ", detail::get_pid(), detail::get_tid());

    // Level mark
    if (check_format(kFormatLevel)) out += lv.mark;

    // Gray color for source location
    if (check_format(kFormatLevel | kFormatFileLine | kFormatFunction) && col)
      out += detail::kColorGray;

    if (check_format(kFormatLevel)) out += ' ';

    // Source location
    if (check_format(kFormatFileLine | kFormatFunction)) {
      out += '(';
      if (check_format(kFormatFileLine))
        out += fmt::format("{}:{}", detail::basename(file), line);
      if (check_format(kFormatFunction)) {
        if (check_format(kFormatFileLine)) out += ' ';
        out += func;
      }
      out += ')';
    }

    if (check_format(kFormatLevel | kFormatFileLine | kFormatFunction))
      out += ' ';

    // Message with level color
    if (col) out += lv.color;
    out += msg;
    if (col) out += detail::kColorReset;
    out += '\n';

    write_(out.c_str());

    if (lvl == level::fatal && flush_cb_) flush_cb_(user_data_);
  }
};

// ---------------------------------------------------------------------------
// Global default logger
// ---------------------------------------------------------------------------

inline Logger& get_default_logger() {
  static Logger instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Free functions – use the default logger
// ---------------------------------------------------------------------------

template <typename... Args>
inline void trace(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().trace(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void debug(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().debug(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void info(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().info(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void warn(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().warn(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void error(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().error(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void fatal(
    detail::non_deducible<detail::loc_fmt_str<std::decay_t<Args>...>> lf,
    Args&&... args) {
  get_default_logger().fatal(lf, std::forward<Args>(args)...);
}

template <typename... Args>
inline void raw(level lvl, fmt::format_string<Args...> fmt_str,
                Args&&... args) {
  get_default_logger().raw(lvl, fmt_str, std::forward<Args>(args)...);
}

}  // namespace ulog

#endif  // __cplusplus
