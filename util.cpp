#include "util.h"
#include <cassert>

/*
 *===========================================================
 *========UTIL===============================================
 *===========================================================
 */
namespace util {

void *
align_pointer(void *const start, std::uint32_t alignment) noexcept {
  assert(start != nullptr);
  assert(alignment >= 8);
  assert(alignment % 8 == 0);
  uintptr_t ptr = reinterpret_cast<uintptr_t>(start);
  uintptr_t diff = ptr + alignment - 1;
  ptr += diff & alignment;
  return reinterpret_cast<void *>(ptr);
} // align_pointer()

std::size_t
round_even(std::size_t v) noexcept {
  if (v <= 8) {
    return 8;
  }
  // TODO support 64 bit word
  // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  // 8,16,32,64,...
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
} // round_even()

void *
ptr_math(void *const ptr, std::int64_t add) noexcept {
  assert(ptr != nullptr);
  uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void *>(start + add);
} // ptr_math()

ptrdiff_t
ptr_diff(void *const first, void *const second) noexcept {
  assert(first != nullptr);
  assert(first != nullptr);
  uintptr_t firstPtr = reinterpret_cast<uintptr_t>(first);
  uintptr_t secondPtr = reinterpret_cast<uintptr_t>(second);
  return firstPtr - secondPtr;
} // ptr_diff()

std::size_t
trailing_zeros(std::size_t n) noexcept {
  //TODO have to check for 32 bit mode?
  return __builtin_ctzl(n);
} // util::trailing_zeroes()

std::size_t
leading_zeros(std::size_t n) noexcept {
  //TODO have to check for 32 bit mode?
  return __builtin_clzl(n);
} // util:leading_zeroes()

std::size_t
round_up(std::size_t data, std::size_t evenMultiple) noexcept {
  const std::size_t remaining = data % evenMultiple;
  const std::size_t add = remaining > 0 ? evenMultiple - remaining : 0;
  return data + add;
}

} // namespace util
