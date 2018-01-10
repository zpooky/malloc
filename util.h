#ifndef SP_MALLOC_UTIL_H
#define SP_MALLOC_UTIL_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

/*
 *===========================================================
 *========UTIL===============================================
 *===========================================================
 */
namespace util {
void *
align_pointer(void *const start, std::uint32_t alignment) noexcept;

std::size_t
round_even(std::size_t v) noexcept;

void *
ptr_math(void *const ptr, std::int64_t add) noexcept;

ptrdiff_t
ptr_diff(void *const first, void *const second) noexcept;

std::size_t trailing_zeros(std::size_t) noexcept;
std::size_t leading_zeros(std::size_t) noexcept;

std::size_t
round_up(std::size_t data, std::size_t eventMultiple) noexcept;

} // namespace util

#endif
