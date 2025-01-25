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
//
// "my_folder/.mylog.txt.zst" => ("my_folder/.mylog", ".txt.zst")
// "/etc/rc.d/somelogfile" => ("/etc/rc.d/somelogfile", "")
static inline std::tuple<std::string, std::string> SplitByExtension(const std::string &filename) {
  const auto folder_index = filename.rfind(file::kFolderSep);
  const auto ext_index = filename.find('.', folder_index == std::string::npos ? 1 : folder_index + 2);

  // no valid extension found - return whole path and empty string as extension
  if (ext_index == std::string::npos || ext_index == 0 || ext_index == filename.size() - 1) {
    return std::make_tuple(filename, std::string());
  }

  // finally - return a valid base and extension tuple
  return std::make_tuple(filename.substr(0, ext_index), filename.substr(ext_index));
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

}  // namespace ulog::file
