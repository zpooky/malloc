#ifndef SP_MALLOC_TREE_H
#define SP_MALLOC_TREE_H

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
      delete root;
      root = nullptr;
    }
  }
};

template <typename T, typename F>
typename Tree<T>::const_pointer
search(const Tree<T> &tree, F predicate) {
  // TODO
  return nullptr;
}

template <typename T>
typename Tree<T>::const_pointer
find(const Tree<T> &tree, typename Tree<T>::const_reference) {
  // TODO
  return nullptr;
}

namespace impl {
namespace tree {} // namespace tree
} // namesapce impl
}
#endif
