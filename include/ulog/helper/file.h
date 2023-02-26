#include <sys/stat.h>

#include <string>

namespace ulog {

namespace file {

// folder separator
static constexpr char kFolderSep = '/';

// Return true if path exists (file or directory)
// Ref:
// https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exists-using-standard-c-c11-14-17-c
static inline bool path_exists(const std::string &filename) {
  struct stat buffer {};
  return (::stat(filename.c_str(), &buffer) == 0);
}

// Return file size according to open FILE* object
static inline std::size_t filesize(FILE *f) {
  if (f == nullptr) {
    return 0;
  }

  int fd = ::fileno(f);
// 64 bits(but not in osx or cygwin, where fstat64 is deprecated)
#if (defined(__linux__) || defined(__sun) || defined(_AIX)) && \
    (defined(__LP64__) || defined(_LP64))
  struct stat64 st {};
  if (::fstat64(fd, &st) == 0) {
    return static_cast<std::size_t>(st.st_size);
  }
#else  // other unix or linux 32 bits or cygwin
  struct stat st {};
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
    return false;
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

}  // namespace file
}  // namespace ulog
