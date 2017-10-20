#ifndef SP_MALLOC_GLOBAL_DEBUG_H
#define SP_MALLOC_GLOBAL_DEBUG_H

#include <tuple>
#include <vector>
#include "global.h"

namespace debug {
// these functions can not be used during load
std::vector<std::tuple<void *, std::size_t>>
global_get_free(global::State *);
void
clear_free(global::State *);
void
print_free(global::State *);
std::size_t
count_free(global::State *);
void
sort_free(global::State *);
void
coalesce_free(global::State *);

} // namespace debug

#endif
