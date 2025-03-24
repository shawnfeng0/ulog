# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

[Unreleased]: https://github.com/ShawnFeng0/ulog/compare/v0.6.1...HEAD

## [0.6.1] - 2025-03-25

[0.6.1]: https://github.com/ShawnFeng0/ulog/compare/v0.6.0...v0.6.1

### Added

* logroller: Add native unbuffered write interface, enabled by default when logroller is compressed

### Changed

* file: limit_size_file no longer automatically adds the "-head" suffix, but is user-defined
* logroller: Set up SCHED_BATCH scheduling strategy as a background task

### Fixed

* fix: log sequence number does not support multithreading

## [0.6.0] - 2025-02-08

[0.6.0]: https://github.com/ShawnFeng0/ulog/compare/v0.5.0...v0.6.0

### Added

* Add lock-free and wait-free spsc (single producer-single consumer) and mpsc (multiple producers-single consumer) queue
  libraries based on bip-buffer algorithm
* logroller: Supports zstd real-time compression
* logroller: Support incremental file rotation strategy
* async_file: Add file header callback function

### Changed

* logrotate -> logroller: To avoid naming conflicts with logrotate, change the name to logroller
* logroller: SIZE and TIME parameters support string formats such as kb/mb and s/min/hour
* logroller: Improve performance, using lock-free spsc queue implementation

## [0.5.0] - 2024-11-20

[0.5.0]: https://github.com/ShawnFeng0/ulog/compare/v0.4.2...v0.5.0

### Added

### Changed

* Delete log lock function
* flush: Automatically execute flush only when logging at FATAL level
* Format modification uses a unified function: logger_check_format/logger_format_disable/logger_format_enable
* Remove the stdout function for file storage
* CI: Change the CI provider from travis-ci to github workflows
* Use "\n" instead of "\r\n" for line breaks
* format: Level and debug information are separated by " " instead of "/"

### Fixed

* remove() does not need to be executed when file is rotated

## [0.4.2] - 2023-02-09

[0.4.2]: https://github.com/ShawnFeng0/ulog/compare/v0.4.1...v0.4.2

### Fixed

* Fix: logrotate compilation error: pthread symbol missing

## [0.4.1] - 2023-02-08

[0.4.1]: https://github.com/ShawnFeng0/ulog/compare/v0.4.0...v0.4.1

### Added

* Added command line tool logrotate for circular logging to file

## [0.4.0] - 2022-09-08

[0.4.0]: https://github.com/ShawnFeng0/ulog/compare/v0.3.1...v0.4.0

### Added

* Add MIT license
* Add changelog file

### Fixed

* Resolve compile warnings under -W -Wall -Waddress compile options
* TIME_CODE() and TOKEN(string) cannot turn off color output
* Token output is not limited by level

## [0.3.1] - 2021-06-21

[0.3.1]: https://github.com/ShawnFeng0/ulog/compare/v0.3.0...v0.3.1

### Added

* Execute flush operation when outputting logs above the error level
* Support setting flush callback function

### Changed

* Use enum ulog_level_e to replace LogLevel to avoid naming conflicts with other projects
* Set ulog cmake public header file path (use modern cmake target_include_directories(xxx PUBLIC xxx) syntax)
* Optimize the internal processing of the snprintf function to reduce the call of strlen()
* Set user_data and output_callback separately, keeping each setting independent, keep the logger_create function simple

### Fixed

* Support signed char type token printing

## [0.3.0] - 2021-02-04

[0.3.0]: https://github.com/ShawnFeng0/ulog/compare/v0.2.0...v0.3.0

### Added

* Support the creation of different logger instances

## [0.2.0] - 2020-11-26

[0.2.0]:  https://github.com/ShawnFeng0/ulog/compare/v0.1.2...v0.2.0

### Added

* Add user private data settings to make the output more flexible
* Add examples of asynchronous file output and auxiliary classes

### Changed

* Use LOGGER_XXX macros instead of LOG_XXX to avoid naming conflicts
* Remove the brackets from the time information

### Fixed

* fix: It is executed twice when the log parameter is a function

### Removed

* Delete the ABORT() and ASSERT() auxiliary functions, it should not be the responsibility of the log library

## [0.1.2] - 2020-05-25

[0.1.2]: https://github.com/ShawnFeng0/ulog/compare/v0.1.1...v0.1.2

### Removed

* Delete too many macro definitions (try to use functions instead of macros):
* Delete ULOG_DISABLE macro
* Delete ULOG_DEFAULT_LEVEL macro
* Delete ULOG_DISABLE_COLOR macro
* Delete ULOG_CLS macro

### Fixed

* After the first line of hexdump indicates that the output fails, the output will not continue

## [0.1.1] - 2020-05-22

[0.1.1]: https://github.com/ShawnFeng0/ulog/compare/v0.1.0...v0.1.1

### Changed

* LOG_TIME_CODE uses microseconds as the output unit
* Make asynchronous logs block waiting

## [0.1.0] - 2020-02-07

[0.1.0]: https://github.com/ShawnFeng0/ulog/releases/tag/v0.1.0

* Realize basic functions
* Various log levels output, rich colors, hexdump output, token output, time_code statistics

* Add fifo for asynchronous output
