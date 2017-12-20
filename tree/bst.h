#ifndef SP_MALLOC_BINARY_SEARCH_TREE_H
#define SP_MALLOC_BINARY_SEARCH_TREE_H

#include "tree.h"

namespace bst {
template <typename T>
struct Node {
  Node *left;
  Node *right;
  T value;
  template <typename K>
  explicit Node(K &&k)
      : left(nullptr)
      , right(nullptr)
      , value(std::forward<K>(k)) {
  }
};

template <typename T>
using Tree = sp::Tree<Node<T>>;

bool
balance(Tree &) const noexcept {
  // TODO
  return true;
}
}

#endif
