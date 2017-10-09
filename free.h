#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {
int
node_index_of(header::Node *, void *) noexcept;

std::size_t
node_indecies_in(header::Node *) noexcept;

bool
perform_free(header::Extent *, std::size_t idx) noexcept;

enum class FreeCode { FREED, FREED_RECLAIM, NOT_FOUND };
FreeCode
free(local::PoolsRAII &, void *) noexcept;
FreeCode
free(local::Pools &, void *) noexcept;

std::size_t
usable_size(local::PoolsRAII&, void *) noexcept;
std::size_t
usable_size(local::Pools&, void *) noexcept;

void *
sp_realloc(local::Pools &, void *, std::size_t) noexcept;

} // namespace shared

#endif
