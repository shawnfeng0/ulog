#pragma once

// ulog.h — single public entry point.
//
// Always provides the C API: printf-style macros (LOGGER_INFO, etc.) and
// C function declarations.  The same API is also available directly via
// ulog_c.h.
//
// In C++ translation units, also provides the C++ API (ulog::Logger class
// and free functions such as ulog::info()) when the `ulog_fmt` CMake target
// is linked.  That target sets ULOG_FMT_AVAILABLE=1 and brings in the
// necessary fmt/std::format dependency.  The same API is also available
// directly via ulog_fmt.h.

#include "ulog/ulog_c.h"

#ifdef __cplusplus
#ifdef ULOG_FMT_AVAILABLE
#include "ulog/ulog_fmt.h"
#endif
#endif
