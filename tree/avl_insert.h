#ifndef SP_MALLOC_AVL_INSERT_TREE_H
#define SP_MALLOC_AVL_INSERT_TREE_H

#include "avl.h"

namespace avl {

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &, K &&) noexcept;

namespace impl {
namespace avl {

template <typename T>
static std::size_t
insert_parent_balance(Node<T> *const child) noexcept {
  Node<T> *parent = child->parent;
  Direction d = direction(child);

  if (d == Direction::LEFT) {
    parent->balance--;
  } else {
    parent->balance++;
  }
  return parent->balance;
}

namespace ins {
// - The retracing can stop if the balance factor becomes 0 implying that the
//   height of that subtree remains unchanged.
// - If balance factor becomes -1 or +1 continue retraceing
// - If balance factor becomes -2 or +2 we need to repair.
//   After which the subtree has the same height as before
template <typename T, typename F>
static Node<T> *
retrace(Node<T> *it, F update_parent_balance) noexcept {
  Node<T> *current = nullptr;
Lstart:
  if (it) {
    current = it;

    /* Left Heavy */
    if (balance(current) == -2) {
      if (balance(current->left) == 1) {
        current->left = rotate_left(current->left);
      }

      // update parent with new child
      set(current) = rotate_right(current);

      /*
       * If there is _no_ parent then current is root.
       * If there is a parent then we have not altered the root node.
       */
      return current->parent ? nullptr : current;
    }
    /* Right Heavy */
    else if (balance(current) == 2) {
      if (balance(current->right) == -1) {
        current->right = rotate_right(current->right);
      }

      set(current) = rotate_left(current);

      return current->parent ? nullptr : current;
    }

    if (current->parent) {
      if (update_parent_balance(current) == 0) {
        return nullptr;
      }
    }

    it = current->parent;
    goto Lstart;
  }

  return current;
} // avl::impl::retrace()
} // namespace ins
} // namespace avl
} // namespace impl

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &tree, K &&ins) noexcept {
  /*Ordinary Binary Insert*/
  auto set_root = [&tree](Node<T> *new_root) {
    if (new_root) {
      tree.root = new_root;
    }
  };

  if (!tree.root) {
    /*Insert into empty tree*/
    tree.root = new (std::nothrow) Node<T>(std::forward<T>(ins));
    if (tree.root) {
      return std::make_tuple(&tree.root->value, true);
    }

    return std::make_tuple(nullptr, false);
  }

  // TODO share with bst
  Node<T> *it = tree.root;

Lstart:
  if (ins < it->value) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    auto res = it->left = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->left) {
      set_root(impl::avl::ins::retrace(it->left, [](Node<T> *child) {
        return impl::avl::insert_parent_balance(child);
      }));

      return std::make_tuple(&res->value, true);
    }
  } else if (ins > it->value) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    auto res = it->right = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->right) {
      set_root(impl::avl::ins::retrace(it->right, [](Node<T> *child) {
        return impl::avl::insert_parent_balance(child);
      }));

      return std::make_tuple(&res->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
}
}

#endif
