#pragma once

// C++ frontend for ulog that accepts {fmt}-style format strings.
//
// Usage:
//   LOGGER_FMT_INFO("Hello {}", name);
//   LOGGER_FMT_LOCAL_DEBUG(my_logger, "x={:.2f}", 3.14);
//
// CMake: link against the `ulog_fmt` target, which pulls in either
//   std::format (C++20), system {fmt}, or a bundled header-only copy of
//   {fmt} (FMT_HEADER_ONLY) that produces no exported link symbols and
//   therefore does not conflict with any fmt the application links separately.

#ifdef __cplusplus

#include "ulog/ulog.h"

#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// Step 1: arrange the format implementation
// ---------------------------------------------------------------------------

#if defined(__cpp_lib_format)
// C++20 std::format is available – no external dependency needed.
#  include <format>
#  define ULOG_FMT_STD 1

#else
// Use the {fmt} library (system or bundled header-only via CMake).
// When FMT_HEADER_ONLY is defined (bundled path), all fmt symbols are
// inlined; no exported symbols exist, so there is no conflict with a
// separately linked system fmt.
#  include <fmt/format.h>
#  define ULOG_FMT_STD 0

#endif

// ---------------------------------------------------------------------------
// Step 2: public API – everything lives inside namespace ulog
// ---------------------------------------------------------------------------

namespace ulog {

#if ULOG_FMT_STD
template <typename... Args>
using format_string_t = std::format_string<Args...>;
#else
template <typename... Args>
using format_string_t = fmt::format_string<Args...>;
#endif

// Internal helper: format the message and hand it to the C core.
template <typename... Args>
inline void log(struct ulog_s *logger, enum ulog_level_e level,
                const char *file, const char *func, uint32_t line,
                format_string_t<Args...> fmt_str, Args &&...args) {
#if ULOG_FMT_STD
  auto msg = std::format(fmt_str, std::forward<Args>(args)...);
#else
  auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);
#endif
  logger_log_with_header(logger, level, file, func, line, true, true, "%s",
                         msg.c_str());
}

template <typename... Args>
inline void raw(struct ulog_s *logger, enum ulog_level_e level,
                format_string_t<Args...> fmt_str, Args &&...args) {
#if ULOG_FMT_STD
  auto msg = std::format(fmt_str, std::forward<Args>(args)...);
#else
  auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);
#endif
  logger_raw(logger, level, "%s", msg.c_str());
}

}  // namespace ulog

// ---------------------------------------------------------------------------
// Step 3: convenience macros mirroring the LOGGER_* / LOGGER_LOCAL_* set
// ---------------------------------------------------------------------------

#define LOGGER_FMT_LOCAL_TRACE(logger, ...)                               \
  ::ulog::log(logger, ULOG_LEVEL_TRACE, __FILENAME__, __FUNCTION__,       \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_TRACE(...) LOGGER_FMT_LOCAL_TRACE(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_DEBUG(logger, ...)                               \
  ::ulog::log(logger, ULOG_LEVEL_DEBUG, __FILENAME__, __FUNCTION__,       \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_DEBUG(...) LOGGER_FMT_LOCAL_DEBUG(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_INFO(logger, ...)                                \
  ::ulog::log(logger, ULOG_LEVEL_INFO, __FILENAME__, __FUNCTION__,        \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_INFO(...) LOGGER_FMT_LOCAL_INFO(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_WARN(logger, ...)                                \
  ::ulog::log(logger, ULOG_LEVEL_WARN, __FILENAME__, __FUNCTION__,        \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_WARN(...) LOGGER_FMT_LOCAL_WARN(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_ERROR(logger, ...)                               \
  ::ulog::log(logger, ULOG_LEVEL_ERROR, __FILENAME__, __FUNCTION__,       \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_ERROR(...) LOGGER_FMT_LOCAL_ERROR(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_FATAL(logger, ...)                               \
  ::ulog::log(logger, ULOG_LEVEL_FATAL, __FILENAME__, __FUNCTION__,       \
              __LINE__, __VA_ARGS__)
#define LOGGER_FMT_FATAL(...) LOGGER_FMT_LOCAL_FATAL(ULOG_GLOBAL, __VA_ARGS__)

#define LOGGER_FMT_LOCAL_RAW(logger, ...)                                 \
  ::ulog::raw(logger, ULOG_LEVEL_RAW, __VA_ARGS__)
#define LOGGER_FMT_RAW(...) LOGGER_FMT_LOCAL_RAW(ULOG_GLOBAL, __VA_ARGS__)

#endif  // __cplusplus
