#ifndef SP_MALLOC_ALLOC_H
#define SP_MALLOC_ALLOC_H

#include "shared.h"

namespace shared {
void *
alloc(global::State &, local::PoolsRAII &, std::size_t) noexcept;

void *
alloc(global::State &, local::Pools &, std::size_t) noexcept;

} // namespace shared

#endif
