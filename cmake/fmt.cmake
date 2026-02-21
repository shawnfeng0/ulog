# Find or fetch the {fmt} library.
# When found via find_package, the system installation is used directly.
# When not found, a bundled copy is downloaded via FetchContent and used in
# header-only mode (FMT_HEADER_ONLY) so that no compiled fmt symbols are
# added to the link, avoiding conflicts with any fmt the caller may already
# depend on.

find_package(fmt QUIET)

if (fmt_FOUND)
    add_library(ulog_fmt_dep INTERFACE)
    target_link_libraries(ulog_fmt_dep INTERFACE fmt::fmt)
    message(STATUS "ulog_fmt: using system {fmt} ${fmt_VERSION}")
else ()
    include(FetchContent)
    FetchContent_Declare(
            fmt
            URL https://github.com/fmtlib/fmt/archive/refs/tags/11.1.4.zip
    )
    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(fmt)

    # Use header-only mode: all fmt code is inlined into each translation unit
    # and no exported symbols are produced, so there is no symbol conflict with
    # any fmt library the application may independently link against.
    add_library(ulog_fmt_dep INTERFACE)
    target_include_directories(ulog_fmt_dep INTERFACE ${fmt_SOURCE_DIR}/include)
    target_compile_definitions(ulog_fmt_dep INTERFACE FMT_HEADER_ONLY=1)
    message(STATUS "ulog_fmt: using bundled {fmt} (header-only)")
endif ()
