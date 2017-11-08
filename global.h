#ifndef SP_MALLOC_GLOBAL_H
#define SP_MALLOC_GLOBAL_H

#include "shared.h"

namespace global {

header::Free *
find_free(State &, sp::node_size) noexcept;

void *
alloc(State &, sp::node_size) noexcept;

void
dealloc(State &, void *, sp::node_size) noexcept;

void
dealloc(State &, header::LocalFree *) noexcept;

} // namespace global

#endif
