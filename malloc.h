#include <cstdint>

#ifndef SP_MALLOC_MALLOC_H
#define SP_MALLOC_MALLOC_H

void *sp_malloc(std::size_t) noexcept;
bool sp_free(void *) noexcept;
std::size_t sp_sizeof(void *) noexcept;
void *sp_realloc(void *, std::size_t) noexcept;

#endif
