#include "LocalFreeList.h"
#include <cstring>

#ifdef SP_TEST
#include "LocalFreeList_debug.h"
#endif

namespace list {
static header::LocalFree *
enlist(header::LocalFree *node, header::LocalFree *list) noexcept;

static void
unlist(header::LocalFree *node) noexcept;
} // namespace list

namespace local {
using header::LocalFree;

static void
insert_from_stack(local::PoolsRAII &pool, LocalFree *stack) noexcept;

static bool
merge_stack(local::PoolsRAII &pool) noexcept;

//==PUBLIC=================================================================
void *
alloc(local::PoolsRAII &pool, sp::node_size search) noexcept {
  if (search == 0) {
    return nullptr;
  }

Lstart:
  LocalFree *best = nullptr;
  LocalFree *const start = &pool.free_list;
  LocalFree *current = start;
Lnext:
  if (current->size == search) {
    list::unlist(current);

#ifdef SP_TEST
    std::memset(current, 0, std::size_t(current->size));
#endif
    return current;
  } else if (current->size > search) {
    if (!best || best->size > current->size) {
      best = current;
    }
  }

  current = current->next;
  if (current != start) {
    goto Lnext;
  } else {
    if (best) {
      return header::reduce(best, search);
    }

    if (merge_stack(pool)) {
      // TODO make better
      goto Lstart;
    }
  }
  return nullptr;
} // local::alloc()

void
dealloc(local::PoolsRAII &ps, LocalFree *first, LocalFree *last) noexcept {
  // TODO how to handle only one LocalFree* dealloc
  assert(first);
  assert(last->next == nullptr);
#ifdef DEBUG
  {
    // TODO assert first->...next == last
  }
#endif
  sp::SharedLock guard{ps.free_lock};
  if (guard) {
    // Not a doubly linked list when in the free stack
    auto base = ps.free_stack.load();
  retry:
    last->next = base;
    if (!ps.free_stack.compare_exchange_weak(base, first)) {
      goto retry;
    }
  } else {
    // TODO handle dealloc-alloc contention should probably just retry
    assert(false);
  }
} // local::dealloc()
} // namespace local

//==PRIVATE=================================================================

namespace list {
static void
relink(header::LocalFree *node, header::LocalFree *priv,
       header::LocalFree *next) noexcept {
  node->priv = priv;
  node->next = next;

  priv->next = node;
  next->priv = node;
}

static header::LocalFree *
enlist(header::LocalFree *node, header::LocalFree *list) noexcept {
  assert(node);
  assert(node->next == nullptr);
  assert(node->priv == nullptr);

  assert(list);
  assert(list->next != nullptr);
  assert(list->priv != nullptr);

  header::LocalFree *priv = list->priv;
  header::LocalFree *next = list;

  relink(node, priv, next);

  return node;
} // list::enlist()

static void
unlist(header::LocalFree *node) noexcept {
  assert(node);

  header::LocalFree *priv = node->priv;
  header::LocalFree *next = node->next;
  node->next = nullptr;
  node->priv = nullptr;

  if (priv)
    priv->next = next;

  if (next)
    next->priv = priv;
} // list::unlist()

} // namespace list

