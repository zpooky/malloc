#ifndef SP_MALLOC_BINARY_SEARCH_TREE_H
#define SP_MALLOC_BINARY_SEARCH_TREE_H

#include "tree.h"

namespace bst {
template <typename T>
struct Node {
  using value_type = T;

  Node<T> *left;
  Node<T> *right;
  Node<T> *parent;
  T value;

  template <typename K>
  explicit Node(K &&k, Node<T> *p = nullptr)
      : left(nullptr)
      , right(nullptr)
      , parent(p)
      , value(std::forward<K>(k)) {
  }

  explicit operator std::string() const {
    std::string s;
    s.append("[v:");
    s.append(std::to_string(int(value)));
    s.append("]");
    return s;
  }

  ~Node() noexcept {
    // TODO this is recursive
    if (left) {
      delete left;
      left = nullptr;
    }
    if (right) {
      delete right;
      right = nullptr;
    }
  }

  template <typename O>
  bool
  operator<(const O &o) const noexcept {
    return value < o;
  }

  template <typename O>
  bool
  operator>(const O &o) const noexcept {
    return value > o;
  }

  template <typename O>
  bool
  operator==(const O &o) const noexcept {
    return value == o;
  }
};

template <typename T>
using Tree = sp::Tree<Node<T>>;

template <typename T>
void
dump(Tree<T> &tree, std::string prefix = "") noexcept;

template <typename T>
bool
verify(Tree<T> &tree) noexcept;

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &, K &&) noexcept;

template <typename T, typename K>
bool
remove(Tree<T> &, const K &) noexcept;

// Unbalanced Tree -> Balanced Tree
template <typename T>
bool
balance(Tree<T> &) noexcept;

//??
template <typename T>
bool
reverse(Tree<T> &) noexcept;

//===================================================
namespace impl {
namespace bst {

template <typename T>
bool
verify(Node<T> *parent, Node<T> *tree) noexcept {
  if (tree) {
    if (!verify(tree, tree->left)) {
      printf("left-");
      return false;
    }
    if (!verify(tree, tree->right)) {
      printf("right-");
      return false;
    }

    if (tree->parent != parent) {
      return false;
    }

    auto *left = tree->left;
    if (left) {
      if (!(tree->value > left->value)) {
        printf("left-");
        return false;
      }
    }

    auto *right = tree->right;
    if (right) {
      if (!(tree->value < right->value)) {
        printf("right-");
        return false;
      }
    }
  }
  return true;
} // impl::bst::verify()

/*
 * Unlinks from and replaces it with to
 */
// template <typename T>
// void
// replace(Node<T> *from, Node<T> *to) noexcept {
//   auto parent_child_link = [](Node<T> *subject, Node<T> *nev) {
//     // update parent -> child
//     Node<T> *const parent = subject->parent;
//     if (parent) {
//       if (parent->left == subject) {
//         parent->left = nev;
//       } else {
//         assert(parent->right == subject);
//         parent->right = nev;
//       }
//     }
//   };
//
//   parent_child_link(from, to);
//
//   if (to) {
//     parent_child_link(#<{(|from|)}># to, #<{(|to|)}># nullptr);
//     to->parent = from->parent;
//
//     to->left = from->left;
//     if (to->left)
//       to->left->parent = to;
//
//     to->right = from->right;
//     if (to->right)
//       to->right->parent = to;
//   }
// } // impl::bst::replace()

template <typename T>
Node<T> *
remove(Node<T> *current) noexcept {
  assert(current);
  auto parent_child_link = [](Node<T> *subject, Node<T> *nev) {
    // update parent -> child
    Node<T> *const parent = subject->parent;
    if (parent) {
      if (parent->left == subject) {
        parent->left = nev;
      } else {
        assert(parent->right == subject);
        parent->right = nev;
      }
    }
  };

  assert(sp::impl::tree::doubly_linked(current));

  if (current->left && current->right) {
    // two children

    // Replace current with the smallest right child
    Node<T> *const successor = sp::impl::tree::find_min(current->right);
    {
      remove(successor);

      parent_child_link(current, successor);
      successor->parent = current->parent;
      successor->left = current->left;
      successor->left->parent = successor;

      if (current->right) {
        successor->right = current->right;
        successor->right->parent = successor;
      }

      assert(sp::impl::tree::doubly_linked(successor));
    }

    current->parent = nullptr;
    current->left = nullptr;
    current->right = nullptr;

    return successor->parent ? nullptr : successor;
  } else if (!current->left && !current->right) {
    // zero children

    parent_child_link(current, (Node<T> *)nullptr);
    assert(sp::impl::tree::doubly_linked(current->parent)); // x
    current->parent = nullptr;

    return nullptr;
  } else if (current->left) {
    // one child

    Node<T> *const parent = current->parent;

    auto *const left = current->left;
    parent_child_link(current, left);
    left->parent = parent;
    assert(sp::impl::tree::doubly_linked(parent)); // x

    current->parent = nullptr;
    current->left = nullptr;

    return left->parent ? nullptr : left;
  }
  assert(current->right);
  // one child

  Node<T> *const parent = current->parent;

  auto *const right = current->right;
  parent_child_link(current, right);
  right->parent = parent;
  assert(sp::impl::tree::doubly_linked(parent)); // x

  current->parent = nullptr;
  current->right = nullptr;

  return right->parent ? nullptr : right;
} // impl::bst::remove()

} // namespace bst
} // namespace impl
//===================================================

template <typename T>
void
dump(Tree<T> &tree, std::string prefix) noexcept {
  return sp::impl::tree::dump(tree.root, prefix);
} // bst::dump()

template <typename T>
bool
verify(Tree<T> &tree) noexcept {
  return impl::bst::verify((Node<T> *)nullptr, tree.root);
} // bst::verify()

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &tree, K &&ins) noexcept {
  if (!tree.root) {
    // insert into empty tree
    tree.root = new (std::nothrow) Node<T>(std::forward<K>(ins));
    if (tree.root) {
      return std::make_tuple(&tree.root->value, true);
    }

    return std::make_tuple(nullptr, false);
  }

  Node<T> *it = tree.root;

Lstart:
  if (ins < it->value) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    it->left = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->left) {
      return std::make_tuple(&it->left->value, true);
    }
  } else if (ins > it->value) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    it->right = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->right) {
      return std::make_tuple(&it->right->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
} // bst::insert()

template <typename T, typename K>
bool
remove(Tree<T> &tree, const K &k) noexcept {
  Node<T> *const node = sp::impl::tree::find_node(tree.root, k);
  if (node) {
    Node<T> *const new_root = impl::bst::remove(node);

    if (new_root) {
      assert(new_root != node);
      tree.root = new_root;
    } else {
      if (tree.root == node) {
        tree.root = nullptr;
      }
    }
    delete (node);

    return true;
  }

  return false;
} // bst::remove()

// Unbalanced Tree -> Balanced Tree
template <typename T>
bool
balance(Tree<T> &) noexcept {
  // TODO
  return true;
} // bst::balance()
}

#endif
