#pragma once

#include <sys/stat.h>

#include <string>
#include <tuple>

namespace ulog::file {

// folder separator
static constexpr char kFolderSep = '/';

// Return true if path exists (file or directory)
// Ref:
// https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exists-using-standard-c-c11-14-17-c
static inline bool path_exists(const std::string &filename) {
  struct stat buffer{};
  return (::stat(filename.c_str(), &buffer) == 0);
}

// Return file size according to open FILE* object
static inline std::size_t filesize(FILE *f) {
  if (f == nullptr) {
    return 0;
  }

  int fd = ::fileno(f);
// 64 bits(but not in osx or cygwin, where fstat64 is deprecated)
#if (defined(__linux__) || defined(__sun) || defined(_AIX)) && (defined(__LP64__) || defined(_LP64))
  struct stat64 st{};
  if (::fstat64(fd, &st) == 0) {
    return static_cast<std::size_t>(st.st_size);
  }
#else  // other unix or linux 32 bits or cygwin
  struct stat st{};
  if (::fstat(fd, &st) == 0) {
    return static_cast<std::size_t>(st.st_size);
  }
#endif
  return 0;
}

// return true on success
static inline bool mkdir_(const std::string &path) {
  int ret = ::mkdir(path.c_str(), mode_t(0755));
  if (ret == 0 || (ret == -1 && errno == EEXIST)) {
    return true;
  }
  return false;
}

// create the given directory - and all directories leading to it
// return true on success or if the directory already exists
static inline bool create_dir(const std::string &path) {
  if (path_exists(path)) {
    return true;
  }

  if (path.empty()) {
    return true;
  }

  size_t search_offset = 0;
  do {
    auto token_pos = path.find(kFolderSep, search_offset);
    // treat the entire path as a folder if no folder separator not found
    if (token_pos == std::string::npos) {
      token_pos = path.size();
    }

    auto subdir = path.substr(0, token_pos);

    if (!subdir.empty() && !path_exists(subdir) && !mkdir_(subdir)) {
      return false;  // return error if failed creating dir
    }
    search_offset = token_pos + 1;
  } while (search_offset < path.size());

  return true;
}

// Return directory name from given path or empty string
// "abc/file" => "abc"
// "abc/" => "abc"
// "abc" => ""
// "abc///" => "abc//"
static inline std::string dir_name(const std::string &path) {
  auto pos = path.find_last_of(kFolderSep);
  return pos != std::string::npos ? path.substr(0, pos) : std::string{};
}

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
static inline std::tuple<std::string, std::string> SplitByExtension(const std::string &fname) {
  const auto ext_index = fname.rfind('.');

  // no valid extension found - return whole path and empty string as
  // extension
  if (ext_index == std::string::npos || ext_index == 0 || ext_index == fname.size() - 1) {
    return std::make_tuple(fname, std::string());
  }

  // treat cases like "/etc/rc.d/somelogfile or "/abc/.hiddenfile"
  const auto folder_index = fname.rfind(file::kFolderSep);
  if (folder_index != std::string::npos && folder_index >= ext_index - 1) {
    return std::make_tuple(fname, std::string());
  }

  // finally - return a valid base and extension tuple
  return std::make_tuple(fname.substr(0, ext_index), fname.substr(ext_index));
}

static inline std::string CalcFilename(const std::string &filename, std::size_t index) {
  if (index == 0u) {
    return filename;
  }

  std::string basename, ext;
  std::tie(basename, ext) = SplitByExtension(filename);
  return basename + "." + std::to_string(index) + ext;
}

// Rename the src file to target
// return true on success, false otherwise.
static inline bool RenameFile(const std::string &src_filename, const std::string &target_filename) {
  // If the new filename already exists, according to the posix standard,
  // rename will automatically delete the new file name, and will ensure that
  // the path of the new file is always valid. If you use remove() to delete
  // the new file first, other processes will not be able to access the new
  // file for a moment.
  // Ref:
  // https://pubs.opengroup.org/onlinepubs/000095399/functions/rename.html
  return std::rename(src_filename.c_str(), target_filename.c_str()) == 0;
}

// Rotate files:
// log.txt -> log.1.txt
// log.1.txt -> log.2.txt
// log.2.txt -> log.3.txt
// log.3.txt -> delete
static void inline RotateFiles(const std::string &filename, const size_t max_files) {
  for (auto i = max_files - 1; i > 1; --i) {
    std::string src = CalcFilename(filename, i - 1);
    if (!ulog::file::path_exists(src)) {
      continue;
    }
    std::string target = CalcFilename(filename, i);
    RenameFile(src, target);
  }

  // "tail -f" may be interrupted when rename is executed, and "tail -F" can
  // be used instead, but some "-F" implementations (busybox tail) cannot
  // obtain all logs in real time.
  RenameFile(filename, CalcFilename(filename, 1));
}

}  // namespace ulog::file
