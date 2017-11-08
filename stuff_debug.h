#ifndef SP_MALLOC_STUFF_DEBUG_H
#define SP_MALLOC_STUFF_DEBUG_H

#include "shared.h"

namespace debug {

std::size_t
stuff_count_unclaimed_orphan_pools() noexcept;

std::size_t
stuff_count_alloc();
std::size_t stuff_count_alloc(std::size_t);

void
stuff_force_reclaim_orphan(global::State &);
}

#endif
