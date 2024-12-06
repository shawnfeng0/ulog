//
// Created by shawn on 24-12-6.
//

#pragma once

#include "intrusive_struct.h"
#include "power_of_two.h"

template <typename T, size_t size>
class MemoryLogger {
  static_assert(ulog::queue::is_power_of_2(size));

  struct Item {
    T data;
    uint32_t seq{0};
    std::atomic_bool is_writing{false};
  };

 public:
  T *TryReserve() {
    const auto head = head_.fetch_add(1, std::memory_order_relaxed);
    auto *item = &items_[head & (size - 1)];

    bool expected = false;
    const bool result = item->is_writing.compare_exchange_strong(expected, true, std::memory_order_acquire);

    if (!result) return nullptr;

    item->seq = head;
    return &item->data;
  }

  void Commit(T *ptr) { intrusive::owner_of(ptr, &Item::data)->is_writing.store(false, std::memory_order_release); }

  const T *Get(const size_t index) const { return &items_[index & (size - 1)].data; }

 private:
  Item items_[size];
  std::atomic_uint32_t head_{0};
};
