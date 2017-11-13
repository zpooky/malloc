#include "LocalFreeList.h"
#include <cstring>

#ifdef SP_TEST
#include "LocalFreeList_debug.h"
#endif

namespace local {
using header::LocalFree;

static LocalFree *
merge_tree(LocalFree *, LocalFree *) noexcept;

static LocalFree *
stack_to_tree(LocalFree *) noexcept;

//==PUBLIC=================================================================
void *
alloc(local::PoolsRAII &pool, sp::node_size search) noexcept {
// TODO change to tree used when inserting from free stack but remain a list
// when alloc

/* Doubly linked but not circular. */
Lstart:
  LocalFree &free_list = pool.free_list;
  LocalFree *best = nullptr;
  LocalFree *current = free_list.next;
Lnext:
  if (current) {
    if (current->size == search) {
      if (current->priv)
        current->priv->next = current->next;

      if (current->next)
        current->next->priv = current->priv;

      if (free_list.next == current)
        free_list.next = current->next;

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
    goto Lnext;
  } else {
    if (best) {
      return header::reduce(best, search);
    }
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
      // TODO probably better just to insert the stack to the main tree
      // LocalFree *const t = stack_to_tree(stack);
      // free_list.next = merge_tree(free_list.next, t);
      // TODO make better
      goto Lstart;
    }
  }
  return nullptr;
} // local::alloc()

void
dealloc(local::PoolsRAII &ps, LocalFree *first, LocalFree *last) noexcept {
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
    // TODO handle
    assert(false);
  }
} // local::dealloc()

//==PRIVATE=================================================================
static void
unlink_list(LocalFree *const first, LocalFree *const second, //
            LocalFree *&priv, LocalFree *&next) noexcept {
  assert(first);
  assert(second);
  if (first->next) {
    if (first->next != second) {
      next = first->next;
      goto Lpriv;
    }
  }
  if (second->next) {
    if (second->next != first) {
      next = second->next;
    }
  }

Lpriv:
  if (first->priv) {
    if (first->priv != second) {
      priv = first->priv;
      goto Ldone;
    }
  }
  if (second->priv) {
    if (second->priv != first) {
      priv = second->priv;
    }
  }

Ldone:
  return;
}

static LocalFree *
insert_node(LocalFree *root, LocalFree *node, bool &merged) noexcept {
  // TODO update doubly linked list
  assert(root);
  assert(node);
  // assert(node->next == nullptr);
  // assert(node->priv == nullptr);

  assert(node->left == nullptr);
  assert(node->right == nullptr);

  LocalFree *parent = nullptr;
  LocalFree **ptr_to_it = &root;
  LocalFree *it = root;

Lstart:
  LocalFree *left = it->left;
  LocalFree *right = it->right;
  /*this part does not change the order of nodes it just merges the new node
  *into an existing thusly requiring no rebalancing.
  */
  {
    if (is_consecutive(it, node)) {
      // merge
      it->size = it->size + node->size;
      merged = true;

      if (right && is_consecutive(it, right)) {
        assert(right->left == nullptr);
        // merge
        it->size = it->size + right->size;
        it->right = right->right;
        // TODO unlink
      }
      return root;
    }
    if (is_consecutive(node, it)) {
      LocalFree *priv = nullptr;
      LocalFree *next = nullptr;

      // merge
      // unlink_list(node, it, priv, next);
      node = header::init_local_free(node, node->size + it->size);
      merged = true;

      if (left && is_consecutive(left, node)) {
        assert(left->right == nullptr);
        left = left->left;

        // merge
        // unlink_list(left, node, priv, next);
        node = header::init_local_free(left, left->size + node->size);
      }

      if (parent && is_consecutive(parent, node)) {
        printf("parent\n");
        assert(parent->left == nullptr);

        // merge
        // unlink_list(parent, node, priv, next);
        node = header::init_local_free(parent, parent->size + node->size);
      }

      {
        if (priv)
          priv->next = node;
        node->priv = priv;
        if (next)
          next->priv = node;
        node->next = next;
      }

      {
        node->left = left;
        node->right = right;
      }

      // update parent to the new start if it node
      *ptr_to_it = node;
      return root;
    }
  }
  /*TODO this part should be rebalanced*/
  {
    if (node > it) {
      if (right) {
        ptr_to_it = &it->right;
        parent = it;
        it = right;
        goto Lstart;
      } else {
        it->right = node;
      }
    } else {
      if (left) {
        ptr_to_it = &it->left;
        parent = it;
        it = left;
        goto Lstart;
      } else {
        it->left = node;
      }
    }
    merged = false;
  }

  return root;
}

static LocalFree *
merge_tree(LocalFree *target, LocalFree *src) noexcept {
  if (!target)
    return src;

  if (!src)
    return target;

  // TODO
  return nullptr;
}

static LocalFree *
stack_to_tree(LocalFree *const stack) noexcept {
  assert(stack);
  LocalFree *root = stack;

  LocalFree *previous = root;
  LocalFree *current = root->next;
Lstart:
  if (current) {
    previous->next = nullptr;
    current->priv = nullptr;

    bool merged = false;
    root = insert_node(root, current, merged);
    if (!merged) {
      previous->next = current;
      current->priv = previous;
    }

    previous = current;
    current = current->next;
    goto Lstart;
  }

  return root;
} // local::stack_to_tree()

} // namespace local
//==DEBUG=================================================================
#ifdef SP_TEST
namespace debug {
header::LocalFree *
local_free_stack_to_tree(header::LocalFree *stack) {
  return local::stack_to_tree(stack);
}

header::LocalFree *
local_free_tree_insert_node(header::LocalFree *tree, header::LocalFree *node,
                            bool &merged) {
  return local::insert_node(tree, node, merged);
}

header::LocalFree *
local_free_merge_tree(header::LocalFree *first, header::LocalFree *second) {
  return local::merge_tree(first, second);
}
} // namespace debug
#endif
