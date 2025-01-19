//
// Created by fs on 2020-05-25.
//

#pragma once

#include "file.h"
#include "writer_interface.h"

namespace ulog {

class FileLimitWriter final : public WriterInterface {
 public:
  explicit FileLimitWriter(const size_t file_limit_size) : file_limit_size_(file_limit_size) {}
  ~FileLimitWriter() override { FileLimitWriter::Close(); }

  Status Open(const std::string &filename, const bool truncate) override {
    if (file_ != nullptr) {
      return Status::Corruption("File already opened!", filename);
    }

    // create containing folder if not exists already.
    if (!file::create_dir(file::dir_name(filename))) {
      return Status::Corruption("Error creating directory", filename);
    }

    file_ = fopen(filename.c_str(), truncate ? "wb" : "ab");
    if (!file_) {
      return Status::IOError("Error opening file", filename);
    }

    file_write_size_ = truncate ? 0 : file::filesize(file_);
    return Status::OK();
  }

  Status Write(const void *data, const size_t length) override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    if (file_write_size_ + length > file_limit_size_) {
      return Status::Full();
    }

    if (std::fwrite(data, 1, length, file_) != length) {
      return Status::IOError("Error writing to file");
    }

    file_write_size_ += length;
    return Status::OK();
  }

  Status Flush() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    fflush(file_);
    return Status::OK();
  }

  Status Close() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    fclose(file_);
    file_ = nullptr;

    return Status::OK();
  }

 private:
  // config
  const size_t file_limit_size_;

  std::FILE *file_{nullptr};
  size_t file_write_size_{0};
};
}  // namespace ulog
