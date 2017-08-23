#include <cstdint>

void sp_free(void *const dealloc) noexcept;
void *sp_malloc(std::size_t sz) noexcept;