namespace local {
// static void
// unlink_list(LocalFree *const first, LocalFree *const second, //
//             LocalFree *&priv, LocalFree *&next) noexcept {
//   assert(first);
//   assert(second);
//   if (first->next) {
//     if (first->next != second) {
//       next = first->next;
//       goto Lpriv;
//     }
//   }
//   if (second->next) {
//     if (second->next != first) {
//       next = second->next;
//     }
//   }
//
// Lpriv:
//   if (first->priv) {
//     if (first->priv != second) {
//       priv = first->priv;
//       goto Ldone;
//     }
//   }
//   if (second->priv) {
//     if (second->priv != first) {
//       priv = second->priv;
//     }
//   }
//
// Ldone:
//   return;
// }

static LocalFree *
insert_node(LocalFree *root, LocalFree *node, bool &merged) noexcept {
  assert(node);

  assert(node->next == nullptr);
  assert(node->priv == nullptr);

  assert(node->left == nullptr);
  assert(node->right == nullptr);

  if (root == nullptr) {
    merged = false;
    return node;
  }

  assert(root);
  assert(root->next != nullptr);
  assert(root->priv != nullptr);

  // if /it/ is left or right from parent
  bool is_left = false;
  LocalFree **parent = nullptr;
  LocalFree *it = root;
Lstart:
  /*
   * this part does not change the order of nodes it just merges the new node
   * into an existing thusly requiring no rebalancing.
   */
  if (is_consecutive(it, node)) {
    // printf("is_consecutive(it, node)\n");
    // merge [it][node]
    it->size = it->size + node->size;
    merged = true;

    {
      LocalFree *right = it->right;
      if (right && is_consecutive(it, right)) {
        // printf("is_consecutive(it, right)\n");
        assert(right->left == nullptr);
        list::unlist(right);

        // merge [it][it->right]
        it->size = it->size + right->size;
        it->right = right->right;
      }
    }

    // if (parent && is_consecutive(it, *parent)) {
    //   // printf("is_consecutive(it, *parent)\n");
    //   assert(it->right == nullptr);
    //   assert((*parent)->left == it);
    //   // printf("parent-left\n");
    //
    //   list::unlist(it);
    //   LocalFree *right = (*parent)->right;
    //   LocalFree *left = it->left;
    //
    //   // merge [it]<-[parent]->null
    //   *parent = header::init_local_free(it, it->size + (*parent)->size);
    //   (*parent)->left = left;
    //   (*parent)->right = right;
    // }

    assert(root->next != nullptr);
    assert(root->priv != nullptr);

    return root;
  }

  if (is_consecutive(node, it)) {
    // printf("is_consecutive(node, it)\n");
    LocalFree *new_left = it->left;
    LocalFree *new_right = it->right;

    LocalFree *old_left = it->left;
    LocalFree *old_right = it->right;

    auto updateIt = [](header::LocalFree **parent, bool is_left,
                       header::LocalFree *subject) { //
      if (is_left) {
        assert(parent);
        (*parent)->left = subject;
      } else {
        assert(parent);
        (*parent)->right = subject;
      } // TODO handle parent nullptr
      return;
    };
    {
      // merge [node][it]
      LocalFree *priv = it->priv;
      LocalFree *next = it->next;
      node = header::init_local_free(node, node->size + it->size);
      list::relink(node, priv, next);

      updateIt(parent, is_left, node);
    }

    if (old_left && is_consecutive(old_left, node)) {
      assert(old_left->right == nullptr);
      // assert(left->left == it);
      list::unlist(node);
      new_left = old_left->left;

      {
        // merge [left][it]
        LocalFree *priv = old_left->priv;
        LocalFree *next = old_left->next;
        node = header::init_local_free(old_left, old_left->size + node->size);
        assert(node == old_left);
        list::relink(node, priv, next);
        updateIt(parent, is_left, node);
      }
    }

    // if (parent && is_consecutive(*parent, node)) {
    //   assert((*parent)->left == nullptr);
    //   assert((*parent)->right == node);
    //   // right = parent->right;
    //
    //   // merge [parent][it]
    //   // (*parent)->size += node->size;
    //   auto add_sz = node->size;
    //   node = *parent;
    //   node->size = node->size + add_sz;
    // }

    {
      node->left = new_left;
      assert(node->left == nullptr || node->left < node);
      node->right = new_right;
      assert(node->right == nullptr || node->right > node);
    }

    assert(root->next != nullptr);
    assert(root->priv != nullptr);

    merged = true;
    return root;
  }
  /*TODO this part should be rebalanced*/
  {
    if (!parent) {
      parent = &root;
    } else if (is_left) {
      parent = &((*parent)->left);
    } else {
      parent = &((*parent)->right);
    }

    if (node > it) {
      if (it->right) {
        is_left = false;
        it = it->right;
        goto Lstart;
      } else {
        it->right = node;
      }
    } else {
      if (it->left) {
        is_left = true;
        it = it->left;
        goto Lstart;
      } else {
        it->left = node;
      }
    }
  }

  assert(root->next != nullptr);
  assert(root->priv != nullptr);

  merged = false;
  return root;
}

static void
insert_from_stack(local::PoolsRAII &pool, LocalFree *stack) noexcept {
  LocalFree *tree = pool.free_tree.next;
Lstart:
  if (stack) {
    assert(stack != nullptr);

    LocalFree *next = stack->next;
    stack->next = nullptr;
    // if (next && header::is_consecutive(stack, next)) {
    //   stack =
    //       header::init_local_free(stack, stack->size + next->size,
    //       next->next);
    //   next = stack->next;
    //   goto Lstart;
    // }
    // if (next && header::is_consecutive(next, stack)) {
    //   stack =
    //       header::init_local_free(next, next->size + stack->size,
    //       next->next);
    //   next = stack->next;
    //   goto Lstart;
    // }

    bool merged = false;
    tree = insert_node(tree, stack, merged);
    if (!merged) {
      // printf("=\n");
      pool.free_list.next = list::enlist(stack, pool.free_list.next);
    }

    stack = next;
    goto Lstart;
  }
  pool.free_tree.next = tree;
}

static bool
merge_stack(local::PoolsRAII &pool) noexcept {
  auto stack = pool.free_stack.load();
  if (stack) {
    sp::EagerExclusiveLock guard{pool.free_lock};
    if (guard) {
    retry:
      if (stack) {
        if (!pool.free_stack.compare_exchange_weak(stack, nullptr)) {
          goto retry;
        }
      }
    }
  }
  if (stack) {
    insert_from_stack(pool, stack);
    return true;
  }

  return false;
}

} // namespace local
//==DEBUG=================================================================
#ifdef SP_TEST
namespace debug {
bool
local_free_list_merge_stack_to_tree(local::PoolsRAII &pool) {
  return local::merge_stack(pool);
}

} // namespace debug
#endif
