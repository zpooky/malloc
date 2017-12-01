#ifndef SP_MALLOC_STATIC_TREE_H
#define SP_MALLOC_STATIC_TREE_H

namespace sp {
template <typename T, std::size_t SIZE = sizeof(T) * 1024>
struct static_tree {
  T storage[SIZE];

  static_tree() noexcept(noexcept(T{}))
      : storage() {
  }

  static_tree(const static_tree<T> &) = delete;
  static_tree(const static_tree<T> &&) = delete;

  static_tree &
  operator=(const static_tree &) = delete;
  static_tree &
  operator=(const static_tree &&) = delete;
};
// TODO dynamic_tree<T>(T* storage,std::size_t)

namespace impl {
/* Calculate level start offset
 * TODO does not support N
 * level|offset
 *     0|0
 *     1|1
 *     2|3
 *     4|7
 */
static std::size_t
level(std::size_t l) noexcept {
  if (l == 0) {
    return 0;
  }
  return std::size_t(std::size_t(1) << l) - std::size_t(1);
}

/*N is number of possible children
 */
template <std::size_t N = 2>
static std::size_t
base(std::size_t l, std::size_t oldIdx) noexcept {
  std::size_t start = level(l);
  std::size_t offset = oldIdx * N;
  return start + offset;
}

std::size_t
lookup(std::size_t level, std::size_t oldIdx, bool less_than) noexcept {
  std::size_t idx = base(level, oldIdx);
  if (!less_than) {
    ++idx;
  }
  return idx;
}

template <typename T, typename Key>
int
cmp(const T &current, const Key &needle) noexcept {
  return current.cmp(needle);
}

} // namespace impl

template <typename T, std::size_t SIZE, typename Key>
T *
search(static_tree<T, SIZE> &tree, const Key &needle) noexcept {
  std::size_t level = 0;
  std::size_t idx = 0;
Lstart:
  if (idx < SIZE) {
    T &current = tree.storage[idx];
    if (bool(current)) {
      int c = impl::cmp(current, needle);
      if (c == 0) {
        return &current;
      }

      level++;
      bool less_than = c == -1;
      idx = impl::lookup(level, idx, less_than);

      goto Lstart;
    }
  }
  return nullptr;
}

template <typename T, std::size_t SIZE>
bool
binary_insert(static_tree<T, SIZE> &, const T &data) noexcept {
  std::size_t level = 0;
  std::size_t idx = 0;
Lstart:
  if (idx < SIZE) {
    T &current = tree.storage[idx];
    if (bool(current)) {
      int c = impl::cmp(current, needle);

      level++;
      bool less_than = c == -1;
      idx = impl::lookup(level, idx, less_than);

      goto Lstart;
    } else {
      // TODO
      return true;
    }
  }
  return false;
}

// bfs_for_each()
}

#endif
