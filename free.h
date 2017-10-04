#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {
int
node_index_of(header::Node *node, void *ptr) noexcept;

std::size_t
node_indecies_in(header::Node *node) noexcept;

bool
perform_free(header::Extent *ext, std::size_t idx) noexcept;

bool
free(local::PoolsRAII &pools, void *const ptr) noexcept;

bool
free(local::Pools &pools, void *const ptr) noexcept;
} // namespace shared

#endif
