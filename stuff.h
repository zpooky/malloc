#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

#include "shared.h"

/*
 * non-Thread local operations
 */
namespace global {
shared::FreeCode
free(void *) noexcept;

util::maybe<sp::bucket_size>
usable_size(void *) noexcept;

util::maybe<void *>
realloc(local::PoolsRAII &, void *, std::size_t) noexcept;

util::maybe<void *>
realloc(local::Pools &, void *, std::size_t) noexcept;

local::PoolsRAII *
alloc_pool() noexcept;

void
release_pool(local::PoolsRAII *) noexcept;

} // namespace stuff

#endif
