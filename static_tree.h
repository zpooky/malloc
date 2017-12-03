#ifndef SP_MALLOC_STATIC_TREE_H
#define SP_MALLOC_STATIC_TREE_H

namespace sp {
namespace impl {

template <std::size_t size, std::size_t level>
struct static_level { //
};

template <std::size_t level>
struct static_level<0, level> { //
  static constexpr std::size_t value = level;
};

} // namespace impl

template <std::size_t size>
struct static_level {
  static constexpr std::size_t value = 0;
};

template <std::size_t level>
struct static_size {
  static constexpr std::size_t value = 0;
};

template <typename T, std::size_t SIZE = 1024>
struct static_tree {
  static constexpr std::size_t capacity = SIZE;
  // static constexpr size = sp::static_size<level>::value;
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
binary_insert(static_tree<T, SIZE> &tree, const T &data) noexcept {
  printf("insert(%d)\n", data.data);
  std::size_t level = 0;
  std::size_t idx = 0;
Lstart:
  if (idx < SIZE) {
    T &current = tree.storage[idx];
    if (bool(current)) {
      int c = impl::cmp(current, data);
      printf("%d = cmp(current[%zu], data[%zu])\n", c, current.data, data.data);

      level++;
      bool less_than = c == -1;
      printf("%zu = lookup(%s)\n", idx, less_than ? "ls" : "gt");
      idx = impl::lookup(level, idx, less_than);

      goto Lstart;
    } else {
      printf("table[%zu] = %d\n", idx, data.data);
      current = data;
      return true;
    }
  }
  return false;
}
namespace impl {
static std::size_t
breadth(std::size_t level) noexcept {
  if (level == 0) {
    return 1;
  }
  return level * 2;
}

static std::size_t
parent(std::size_t l, std::size_t idx) noexcept {
  // brute force search parent index for idx
  std::size_t parent_level = l - 1;
  std::size_t offset = level(parent_level);
  std::size_t b = offset + breadth(parent_level);

  for (std::size_t i = offset; i < b; ++i) {
    if (lookup(parent_level, i, false) == idx) {
      return i;
    }
    if (lookup(parent_level, i, true) == idx) {
      return i;
    }
  }
  assert(false);
  return 0;
}
} // namespace impl

// bfs_for_each()
/*       7
 *       /\
 *      /  \
 *     /    \
 *     3    11
 *    /|    | \
 *   / |    |  \
 *  /  |    |   \
 * /   |    |    \
 * 2   4    9    13
 *         /|    | \
 *        / |    |  \
 *       /  |    |   \
 *      8   10   12   14
 */
template <typename T, std::size_t SIZE, typename F>
void
in_order_for_each(static_tree<T, SIZE> &tree, F f) {
  const bool left = true;
  const bool right = false;

  std::size_t level = 0;
  std::size_t idx = 0;
  bool direction = left;
  bool d = false;
  auto up = [&d] { return !d; };
  auto down = [&d] { return d; };
Lstart:
  if (idx < SIZE) {
    // lookup next upwards
    idx = impl::lookup(level, idx, direction);

    // after increment idx
    if (down()) {
      if (direction == right) {
        if (idx == 0) {
          return;
        }
      }
    }

    if (up()) {
      if (direction == right) {
        direction = left;
      }
    }

    goto Lstart;
  } else {
    idx = impl::parent(level, idx);
    f(tree.storage[idx]);

    if (up()) {
      if (direction == left) {
        direction = right;
        idx = impl::parent(level, idx);
      } else {
      }
    } else {
      assert(false);
    }
    // goto Lstart;
  }
}

} // namespace sp

#endif
