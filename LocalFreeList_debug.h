#ifndef SP_MALLOC_LOCAL_FREE_LIST_DEBUG_H
#define SP_MALLOC_LOCAL_FREE_LIST_DEBUG_H

#include "shared.h"

namespace debug {
header::LocalFree *
local_free_stack_to_tree(header::LocalFree *);

header::LocalFree *
local_free_tree_insert_node(header::LocalFree *, header::LocalFree *, bool &);

header::LocalFree *
local_free_merge_tree(header::LocalFree *, header::LocalFree *);
}

#endif
