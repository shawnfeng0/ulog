//
// Created by fs on 2020-05-25.
//

#pragma once

#include <fstream>

#include "file.h"
#include "file_writer_base.h"

namespace ulog::file {

class FileWriterBufferedIo final : public FileWriterBase {
 public:
  explicit FileWriterBufferedIo() : file_limit_size_(kNoLimit) {}
  ~FileWriterBufferedIo() override { FileWriterBufferedIo::Close(); }

  Status Open(const std::string &filename, const bool truncate, size_t limit) override {
    if (file_stream_.is_open()) {
      return Status::Corruption("File already opened!", filename);
    }

    // create containing folder if not exists already.
    if (!create_dir(file::dir_name(filename))) {
      return Status::Corruption("Error creating directory", filename);
    }

    file_stream_.open(filename, truncate ? std::ios::trunc : std::ios::app);
    if (!file_stream_) {
      return Status::IOError("Error opening file", filename);
    }

    file_write_size_ = file_stream_.tellp();
    file_limit_size_ = limit;
    return Status::OK();
  }

  Status Write(const void *data, const size_t length) override {
    if (!file_stream_.is_open()) {
      return Status::Corruption("Not opened");
    }

    if (file_write_size_ + length > file_limit_size_) {
      return Status::Full();
    }

    if (!file_stream_.write(static_cast<const char *>(data), length).good()) {
      return Status::IOError("Error writing to file", std::to_string(file_stream_.rdstate()));
    }

    file_write_size_ += length;
    return Status::OK();
  }

  Status Flush() override {
    if (!file_stream_.is_open()) {
      return Status::Corruption("Not opened");
    }

    file_stream_.flush();
    return Status::OK();
  }

  Status Close() override {
    if (!file_stream_.is_open()) {
      return Status::Corruption("Not opened");
    }

    file_stream_.close();

    return Status::OK();
  }

  size_t TellP() override { return file_write_size_; }

 private:
  // config
  size_t file_limit_size_;

  std::ofstream file_stream_;
  size_t file_write_size_{0};
};
}  // namespace ulog::file
