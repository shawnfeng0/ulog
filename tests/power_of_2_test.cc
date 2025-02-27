//
// Created by shawnfeng on 23-6-27.
//
#include <gtest/gtest.h>

#include "ulog/queue/power_of_two.h"

TEST(PowOfTwo, IsInRange) {
  ASSERT_EQ(ulog::queue::IsInRange(10, 9, 20), false);
  ASSERT_EQ(ulog::queue::IsInRange(10, 10, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 11, 20), true);

  ASSERT_EQ(ulog::queue::IsInRange(10, 19, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 20, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 21, 20), false);

  ASSERT_EQ(ulog::queue::IsInRange(10, 9, 10), false);
  ASSERT_EQ(ulog::queue::IsInRange(10, 10, 10), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 11, 10), false);

  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX - 2, 1), false);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX - 1, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 0, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 1, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 2, 1), false);
}

TEST(PowOfTwo, IsPassed) {
  ASSERT_EQ(ulog::queue::IsPassed(1, 1), true);

  ASSERT_EQ(ulog::queue::IsPassed(2, 1), false);
  ASSERT_EQ(ulog::queue::IsPassed(1, 2), true);

  ASSERT_EQ(ulog::queue::IsPassed(1, 0xFF000000), false);
  ASSERT_EQ(ulog::queue::IsPassed(0xFF000000, 1), true);

  ASSERT_EQ(ulog::queue::IsPassed(1, 0xFFFFFFFF), false);
  ASSERT_EQ(ulog::queue::IsPassed(0xFFFFFFFF, 1), true);
}
