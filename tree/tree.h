#ifndef SP_MALLOC_TREE_H
#define SP_MALLOC_TREE_H

#include <cassert>
#include <string>

namespace sp {
// template<
//
//     class Key,
//     class Compare = std::less<Key>,
//     class Allocator = std::allocator<Key>
// >
template <typename T>
struct Tree {
  using value_type = typename T::value_type;
  using reference = value_type &;
  using const_reference = const reference;
  using pointer = value_type *;
  using const_pointer = const pointer;

  T *root;

  Tree()
      : root(nullptr) {
  }
  Tree(const Tree<T> &) = delete;
  Tree(const Tree<T> &&) = delete;

  Tree &
  operator=(const Tree<T> &) = delete;
  Tree &
  operator=(const Tree<T> &&) = delete;

  ~Tree() {
    if (root) {
      // TODO support non recursive delete
      delete root;
      root = nullptr;
    }
  }
};

// bfs_search
// inorder_search
template <typename T, typename F>
typename Tree<T>::const_pointer
search(const Tree<T> &tree, F predicate) {
  // TODO
  return nullptr;
}

template <typename T, typename S>
typename Tree<T>::const_pointer
find(const Tree<T> &tree, const S &search) {
  auto *root = tree.root;
Lstart:
  if (root) {
    if (*root < search) {
      root = root->right;
      goto Lstart;
    } else if (*root > search) {
      root = root->left;
      goto Lstart;
    } else {
      assert(*root == search);
      return &root->value;
    }
  }
  return nullptr;
}

namespace impl {
namespace tree {

template <typename T>
void
dump(T *tree, std::string prefix = "", bool isTail = true,
     const char *ctx = "") noexcept {
  if (tree) {
    char name[256] = {0};
    auto val = std::string(*tree);
    sprintf(name, "%s%s", ctx, val.c_str());

    printf("%s%s%s\n", prefix.c_str(), (isTail ? "└── " : "├── "), name);

    const char *ls = (isTail ? "    " : "│   ");
    if (tree->right && tree->left) {
      dump(tree->right, prefix + ls, false, "gt:");
      dump(tree->left, prefix + ls, true, "lt:");
    } else {
      if (tree->left) {
        dump(tree->left, prefix + ls, true, "lt:");
      } else if (tree->right) {
        dump(tree->right, prefix + ls, true, "gt:");
      }
    }
  }
}

} // namespace tree
} // namesapce impl
}
#endif