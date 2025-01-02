//
// Copy from
// https://github.com/shawnfeng0/intrusive_list/blob/3cc9512eee33c72a3011a0baa5b351cbd8d65ad9/include/intrusive_list/common.h
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace intrusive {

template <typename Type, typename Member>
static constexpr ptrdiff_t offset_of(const Member Type::*member) {
  return reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<Type *>(0)->*member));
}

template <typename Type, typename Member>
static constexpr Type *owner_of(const Member *ptr, const Member Type::*member) {
  return reinterpret_cast<Type *>(reinterpret_cast<intptr_t>(ptr) - offset_of(member));
}

template <typename Type, typename Member>
static constexpr Type *owner_of(const void *ptr, const Member Type::*member) {
  return reinterpret_cast<Type *>(reinterpret_cast<intptr_t>(ptr) - offset_of(member));
}

}  // namespace intrusive
