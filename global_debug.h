#ifndef SP_MALLOC_GLOBAL_DEBUG_H
#define SP_MALLOC_GLOBAL_DEBUG_H

#include "global.h"
#include <tuple>
#include <vector>

namespace debug {
/*these functions can not be used during load*/

std::vector<std::tuple<void *, std::size_t>>
global_get_free(global::State &);

void
global_clear_free(global::State &);

void
global_print_free(global::State &);

std::size_t
global_count_free(global::State &);

void
global_sort_free(global::State &);

void
global_coalesce_free(global::State &);

} // namespace debug

#endif
