#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <cstdlib>
#include <unistd.h>

#include "cmdline.h"
#include "ulog/helper/async_rotating_file.h"
#include "ulog/ulog.h"

int main(int argc, char *argv[]) {
  gengetopt_args_info args_info{};

  if (cmdline_parser(argc, argv, &args_info) != 0) exit(1);

  const ulog::AsyncRotatingFile async_rotate(
      args_info.fifo_size_arg, args_info.file_path_arg, args_info.file_size_arg,
      args_info.file_number_arg, args_info.flush_interval_arg);

  cmdline_parser_free(&args_info); /* release allocated memory */

  // Set O_NONBLOCK flag
  {
    const int flag = fcntl(STDIN_FILENO, F_GETFL);
    if (flag < 0) {
      perror("fcntl");
      return -1;
    }
    fcntl(STDIN_FILENO, F_SETFL, flag | O_NONBLOCK);
  }

  pollfd fds{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
  while (poll(&fds, 1, -1) >= 0) {
    char buffer[10 * 1024];
    const auto n = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (n <= 0) break;  // End of input
    if (args_info.stdout_flag) {
      write(STDOUT_FILENO, buffer, n);
    }
    async_rotate.InPacket(buffer, n);
  }
  // async_rotate.Flush();

  return 0;
}
