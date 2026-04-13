#include "ulog/ulog_fmt.h"

// Also verify that ulog.h auto-includes the C++ frontend when ULOG_FMT_AVAILABLE
// is defined (which it is when linking the ulog_fmt CMake target).
#include "ulog/ulog.h"

#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Captures all output written to a Logger instance
class OutputCapture {
 public:
  explicit OutputCapture(ulog::Logger& logger) {
    logger.set_output_callback(Callback, this);
  }
  const std::string& str() const { return buf_; }
  void clear() { buf_.clear(); }

 private:
  std::string buf_;
  static int Callback(void* self, const char* s) {
    static_cast<OutputCapture*>(self)->buf_ += s;
    return static_cast<int>(strlen(s));
  }
};

// ---------------------------------------------------------------------------
// All log levels produce output
// ---------------------------------------------------------------------------
TEST(UlogFmt, AllLevels) {
  ulog::Logger logger;
  OutputCapture cap(logger);

  logger.trace("trace {}", 1);
  logger.debug("debug {}", 2);
  logger.info("info {}", 3);
  logger.warn("warn {}", 4);
  logger.error("error {}", 5);
  logger.fatal("fatal {}", 6);

  EXPECT_NE(cap.str().find("trace 1"), std::string::npos);
  EXPECT_NE(cap.str().find("debug 2"), std::string::npos);
  EXPECT_NE(cap.str().find("info 3"), std::string::npos);
  EXPECT_NE(cap.str().find("warn 4"), std::string::npos);
  EXPECT_NE(cap.str().find("error 5"), std::string::npos);
  EXPECT_NE(cap.str().find("fatal 6"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Level filtering works
// ---------------------------------------------------------------------------
TEST(UlogFmt, LevelFilter) {
  ulog::Logger logger;
  OutputCapture cap(logger);

  logger.set_level(ulog::level::warn);
  logger.trace("filtered trace");
  logger.debug("filtered debug");
  logger.info("filtered info");
  logger.warn("visible warn");
  logger.error("visible error");

  EXPECT_EQ(cap.str().find("filtered"), std::string::npos);
  EXPECT_NE(cap.str().find("visible warn"), std::string::npos);
  EXPECT_NE(cap.str().find("visible error"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Source location is automatically captured (filename appears in output)
// ---------------------------------------------------------------------------
TEST(UlogFmt, SourceLocation) {
  ulog::Logger logger;
  OutputCapture cap(logger);

  logger.info("location test");

  // The captured output should contain this file's name
  EXPECT_NE(cap.str().find("ulog_fmt_test"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Various format specifiers
// ---------------------------------------------------------------------------
TEST(UlogFmt, FormatSpecifiers) {
  ulog::Logger logger;
  OutputCapture cap(logger);

  logger.info("int={} float={:.2f} str={} hex={:#x}", 42, 3.14, "hi", 255);

  EXPECT_NE(cap.str().find("int=42"), std::string::npos);
  EXPECT_NE(cap.str().find("float=3.14"), std::string::npos);
  EXPECT_NE(cap.str().find("str=hi"), std::string::npos);
  EXPECT_NE(cap.str().find("hex=0xff"), std::string::npos);
}

// ---------------------------------------------------------------------------
// raw() produces output without a header
// ---------------------------------------------------------------------------
TEST(UlogFmt, RawOutput) {
  ulog::Logger logger;
  OutputCapture cap(logger);
  // Disable all format flags so a normal log would emit nothing but the msg
  logger.disable_format(ulog::kFormatTime | ulog::kFormatPid |
                        ulog::kFormatLevel | ulog::kFormatFileLine |
                        ulog::kFormatFunction | ulog::kFormatColor);
  cap.clear();

  logger.raw(ulog::level::info, "raw {}", 7);

  EXPECT_NE(cap.str().find("raw 7"), std::string::npos);
  // raw() should not include a trailing newline from the header logic
  EXPECT_EQ(cap.str().find('\n'), std::string::npos);
}

// ---------------------------------------------------------------------------
// Auto-include via ulog.h: ulog::info() etc. are available after including
// just ulog.h when the ulog_fmt CMake target is linked.
// ---------------------------------------------------------------------------
TEST(UlogFmt, AutoIncludeViaUlogH) {
  // The ulog:: namespace is available — this would fail to compile if
  // ulog.h did not pull in ulog_fmt.h.
  ulog::get_default_logger().set_output_callback(
      [](void*, const char*) -> int { return 0; });
  ulog::info("auto-include check {}", 42);
}

// ---------------------------------------------------------------------------
// Free functions compile and forward to the default logger
// ---------------------------------------------------------------------------
TEST(UlogFmt, FreeFunctions) {
  // Set a no-op output so the global logger doesn't write to stdout
  ulog::get_default_logger().set_output_callback(
      [](void*, const char*) -> int { return 0; });

  // These must compile and not crash
  ulog::trace("trace free {}", 1);
  ulog::debug("debug free {}", 2);
  ulog::info("info free {}", 3);
  ulog::warn("warn free {}", 4);
  ulog::error("error free {}", 5);
  ulog::fatal("fatal free {}", 6);
  ulog::raw(ulog::level::info, "raw free {}", 7);
}
