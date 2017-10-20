#ifndef SP_MALLOC_ALLOC_DEBUG_H
#define SP_MALLOC_ALLOC_DEBUG_H

#include "shared.h"

namespace debug {

std::size_t
alloc_count_alloc(local::PoolsRAII &);

std::size_t
alloc_count_alloc(local::PoolsRAII &, std::size_t);

} // namespace debug

#endif
