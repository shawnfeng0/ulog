//
// Created by shawn on 25-1-17.
//

#pragma once

#include <cassert>
#include <cstdio>
#include <string>

// Reference from leveldb
namespace ulog {

class Status {
 public:
  // Create a success status.
  Status() noexcept : code_(kOk), error_(nullptr) {}
  ~Status() { delete error_; }

  Status(const Status& rhs) : code_(rhs.code_), error_(rhs.error_ ? new std::string(*rhs.error_) : nullptr) {}
  Status& operator=(const Status& rhs) {
    if (this == &rhs) {
      return *this;
    }

    if (rhs.error_) {
      if (!error_) error_ = new std::string();
      *error_ = *rhs.error_;

    } else {
      delete error_;
    }

    code_ = rhs.code_;
    return *this;
  }

  Status(Status&& rhs) noexcept : code_(rhs.code_), error_(rhs.error_) {
    rhs.code_ = kOk;
    rhs.error_ = nullptr;
  }
  Status& operator=(Status&& rhs) noexcept {
    std::swap(code_, rhs.code_);
    std::swap(error_, rhs.error_);
    return *this;
  }

  // Return a success status.
  static Status OK() { return Status{}; }

  // Return error status of an appropriate type.
  static Status NotFound(const std::string& msg, const std::string& msg2 = "") { return {kNotFound, msg, msg2}; }
  static Status Corruption(const std::string& msg, const std::string& msg2 = "") { return {kCorruption, msg, msg2}; }
  static Status NotSupported(const std::string& msg, const std::string& msg2 = "") {
    return {kNotSupported, msg, msg2};
  }
  static Status InvalidArgument(const std::string& msg, const std::string& msg2 = "") {
    return {kInvalidArgument, msg, msg2};
  }
  static Status IOError(const std::string& msg, const std::string& msg2 = "") { return {kIOError, msg, msg2}; }
  static Status Full(const std::string& msg) { return {kFull, msg}; }
  static Status Full() { return Status{kFull}; }
  static Status Empty(const std::string& msg) { return {kEmpty, msg}; }
  static Status Empty() { return Status{kEmpty}; }

  // Returns true if the status indicates success.
  [[nodiscard]] bool ok() const { return code() == kOk; }
  explicit operator bool() const { return ok(); }

  // Returns true if the status indicates a NotFound error.
  [[nodiscard]] bool IsNotFound() const { return code() == kNotFound; }

  // Returns true if the status indicates a Corruption error.
  [[nodiscard]] bool IsCorruption() const { return code() == kCorruption; }

  // Returns true if the status indicates an IOError.
  [[nodiscard]] bool IsIOError() const { return code() == kIOError; }

  // Returns true if the status indicates a NotSupportedError.
  [[nodiscard]] bool IsNotSupportedError() const { return code() == kNotSupported; }

  // Returns true if the status indicates an InvalidArgument.
  [[nodiscard]] bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  // Returns true if the status indicates a Full error.
  [[nodiscard]] bool IsFull() const { return code() == kFull; }

  // Returns true if the status indicates an Empty error.
  [[nodiscard]] bool IsEmpty() const { return code() == kEmpty; }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  [[nodiscard]] std::string ToString() const;

 private:
  enum Code {
    kOk = 0,
    kNotFound = -1,
    kCorruption = -2,
    kNotSupported = -3,
    kInvalidArgument = -4,
    kIOError = -5,
    kFull = -6,
    kEmpty = -7
  };

  [[nodiscard]] Code code() const { return static_cast<Code>(code_); }

  explicit Status(const Code code) : code_(code), error_(nullptr) {}
  Status(const Code code, const std::string& msg, const std::string& msg2 = "")
      : code_(code), error_(new std::string{msg + (msg2.empty() ? "" : ": " + msg2)}) {}

  int code_;
  std::string* error_;
};

inline std::string Status::ToString() const {
  char tmp[30];
  const char* type;
  switch (code()) {
    case kOk:
      type = "OK";
      break;
    case kNotFound:
      type = "NotFound";
      break;
    case kCorruption:
      type = "Corruption";
      break;
    case kNotSupported:
      type = "Not implemented";
      break;
    case kInvalidArgument:
      type = "Invalid argument";
      break;
    case kIOError:
      type = "IO error";
      break;
    case kFull:
      type = "Full";
      break;
    case kEmpty:
      type = "Empty";
      break;
    default:
      std::snprintf(tmp, sizeof(tmp), "Unknown code(%d): ", static_cast<int>(code()));
      type = tmp;
      break;
  }
  if (!error_) {
    return type;
  }
  return std::string(type) + ": " + *error_;
}
}  // namespace ulog
