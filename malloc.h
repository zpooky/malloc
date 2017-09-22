#include <cstdint>

#ifndef SP_MALLOC_H
#define SP_MALLOC_H

// Pool[Extent[Node[NodeHeader,ExtentHeader,
// Buckets...],Node[NodeHeader]...]...]

// ExtentHeader:
// NodeHeader:

void *sp_malloc(std::size_t sz) noexcept;
void sp_free(void *const dealloc) noexcept;
std::size_t sp_sizeof(void *const p) noexcept;

#endif
