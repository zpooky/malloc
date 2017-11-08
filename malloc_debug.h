#ifndef SP_MALLOC_MALLOC_DEBUG_H
#define SP_MALLOC_MALLOC_DEBUG_H

#include "alloc_debug.h"
#include "global_debug.h"
#include "stuff_debug.h"

namespace debug {
std::size_t
malloc_count_alloc();
std::size_t
malloc_count_alloc(std::size_t sz);

void
force_reclaim_orphan_tl();

std::vector<std::tuple<void *, std::size_t>>
global_get_free();

} // namespace debug

#endif
