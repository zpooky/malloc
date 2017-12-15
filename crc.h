#ifndef SP_MALLOC_CRC_H
#define SP_MALLOC_CRC_H

#include <cstdint>

namespace sp {

std::uint32_t
crc32c(const void *, std::size_t) noexcept;

std::uint32_t
crc32(const void *, std::size_t) noexcept;

void
crc_test() noexcept;
}

#endif
