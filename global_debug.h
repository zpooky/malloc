#ifndef SP_MALLOC_GLOBAL_DEBUG_H
#define SP_MALLOC_GLOBAL_DEBUG_H

namespace debug {
// these functions can not be used during load
std::vector<std::tuple<void *, std::size_t>> watch_free(global::State *);
void clear_free(global::State *);
void print_free(global::State *);
std::size_t count_free(global::State *);
void sort_free(global::State *);
void coalesce_free(global::State *);
}

#endif
