#ifndef SP_MALLOC_LOCAL_FREE_LIST_DEBUG_H
#define SP_MALLOC_LOCAL_FREE_LIST_DEBUG_H

#include "shared.h"

namespace debug {
bool
local_free_list_merge_stack_to_tree(local::PoolsRAII &pool);
}

#endif
