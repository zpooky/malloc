#ifndef SP_MALLOC_GLOBAL_H
#define SP_MALLOC_GLOBAL_H

#include "shared.h"
#include <cstdio>

/*
 *===========================================================
 *========GLOBAL=============================================
 *===========================================================
 */
namespace global {
std::tuple<void *, std::size_t> alloc(std::size_t) noexcept;
bool free(void *const) noexcept;

local::PoolsRAII *alloc_pool() noexcept;
void release_pool(local::PoolsRAII *) noexcept;
}

#endif
