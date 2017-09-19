#include <cstdint>

#ifndef SP_MALLOC_H
#define SP_MALLOC_H

void sp_free(void *const dealloc) noexcept;
void *sp_malloc(std::size_t sz) noexcept;

#endif
