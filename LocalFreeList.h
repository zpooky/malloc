#ifndef SP_MALLOC_LOCAL_FREE_LIST_H
#define SP_MALLOC_LOCAL_FREE_LIST_H

#include "shared.h"

namespace local {
void *
alloc(local::PoolsRAII &, sp::node_size) noexcept;

void
dealloc(local::PoolsRAII &, header::LocalFree *first,
        header::LocalFree *last) noexcept;
}

#endif
