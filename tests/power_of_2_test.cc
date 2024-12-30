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

TEST(IsAllZero, IsAllZero) {
  {
    const char data[5] = {0, 0, 0, 0, 0};
    ASSERT_TRUE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    const char data[5] = {0, 0, 0, 0, 5};
    ASSERT_FALSE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    const char data[5] = {1, 0, 0, 0, 0};
    ASSERT_FALSE(ulog::queue::IsAllZero(data, sizeof(data)));
  }

  {
    char data[1024];
    memset(data, 0, sizeof(data));
    data[224] = 17;
    data[228] = 17;
    ASSERT_FALSE(ulog::queue::IsAllZero(data, 232));
  }
}

TEST(power_of_2, atom_sub_lock) {
  ulog::queue::AtomSubLock atom_sub_lock;
  atom_sub_lock.add(10, std::memory_order_release);
  ASSERT_EQ(atom_sub_lock.size(), 10);
  atom_sub_lock.lock_for_sub();
  ASSERT_EQ(atom_sub_lock.size(), 10);
  atom_sub_lock.sub(4, std::memory_order_relaxed);
  ASSERT_EQ(atom_sub_lock.size(), 6);
  ASSERT_TRUE(atom_sub_lock.unlock_if_not_increased(6, std::memory_order_relaxed));
  atom_sub_lock.add(1);
  ASSERT_EQ(atom_sub_lock.size(), 7);
}