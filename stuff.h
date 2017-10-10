#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

#include "shared.h"

/*
 * non-Thread local operations
 */
namespace global {
shared::FreeCode
free(void *) noexcept;

util::maybe<std::size_t>
usable_size(void *) noexcept;

util::maybe<void *>
realloc(void *, std::size_t) noexcept;

local::PoolsRAII *
alloc_pool() noexcept;

void
release_pool(local::PoolsRAII *) noexcept;

} // namespace stuff

#endif
