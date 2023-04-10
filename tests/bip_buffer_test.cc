//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/helper/BipBuffer.h"

#include <algorithm>
#include <cstdio>

int main() {
  char c_array[] = "1234567890";

  BipBuffer bip(32);
  {
    size_t reserve;
    auto data_ptr = bip.Reserve(1024, reserve);
    size_t ret = snprintf(data_ptr, reserve, "%s", c_array);
    bip.Commit(std::min(ret, reserve));
  }

  for (int i = 0; i < 100; ++i) {
    size_t reserve;
    auto data_ptr = bip.Reserve(1024, reserve);
    size_t ret = snprintf(data_ptr, reserve, "%s", c_array);
    bip.Commit(std::min(ret, reserve));

    size_t get_size;
    bip.GetContiguousBlock(get_size);
    bip.DecommitBlock(10);
  }
  return 0;
}
