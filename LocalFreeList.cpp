#include "LocalFreeList.h"
#include <cstring>

#ifdef SP_TEST
#include "LocalFreeList_debug.h"
#endif

//==PRIVATE=================================================================
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
  assert(start);
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
      // XXX make better
      goto Lstart;
    }
  }
  return nullptr;
} // local::alloc()

void
dealloc(local::PoolsRAII &ps, LocalFree *first, LocalFree *last) noexcept {
  // TODO how to handle only one LocalFree* dealloc
  assert(first);
  assert(last);
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

static LocalFree *
merge_if_n(LocalFree *it) noexcept {
  assert(it);
  if (it->left) {
    if (is_consecutive(it->left, it)) {
      // assert(false);
      assert(it->left->right == nullptr);

      list::unlist(it->left);
      {
        LocalFree *const left = it->left->left;
        LocalFree *const right = it->right;
        {
          LocalFree *const priv = it->priv;
          LocalFree *const next = it->next;
          it = header::init_local_free(it->left, it->left->size + it->size);
          list::relink(it, priv, next);
        }
        it->left = left;
        it->right = right;
      }
    }
  }
  if (it->right) {
    if (is_consecutive(it, it->right)) {
      assert(it->right->left == nullptr);

      list::unlist(it->right);
      {
        LocalFree *const left = it->left;
        LocalFree *const right = it->right->right;
        {
          LocalFree *const priv = it->priv;
          LocalFree *const next = it->next;
          it = header::init_local_free(it, it->size + it->right->size);
          list::relink(it, priv, next);
        }
        it->left = left;
        it->right = right;
      }
    }
  }
  return it;
}

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

  auto update_it = [&root](LocalFree **parent, bool is_left,
                           LocalFree *subject) {
    if (!parent) {
      root = subject;
    } else if (is_left) {
      (*parent)->left = subject;
    } else {
      (*parent)->right = subject;
    }
    return;
  };

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

    // TODO recursively merge upwards until no longer adjacent
    // TODO maybe automaticly merge when travesing the tree
    if (parent && is_consecutive(it, *parent)) {
      // printf("is_consecutive(it, *parent)\n");
      assert(it->right == nullptr);
      assert((*parent)->left == it);
      // printf("parent-left\n");

      list::unlist(it);
      {
        LocalFree *right = (*parent)->right;
        LocalFree *left = it->left;

        // merge [it]<-[parent]->null
        {
          LocalFree *priv = (*parent)->priv;
          LocalFree *next = (*parent)->next;
          *parent = header::init_local_free(it, it->size + (*parent)->size);
          list::relink(*parent, priv, next);
        }
        (*parent)->left = left;
        (*parent)->right = right;
      }
    }
    // assert(!parent || !is_consecutive(*parent, it));

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

    {
      // merge [node][it]
      LocalFree *priv = it->priv;
      LocalFree *next = it->next;
      node = header::init_local_free(node, node->size + it->size);
      list::relink(node, priv, next);

      update_it(parent, is_left, node);
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
        update_it(parent, is_left, node);
      }
    }

    if (parent && is_consecutive(*parent, node)) {
      assert(new_left == nullptr);
      assert((*parent)->right == node);

      list::unlist(node);
      new_left = (*parent)->left;

      // merge [parent][it]
      LocalFree *priv = (*parent)->priv;
      LocalFree *next = (*parent)->next;
      node = header::init_local_free(*parent, (*parent)->size + node->size);
      list::relink(node, priv, next);
      assert(node == *parent);
    }

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
    it = merge_if_n(it);
    update_it(parent, is_left, it);

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
    // TODO readd
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
    // TODO
    // Maybe prepare lock is enough here because only one thread can hold the
    // prepare lock at one time and it does not hinder concurrent shared locks.
    // The only purpose of the lock here is to avoid the ABA concurrency
    // problem when swapping free stack head. Write a stress test X threads cas
    // dequeue then case insert again one by one.
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
