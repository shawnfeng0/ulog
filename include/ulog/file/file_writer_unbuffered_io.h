//
// Created by shawn on 25-3-13.
//

#pragma once

#include <unistd.h>

#include "file.h"
#include "file_writer_base.h"

namespace ulog::file {

class FileWriterUnbufferedIo final : public FileWriterBase {
public:
  explicit FileWriterUnbufferedIo() : file_limit_size_(kNoLimit) {}
  ~FileWriterUnbufferedIo() override { FileWriterUnbufferedIo::Close(); }

  Status Open(const std::string &filename, const bool truncate, const size_t limit) override {
    if (fd_ != -1) {
      return Status::Corruption("File already opened!", filename);
    }

    // create containing folder if not exists already.
    if (!create_dir(file::dir_name(filename))) {
      return Status::Corruption("Error creating directory", filename);
    }

    fd_ = open(filename.c_str(), O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND), 0644);
    if (fd_ == -1) {
      return Status::IOError("Error opening file", filename);
    }

    file_write_size_ = lseek(fd_, 0, SEEK_END);
    file_limit_size_ = limit;
    return Status::OK();
  }

  Status Write(const void *data, const size_t length) override {
    if (fd_ == -1) {
      return Status::Corruption("Not opened");
    }

    if (file_write_size_ + length > file_limit_size_) {
      return Status::Full();
    }

    ssize_t written = write(fd_, data, length);
    if (written == -1 || static_cast<size_t>(written) != length) {
      return Status::IOError("Error writing to file");
    }

    file_write_size_ += length;
    return Status::OK();
  }

  Status Flush() override {
    if (fd_ == -1) {
      return Status::Corruption("Not opened");
    }

    if (fsync(fd_) == -1) {
      return Status::IOError("Error flushing file");
    }
    return Status::OK();
  }

  Status Close() override {
    if (fd_ == -1) {
      return Status::Corruption("Not opened");
    }

    if (close(fd_) == -1) {
      return Status::IOError("Error closing file");
    }
    fd_ = -1;

    return Status::OK();
  }

  size_t TellP() override { return file_write_size_; }

private:
  // config
  size_t file_limit_size_;

  int fd_{-1};
  size_t file_write_size_{0};
};
}  // namespace ulog::file
