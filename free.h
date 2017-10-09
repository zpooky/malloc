#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {

enum class FreeCode { FREED, FREED_RECLAIM, NOT_FOUND };
FreeCode
free(local::PoolsRAII &, void *) noexcept;
FreeCode
free(local::Pools &, void *) noexcept;

std::size_t
usable_size(local::PoolsRAII &, void *) noexcept;
std::size_t
usable_size(local::Pools &, void *) noexcept;

util::maybe<void *>
realloc(local::PoolsRAII &, void *, std::size_t) noexcept;
util::maybe<void *>
realloc(local::Pools &, void *, std::size_t) noexcept;

} // namespace shared

#endif
