#ifndef SP_MALLOC_ALLOC_DEBUG_H
#define SP_MALLOC_ALLOC_DEBUG_H

#include "shared.h"
#include <tuple>
#include <vector>

namespace debug {

std::size_t
alloc_count_alloc(local::PoolsRAII &);

std::size_t
alloc_count_alloc(local::PoolsRAII &, std::size_t);

std::vector<std::tuple<void *, std::size_t>>
local_free_get_free(local::PoolsRAII &);

} // namespace debug

#endif
