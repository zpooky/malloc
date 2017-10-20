#ifndef SP_MALLOC_MALLOC_DEBUG_H
#define SP_MALLOC_MALLOC_DEBUG_H

namespace debug {
std::size_t
malloc_count_alloc();
std::size_t
malloc_count_alloc(std::size_t sz);

} // namespace debug

#endif
