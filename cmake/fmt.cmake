# Locate the {fmt} library.
# Prefer the copy bundled in third_party/fmt (header-only, FMT_HEADER_ONLY).
# If that directory is not present, fall back to a system installation via
# find_package.

set(_ULOG_FMT_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include")

if (EXISTS "${_ULOG_FMT_BUNDLED_DIR}/fmt/format.h")
    # Use bundled copy (header-only – no exported link symbols, no ODR conflicts).
    add_library(ulog_fmt_dep INTERFACE)
    target_include_directories(ulog_fmt_dep INTERFACE "${_ULOG_FMT_BUNDLED_DIR}")
    target_compile_definitions(ulog_fmt_dep INTERFACE FMT_HEADER_ONLY=1)
    message(STATUS "ulog_fmt: using bundled {fmt} (header-only)")
else ()
    find_package(fmt REQUIRED)
    add_library(ulog_fmt_dep INTERFACE)
    target_link_libraries(ulog_fmt_dep INTERFACE fmt::fmt)
    message(STATUS "ulog_fmt: using system {fmt} ${fmt_VERSION}")
endif ()
