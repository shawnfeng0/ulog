//
// Created by shawnfeng on 23-6-27.
//
#include <gtest/gtest.h>

#include "ulog/queue/power_of_two.h"

TEST(PowOfTwo, IsPassed) {
  ASSERT_EQ(ulog::queue::IsPassed(1, 1), true);

  ASSERT_EQ(ulog::queue::IsPassed(2, 1), false);
  ASSERT_EQ(ulog::queue::IsPassed(1, 2), true);

  ASSERT_EQ(ulog::queue::IsPassed(1, 0xFF000000), false);
  ASSERT_EQ(ulog::queue::IsPassed(0xFF000000, 1), true);

  ASSERT_EQ(ulog::queue::IsPassed(1, 0xFFFFFFFF), false);
  ASSERT_EQ(ulog::queue::IsPassed(0xFFFFFFFF, 1), true);
}

TEST(IsAllZero, IsAllZero) {
  {
    alignas(8) const char data[8] = {};
    ASSERT_TRUE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    alignas(8) char data[8] = {};
    data[7] = 5;
    ASSERT_FALSE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    alignas(8) char data[8] = {};
    data[0] = 1;
    ASSERT_FALSE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    alignas(8) char data[1024];
    memset(data, 0, sizeof(data));
    data[224] = 17;
    data[228] = 17;
    ASSERT_FALSE(ulog::queue::IsAllZero(data, 232));
  }
}
