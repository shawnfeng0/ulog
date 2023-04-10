#pragma once

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <stdexcept>

/*
   Copyright (c) 2003 Simon Cooke, All Rights Reserved

   Licensed royalty-free for commercial and non-commercial
   use, without warranty or guarantee of suitability for any purpose.
   All that I ask is that you send me an email
   telling me that you're using my code. It'll make me
   feel warm and fuzzy inside. spectecjr@gmail.com

*/
#define OUT

class BipBuffer {
 private:
  char* const buffer_start_ptr_;
  size_t buffer_size_;
  size_t a_start_{0};
  size_t a_size_{0};
  size_t b_start_{0};
  size_t b_size_{0};
  size_t reserve_start_{0};
  size_t reserve_size_{0};
  bool is_allocated_memory_;

 public:
  explicit BipBuffer(size_t buffer_size)
      : buffer_start_ptr_((char*)::malloc(buffer_size)),
        buffer_size_(buffer_size) {
    if (buffer_start_ptr_ == nullptr)
      throw std::runtime_error{"malloc failed!"};
    is_allocated_memory_ = true;
  }

  explicit BipBuffer(char* pre_allocated_buffer, size_t buffer_size)
      : buffer_start_ptr_(pre_allocated_buffer), buffer_size_(buffer_size) {
    if (buffer_start_ptr_ == nullptr)
      throw std::runtime_error{"buffer_start_ptr_ == nullptr!"};
    is_allocated_memory_ = false;
  }

  ~BipBuffer() {
    // We don't call FreeBuffer, because we don't need to reset our variables -
    // our object is dying
    if (is_allocated_memory_) {
      free((void*)buffer_start_ptr_);
    }
  }

  ///
  /// \brief Clears the buffer of any allocations.
  ///
  /// Clears the buffer of any allocations or reservations. Note; it
  /// does not wipe the buffer memory; it merely resets all pointers,
  /// returning the buffer to a completely empty state ready for new
  /// allocations.
  ///
  void Clear() {
    a_start_ = a_size_ = b_start_ = b_size_ = reserve_start_ = reserve_size_ =
        0;
  }

  // Reserve
  //
  // Reserves space in the buffer for a memory write operation
  //
  // Parameters:
  //   int size                amount of space to reserve
  //   OUT int& reserved        size of space actually reserved
  //
  // Returns:
  //   char*                    pointer to the reserved block
  //
  // Notes:
  //   Will return NULL for the pointer if no space can be allocated.
  //   Can return any value from 1 to size in reserved.
  //   Will return NULL if a previous reservation has not been committed.

  char* Reserve(size_t size, OUT size_t& reserved) {
    // We always allocate on B if B exists; this means we have two blocks and
    // our buffer is filling.
    if (b_size_) {
      auto free_space = GetBFreeSpace();

      if (size < free_space) free_space = size;

      if (free_space == 0) return nullptr;

      reserve_size_ = free_space;
      reserved = free_space;
      reserve_start_ = b_start_ + b_size_;
      return buffer_start_ptr_ + reserve_start_;
    } else {
      // Block b does not exist, so we can check if the space AFTER a is bigger
      // than the space before A, and allocate the bigger one.

      auto free_space = GetSpaceAfterA();
      if (free_space >= a_start_) {
        if (free_space == 0) return nullptr;
        if (size < free_space) free_space = size;

        reserve_size_ = free_space;
        reserved = free_space;
        reserve_start_ = a_start_ + a_size_;
        return buffer_start_ptr_ + reserve_start_;
      } else {
        if (a_start_ == 0) return nullptr;
        if (a_start_ < size) size = a_start_;
        reserve_size_ = size;
        reserved = size;
        reserve_start_ = 0;
        return buffer_start_ptr_;
      }
    }
  }

  // Commit
  //
  // Commits space that has been written to in the buffer
  //
  // Parameters:
  //   int size                number of bytes to commit
  //
  // Notes:
  //   Committing a size > than the reserved size will cause an assert in a
  //   debug build; in a release build, the actual reserved size will be used.
  //   Committing a size < than the reserved size will commit that amount of
  //   data, and release the rest of the space. Committing a size of 0 will
  //   release the reservation.
  //
  void Commit(size_t size) {
    if (size == 0) {
      // decommit any reservation
      reserve_size_ = reserve_start_ = 0;
      return;
    }

    // If we try to commit more space than we asked for, clip to the size we
    // asked for.

    if (size > reserve_size_) {
      size = reserve_size_;
    }

    // If we have no blocks being used currently, we create one in A.

    if (a_size_ == 0 && b_size_ == 0) {
      a_start_ = reserve_start_;
      a_size_ = size;

      reserve_start_ = 0;
      reserve_size_ = 0;
      return;
    }

    if (reserve_start_ == a_size_ + a_start_) {
      a_size_ += size;
    } else {
      b_size_ += size;
    }

    reserve_start_ = 0;
    reserve_size_ = 0;
  }

  // GetContiguousBlock
  //
  // Gets a pointer to the first contiguous block in the buffer, and returns the
  // size of that block.
  //
  // Parameters:
  //   OUT int & size            returns the size of the first contiguous block
  //
  // Returns:
  //   char*                    pointer to the first contiguous block, or NULL
  //   if empty.

  char* GetContiguousBlock(OUT size_t& size) {
    if (a_size_ == 0) {
      size = 0;
      return nullptr;
    }

    size = a_size_;
    return buffer_start_ptr_ + a_start_;
  }

  // DecommitBlock
  //
  // Decommits space from the first contiguous block
  //
  // Parameters:
  //   int size                amount of memory to decommit
  //
  // Returns:
  //   nothing
  void DecommitBlock(size_t size) {
    if (size >= a_size_) {
      a_start_ = b_start_;
      a_size_ = b_size_;
      b_start_ = 0;
      b_size_ = 0;
    } else {
      a_size_ -= size;
      a_start_ += size;
    }
  }

  // GetCommittedSize
  //
  // Queries how much data (in total) has been committed in the buffer
  //
  // Parameters:
  //   none
  //
  // Returns:
  //   int                    total amount of committed data in the buffer
  size_t GetCommittedSize() const { return a_size_ + b_size_; }

  // GetReservationSize
  //
  // Queries how much space has been reserved in the buffer.
  //
  // Parameters:
  //   none
  //
  // Returns:
  //   int                    number of bytes that have been reserved
  //
  // Notes:
  //   A return value of 0 indicates that no space has been reserved
  size_t GetReservationSize() const { return reserve_size_; }

  // GetBufferSize
  //
  // Queries the maximum total size of the buffer
  //
  // Parameters:
  //   none
  //
  // Returns:
  //   int                    total size of buffer
  size_t GetBufferSize() const { return buffer_size_; }

 private:
  size_t GetSpaceAfterA() const { return buffer_size_ - a_start_ - a_size_; }

  size_t GetBFreeSpace() const { return a_start_ - b_start_ - b_size_; }
};
