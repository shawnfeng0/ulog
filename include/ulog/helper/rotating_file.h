//
// Created by fs on 2020-05-25.
//

#ifndef ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_
#define ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_

#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "ulog/helper/file_writer.h"

namespace ulog {

//
// Reference: https://github.com/gabime/spdlog
// Rotating file sink based on size
//
class RotatingFile {
 public:
  RotatingFile(std::string base_filename, std::size_t max_size,
               std::size_t max_files, bool rotate_on_open = false,
               bool copy_and_truncate = false)
      : base_filename_(std::move(base_filename)),
        max_size_(max_size),
        max_files_(max_files),
        copy_and_truncate_(copy_and_truncate) {
    file_writer_.Open(CalcFilename(base_filename_, 0));
    current_size_ = file_writer_.size();  // expensive. called only once
    if (rotate_on_open && current_size_ > 0) {
      Rotate();
    }
  }
  std::string filename() { return file_writer_.filename(); }

  void SinkIt(const void *buffer, size_t size) {
    current_size_ += size;
    if (current_size_ > max_size_) {
      Rotate();
      current_size_ = size;
    }
    file_writer_.Write(buffer, size);
  }

  void Flush() { file_writer_.Flush(); }

 private:
  using filename_t = std::string;

  // Rotate files:
  // log.txt -> log.1.txt
  // log.1.txt -> log.2.txt
  // log.2.txt -> log.3.txt
  // log.3.txt -> delete
  void Rotate() {
    file_writer_.Close();

    for (auto i = max_files_; i > 1; --i) {
      filename_t src = CalcFilename(base_filename_, i - 1);
      if (!file::path_exists(src)) {
        continue;
      }
      filename_t target = CalcFilename(base_filename_, i);
      RenameFile(src, target);
    }

    if (copy_and_truncate_) {
      // In order to use tail -f to read files in real time, need to use copy
      // instead of rename
      CopyFile(base_filename_, CalcFilename(base_filename_, 1));
    } else {
      // "tail -"f may be interrupted when rename is executed, and "tail -F" can
      // be used instead, but some "-F" implementations (busybox tail) cannot
      // obtain all logs in real time.
      RenameFile(base_filename_, CalcFilename(base_filename_, 1));
    }

    file_writer_.Reopen(true);
  }

  template <typename T>
  static std::string to_string(const T &n) {
    std::ostringstream stm;
    stm << n;
    return stm.str();
  }

  static std::string CalcFilename(const std::string &filename,
                                  std::size_t index) {
    if (index == 0u) {
      return filename;
    }

    filename_t basename, ext;
    std::tie(basename, ext) = FileWriter::SplitByExtension(filename);
    return basename + "." + to_string(index) + ext;
  }

  // Copy file
  // Reference:
  // https://stackoverflow.com/questions/10195343/copy-a-file-in-a-sane-safe-and-efficient-way
  static void CopyFile(const std::string &src, const std::string &dst) {
    std::ifstream src_in(src, std::ios::binary);
    std::ofstream dst_out(dst, std::ios::binary);

    dst_out << src_in.rdbuf();
  }

  // Rename the src file to target
  // return true on success, false otherwise.
  static bool RenameFile(const std::string &src_filename,
                         const std::string &target_filename) {
    // If the new filename already exists, according to the posix standard,
    // rename will automatically delete the new file name, and will ensure that
    // the path of the new file is always valid. If you use remove() to delete
    // the new file first, other processes will not be able to access the new
    // file for a moment.
    // Ref:
    // https://pubs.opengroup.org/onlinepubs/000095399/functions/rename.html
    return std::rename(src_filename.c_str(), target_filename.c_str()) == 0;
  }

  std::string base_filename_;
  std::size_t max_size_;
  std::size_t max_files_;
  std::size_t current_size_;
  bool copy_and_truncate_;  // copy and truncate the file instead of rename, for
                            // "tail -f" support
  FileWriter file_writer_;
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_
