#pragma once

#include <string>
#include <utility>

#include "ulog/file/file.h"
#include "ulog/file/rotation_strategy_interface.h"

namespace ulog::file {

class RotationStrategyRename final : public RotationStrategyInterface {
 public:
  RotationStrategyRename(std::string basename, std::string ext, const size_t max_files)
      : basename_(std::move(basename)), ext_(std::move(ext)), max_files_(max_files == 0 ? 1 : max_files) {}

  // Rotate files:
  // log.txt -> log.1.txt
  // log.1.txt -> log.2.txt
  // log.2.txt -> log.3.txt
  // log.3.txt -> delete
  // "tail -f" may be interrupted when rename is executed, and "tail -F" can
  // be used instead, but some "-F" implementations (busybox tail) cannot
  // obtain all logs in real time.
  Status Rotate() override {
    for (size_t i = max_files_ - 1; i > 0; --i) {
      std::string src = get_filename(i - 1);
      if (!path_exists(src)) {
        continue;
      }
      RenameFile(src, get_filename(i));
    }

    // Try to clear the files that were previously configured too much
    size_t not_exists = 0;
    for (size_t i = max_files_;; ++i) {
      std::string filename = get_filename(i);
      if (!path_exists(filename)) {
        if (++not_exists == 2) break;
        continue;
      }
      std::remove(filename.c_str());
    }
    return Status::OK();
  }

  std::string LatestFilename() override { return basename_ + ext_; }

 private:
  [[nodiscard]] std::string get_filename(const size_t index) const {
    if (index == 0u) {
      return basename_ + ext_;
    }

    return basename_ + "." + std::to_string(index) + ext_;
  }
  std::string basename_;
  std::string ext_;
  size_t max_files_;
};

}  // namespace ulog::file