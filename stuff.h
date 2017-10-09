#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

#include "shared.h"

namespace global {
/*non-Thread local free*/
bool
free(void *) noexcept;

std::size_t
usable_size(void *) noexcept;

util::maybe<void *>
realloc(void *, std::size_t) noexcept;

local::PoolsRAII *
alloc_pool() noexcept;

void
release_pool(local::PoolsRAII *) noexcept;

} // namespace stuff

#endif
