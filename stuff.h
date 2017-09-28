#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

#include "shared.h"

namespace stuff {
/*none Thread local free*/
bool free(void *) noexcept;

/*Alloc ThreadLocal headers*/
local::PoolsRAII *alloc_pool() noexcept;
void release_pool(local::PoolsRAII *) noexcept;
} // namespace stuff

#endif
