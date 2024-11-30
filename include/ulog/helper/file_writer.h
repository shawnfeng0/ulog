//
// Created by fs on 2020-05-25.
//

#pragma once

#include "ulog/helper/file.h"

namespace ulog {

// Reference: https://github.com/gabime/spdlog
class FileWriter {
 public:
  FileWriter() = default;

  FileWriter(const FileWriter &) = delete;
  FileWriter &operator=(const FileWriter &) = delete;

  ~FileWriter() { Close(); }

  void Open(const std::string &fname, bool truncate = false) {
    Close();

    filename_ = fname;
    // create containing folder if not exists already.
    file::create_dir(file::dir_name(fname));

    auto *mode = truncate ? "wb" : "ab";
    fd_ = ::fopen((fname.c_str()), mode);
  }

  void Reopen(bool truncate) {
    if (filename_.empty()) {
      return;
    }
    Open(filename_, truncate);
  }

  void Flush() const { std::fflush(fd_); }

  void Close() {
    if (fd_) std::fclose(fd_);
    fd_ = nullptr;
  }

  bool Write(const void *data, std::size_t len) {
    return fd_ != nullptr && (std::fwrite(data, 1, len, fd_) == len);
  }

  std::size_t size() const { return fd_ ? file::filesize(fd_) : 0; }

  const std::string &filename() const { return filename_; }

  // return file path and its extension:
  //
  // "mylog.txt" => ("mylog", ".txt")
  // "mylog" => ("mylog", "")
  // "mylog." => ("mylog.", "")
  // "/dir1/dir2/mylog.txt" => ("/dir1/dir2/mylog", ".txt")
  //
  // the starting dot in filenames is ignored (hidden files):
  //
  // ".mylog" => (".mylog". "")
  // "my_folder/.mylog" => ("my_folder/.mylog", "")
  // "my_folder/.mylog.txt" => ("my_folder/.mylog", ".txt")
  static std::tuple<std::string, std::string> SplitByExtension(
      const std::string &fname) {
    auto ext_index = fname.rfind('.');

    // no valid extension found - return whole path and empty string as
    // extension
    if (ext_index == std::string::npos || ext_index == 0 ||
        ext_index == fname.size() - 1) {
      return std::make_tuple(fname, std::string());
    }

    // treat cases like "/etc/rc.d/somelogfile or "/abc/.hiddenfile"
    auto folder_index = fname.rfind(file::kFolderSep);
    if (folder_index != std::string::npos && folder_index >= ext_index - 1) {
      return std::make_tuple(fname, std::string());
    }

    // finally - return a valid base and extension tuple
    return std::make_tuple(fname.substr(0, ext_index), fname.substr(ext_index));
  }

 private:
  std::FILE *fd_{nullptr};
  std::string filename_;
};
}  // namespace ulog
