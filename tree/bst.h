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

//===================================================
namespace impl {
namespace bst {

template <typename T>
bool
verify(Node<T> *tree) noexcept {
  if (tree) {
    if (!verify(tree->left)) {
      return false;
    }
    if (!verify(tree->right)) {
      return false;
    }
    auto *left = tree->left;
    if (left) {
      if (!(tree->value > left->value)) {
        return false;
      }
    }

    auto *right = tree->right;
    if (right) {
      if (!(tree->value < right->value)) {
        return false;
      }
    }
  }
  return true;
} // impl::bst::verify()

template <typename T>
void
exchange(Node<T> *from, Node<T> *to) noexcept {
  auto update_parent_to_child = [](Node<T> *subject, Node<T> *nev) {
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

  update_parent_to_child(from, to);
  if (to) {
    update_parent_to_child(to, nullptr);
    to->parent = parent;

    to->left = from->left;
    if (to->left) {
      to->left->parent = to;
    }

    to->right = from->right;
    if (to->right) {
      to->right->parent = to;
    }
  } else {
    assert(!from->left);
    assert(!from->right);
  }
} // impl::bst::exchange()

template <typename T>
Node<T> *
remove(Node<T> *current) noexcept {
  if (current->left && current->right) {
    // two children
    // replace current with the smallest right child

    Node<T> *const successor = sp::impl::tree::find_min(current->right);
    remove(successor);

    exchange(current, successor);

    return successor->parent ? nullptr : successor;
  } else if (!current->left && !current->right) {
    // zero children

    exchange(current, (Node<T> *)nullptr);

    return nullptr;
  } else if (current->left) {
    // one child
    auto *const left = current->left;
    exchange(current, left);

    return left->parent ? nullptr : left;
  } else if (/*current->right*/ true) {
    // one child
    auto *const right = current->right;
    exchange(current, right);

    return right->parent ? nullptr : right;
  }
} // impl::bst::remove()
}
}
//===================================================

template <typename T>
void
dump(Tree<T> &tree, std::string prefix) noexcept {
  return sp::impl::tree::dump(tree.root, prefix);
} // bst::dump()

template <typename T>
bool
verify(Tree<T> &tree) noexcept {
  return impl::bst::verify(tree.root);
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
        assert(node->parent == nullptr);
        tree.root = nullptr;
      } else {
        assert(node->parent != nullptr);
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
