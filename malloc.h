#include <cstdint>

#ifndef SP_MALLOC_H
#define SP_MALLOC_H

// Pool[Extent[Node[NodeHeader,ExtentHeader,
// Buckets...],Node[NodeHeader]...]...]

// ExtentHeader:
// NodeHeader:

// std::size_t sp_size(void *const p) noexcept;
void sp_free(void *const dealloc) noexcept;
void *sp_malloc(std::size_t sz) noexcept;

#endif
