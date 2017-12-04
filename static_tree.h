#ifndef SP_MALLOC_STATIC_TREE_H
#define SP_MALLOC_STATIC_TREE_H

namespace sp {
namespace impl {

template <std::size_t level>
struct static_breadth {
  static constexpr std::size_t value = std::size_t(std::size_t(1) << level);
};

template <>
struct static_breadth<0> {
  static constexpr std::size_t value = 1;
};

template <std::size_t level>
struct static_size {
  static constexpr std::size_t value =
      static_breadth<level>::value + static_size<level - 1>::value;
};

template <>
struct static_size<0> {
  static constexpr std::size_t value = static_breadth<0>::value;
};

template <std::size_t size, std::size_t level = 0>
struct static_level { //
  static constexpr std::size_t value =
      static_level<size - static_breadth<level>::value, level + 1>::value;
};

template <std::size_t level>
struct static_level<0, level> { //
  static constexpr std::size_t value = level;
};

// fail case
template <>
struct static_level<0, 0> { //
};

} // namespace impl

template <std::size_t size>
struct static_level {
  static constexpr std::size_t value = impl::static_level<size>::value;
};

template <std::size_t levels>
struct static_size {
  static constexpr std::size_t value = impl::static_size<levels>::value;
};
// level -> size
static_assert(static_size<0>::value == 1, "");
static_assert(static_size<1>::value == 3, "");
static_assert(static_size<2>::value == 7, "");
static_assert(static_size<3>::value == 15, "");
static_assert(static_size<4>::value == 31, "");
static_assert(static_size<5>::value == 63, "");
static_assert(static_size<6>::value == 127, "");
static_assert(static_size<7>::value == 255, "");
static_assert(static_size<8>::value == 511, "");
static_assert(static_size<9>::value == 1023, "");
static_assert(static_size<10>::value == 2047, "");

// size -> levels
static_assert(static_level<1>::value == 1, "");
static_assert(static_level<3>::value == 2, "");
static_assert(static_level<7>::value == 3, "");
static_assert(static_level<15>::value == 4, "");
static_assert(static_level<31>::value == 5, "");
static_assert(static_level<63>::value == 6, "");
static_assert(static_level<127>::value == 7, "");
static_assert(static_level<255>::value == 8, "");
static_assert(static_level<511>::value == 9, "");
static_assert(static_level<1023>::value == 10, "");

template <typename T, std::size_t t_levels = 9>
struct static_tree {
  static constexpr std::size_t levels = t_levels;
  static constexpr std::size_t capacity = static_size<levels>::value;
  // static constexpr size = sp::static_size<level>::value;
  T storage[capacity];

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

template <std::size_t CHILDREN = 2>
static std::size_t
base(std::size_t l, std::size_t oldIdx) noexcept {
  //TODO make better
  std::size_t relOldIdx = l == 0 ? 0 : oldIdx - level(l - 1);

  std::size_t start = level(l);
  std::size_t offset = relOldIdx * CHILDREN;
  printf("base[start[%zu],offset[%zu]]\n", start, offset);
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

template <typename T, std::size_t levels, typename Key>
T *
search(static_tree<T, levels> &tree, const Key &needle) noexcept {
  std::size_t level = 0;
  std::size_t idx = 0;
Lstart:
  if (idx < static_tree<T, levels>::capacity) {
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

template <typename T, std::size_t levels>
bool
binary_insert(static_tree<T, levels> &tree, const T &data) noexcept {
  printf("insert(%d)\n", data.data);
  std::size_t level = 0;
  std::size_t idx = 0;
  constexpr std::size_t capacity = static_tree<T, levels>::capacity;
Lstart:
  if (idx < capacity) {
    T &current = tree.storage[idx];
    if (bool(current)) {
      int c = impl::cmp(current, data);
      printf("%s = cmp(current[%d], data[%d])\n", c == 1 ? "gt" : "lt",
             current.data, data.data);

      level++;
      bool less_than = c == -1;
      const std::size_t b_idx = idx;
      idx = impl::lookup(level, idx, less_than);
      printf("%zu = lookup(level[%zu], idx[%zu], %s)\n", //
             idx, level, b_idx, less_than ? "ls" : "gt");

      goto Lstart;
    } else {
      printf("table[%zu] = %d\n", idx, data.data);
      current = data;
      return true;
    }
  }
  printf("ERROR[%zu > %zu]\n", idx, capacity);
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
template <typename T, std::size_t levels, typename F>
void
in_order_for_each(static_tree<T, levels> &tree, F f) {
  const bool left = true;
  const bool right = false;

  std::size_t level = 0;
  std::size_t idx = 0;
  bool direction = left;
  bool d = false;
  auto up = [&d] { return !d; };
  auto down = [&d] { return d; };
Lstart:
  if (idx < static_tree<T, levels>::capacity) {
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
