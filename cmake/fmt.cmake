# Locate the {fmt} formatting library, using this priority order:
#
#   1. C++20 standard library std::format  (no external dependency)
#   2. System-installed {fmt} library      (find_package)
#   3. Automatic download of a fixed-version {fmt} via FetchContent

# --- Priority 1: C++20 std::format ---
# Temporarily require C++20 for the detection test, then restore the
# project standard. Without this, CMake appends the project's
# -std=c++17 flag AFTER our required-flags entry, overriding it.
include(CheckCXXSourceCompiles)
set(_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 20)
check_cxx_source_compiles([=[
#include <format>
int main() { return std::format("{}", 42).size() > 0 ? 0 : 1; }
]=] ULOG_HAS_STD_FORMAT)
set(CMAKE_CXX_STANDARD "${_saved_cxx_standard}")
unset(_saved_cxx_standard)

if (ULOG_HAS_STD_FORMAT)
    add_library(ulog_fmt_dep INTERFACE)
    target_compile_features(ulog_fmt_dep INTERFACE cxx_std_20)
    target_compile_definitions(ulog_fmt_dep INTERFACE ULOG_FMT_USE_STD=1)
    message(STATUS "ulog_fmt: using std::format (C++20 standard library)")
    return()
endif ()

# --- Priority 2: system-installed {fmt} ---
find_package(fmt QUIET)
if (fmt_FOUND)
    add_library(ulog_fmt_dep INTERFACE)
    target_link_libraries(ulog_fmt_dep INTERFACE fmt::fmt)
    message(STATUS "ulog_fmt: using system {fmt} ${fmt_VERSION}")
    return()
endif ()

# --- Priority 3: download {fmt} 11.1.4 via FetchContent ---
include(FetchContent)
FetchContent_Declare(
        fmt
        URL https://github.com/fmtlib/fmt/archive/refs/tags/11.1.4.zip
)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)
add_library(ulog_fmt_dep INTERFACE)
target_include_directories(ulog_fmt_dep INTERFACE ${fmt_SOURCE_DIR}/include)
target_compile_definitions(ulog_fmt_dep INTERFACE FMT_HEADER_ONLY=1)
message(STATUS "ulog_fmt: using downloaded {fmt} 11.1.4 (header-only)")
