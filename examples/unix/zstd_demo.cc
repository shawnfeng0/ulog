#include <ulog/file/file.h>
#include <ulog/file/file_writer.h>
#include <ulog/file/zstd_file_writer.h>

#include <array>
#include <cstdio>
#include <string>

static void compress_file(const std::string& file_name, const std::string& out_name, ulog::WriterInterface& writer,
                          const int max_files) {
  /* Open the input and output files. */
  FILE* fin = fopen(file_name.c_str(), "rb");
  /* Create the input and output buffers.
   * They may be any size, but we recommend using these functions to size them.
   * Performance will only suffer significantly for very tiny buffers.
   */
  const auto open_ret = writer.Open(out_name, true);
  if (!open_ret) {
    fprintf(stderr, "%s", open_ret.ToString().c_str());
    return;
  }

  std::array<char, 1024> buff{};
  for (;;) {
    const size_t read = fread(buff.data(), 1, buff.size(), fin);
    if (read == 0) {
      break;
    }

    auto ret = writer.Write(buff.data(), read);
    if (ret.IsFull()) {
      fprintf(stderr, "File limit reached\n");
      writer.Close();
      ulog::file::RotateFiles(out_name, max_files);
      ret = writer.Open(out_name, true);
      if (!ret) {
        fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, ret.ToString().c_str());
        return;
      }

      ret = writer.Write(buff.data(), read);
      if (!ret) {
        fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, ret.ToString().c_str());
        return;
      }
    }

    if (!ret) {
      fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, ret.ToString().c_str());
      return;
    }
  }

  fclose(fin);
}

int main(const int argc, const char* argv[]) {
  const char* const exe_name = argv[0];

  if (argc < 3) {
    printf("wrong arguments\n");
    printf("usage:\n");
    printf("%s INPUT_FILE OUTPUT_FILE\n", exe_name);
    return 1;
  }

  ulog::ZstdLimitFile zstd_limit_file(5 << 20, 3);
  compress_file(argv[1], argv[2], zstd_limit_file, 5);

  return 0;
}
