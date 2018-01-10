#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

#include "shared.h"

/*
 * non-Thread local operations
 */
namespace global {
shared::FreeCode
free(global::State &, local::Pools &tl, void *) noexcept;

sp::maybe<sp::bucket_size>
usable_size(void *) noexcept;

sp::maybe<void *>
realloc(global::State &, local::PoolsRAII &, void *, std::size_t) noexcept;

sp::maybe<void *>
realloc(global::State &, local::Pools &, void *, std::size_t) noexcept;

local::PoolsRAII *
acquire_pool(global::State &) noexcept;

void
release_pool(global::State &, local::PoolsRAII *) noexcept;

} // namespace stuff

#endif
