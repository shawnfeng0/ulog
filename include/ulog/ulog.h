#pragma once

// ulog.h — single public entry point.
//
// In C, this includes the C API: printf-style macros (LOGGER_INFO, etc.)
// and C function declarations.  Include ulog_c.h directly for the same API.
//
// In C++, this includes the C++ API: ulog::Logger class and free functions
// (ulog::info(), ulog::debug(), ...) with {fmt}-style format strings.
// Link the `ulog_fmt` CMake target to bring in the necessary dependencies.
// Include ulog_fmt.h directly for the same API.

#ifdef __cplusplus
#include "ulog/ulog_fmt.h"
#else
#include "ulog/ulog_c.h"
#endif
