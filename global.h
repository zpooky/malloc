#ifndef SP_MALLOC_GLOBAL_H
#define SP_MALLOC_GLOBAL_H

#include "shared.h"

#define SP_TEST
#ifdef SP_TEST
#include <tuple>
#include <vector>
#endif

/*
 *===========================================================
 *========GLOBAL=============================================
 *===========================================================
 */
namespace global {
/*raw block alloc*/
void *alloc(std::size_t) noexcept;
void dealloc(void *, std::size_t) noexcept;

/*none Thread local free*/
bool free(void *const) noexcept;

/*Alloc ThreadLocal headers*/
local::PoolsRAII *alloc_pool() noexcept;
void release_pool(local::PoolsRAII *) noexcept;
}

#ifdef SP_TEST
namespace test { //
std::vector<std::tuple<void *, std::size_t>> watch_free();
}
#endif

#endif
