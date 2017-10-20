#ifndef SP_MALLOC_STUFF_H
#define SP_MALLOC_STUFF_H

namespace debug {

std::size_t
stuff_count_unclaimed_orphan_pools() noexcept;

std::size_t
stuff_count_alloc();
std::size_t stuff_count_alloc(std::size_t);

void
stuff_force_reclaim_orphan();
}

#endif
