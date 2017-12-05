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

#define SIZE_TYPE(NAME)                                                        \
  struct NAME {                                                                \
    std::size_t data;                                                          \
    explicit constexpr NAME(std::size_t d) noexcept                            \
        : data(d) {                                                            \
    }                                                                          \
    constexpr bool                                                             \
    operator==(std::size_t o) const noexcept {                                 \
      return data == o;                                                        \
    }                                                                          \
    constexpr bool                                                             \
    operator==(const NAME &o) const noexcept {                                 \
      return this->operator==(o.data);                                         \
    }                                                                          \
    constexpr bool                                                             \
    operator!=(std::size_t o) const noexcept {                                 \
      return !operator==(o);                                                   \
    }                                                                          \
    constexpr NAME &                                                           \
    operator=(std::size_t o) noexcept {                                        \
      data = o;                                                                \
      return *this;                                                            \
    }                                                                          \
    constexpr NAME                                                             \
    operator+(std::size_t o) const noexcept {                                  \
      return NAME{data + o};                                                   \
    }                                                                          \
    constexpr NAME                                                             \
    operator+(const NAME &o) const noexcept {                                  \
      return operator+(o.data);                                                \
    }                                                                          \
    constexpr NAME                                                             \
    operator-(std::size_t o) const noexcept {                                  \
      return NAME{data - o};                                                   \
    }                                                                          \
    constexpr NAME                                                             \
    operator-(NAME o) const noexcept {                                         \
      return operator-(o.data);                                                \
    }                                                                          \
    constexpr bool                                                             \
    operator>(std::size_t o) const noexcept {                                  \
      return data > o;                                                         \
    }                                                                          \
    constexpr bool                                                             \
    operator>(const NAME &o) const noexcept {                                  \
      return operator>(o.data);                                                \
    }                                                                          \
    constexpr bool                                                             \
    operator>=(std::size_t o) const noexcept {                                 \
      return data >= o;                                                        \
    }                                                                          \
    constexpr bool                                                             \
    operator>=(const NAME &o) const noexcept {                                 \
      return operator>=(o.data);                                               \
    }                                                                          \
    constexpr bool                                                             \
    operator<(std::size_t o) const noexcept {                                  \
      return data < o;                                                         \
    }                                                                          \
    constexpr bool                                                             \
    operator<(const NAME &o) const noexcept {                                  \
      return operator<(o.data);                                                \
    }                                                                          \
    constexpr bool                                                             \
    operator<=(std::size_t o) const noexcept {                                 \
      return data <= o;                                                        \
    }                                                                          \
    constexpr bool                                                             \
    operator<=(const NAME &o) const noexcept {                                 \
      return operator<=(o.data);                                               \
    }                                                                          \
    constexpr NAME                                                             \
    operator/(std::size_t o) const noexcept {                                  \
      return NAME{data / o};                                                   \
    }                                                                          \
    constexpr NAME                                                             \
    operator%(std::size_t o) const noexcept {                                  \
      return NAME{data % o};                                                   \
    }                                                                          \
    constexpr NAME operator*(std::size_t o) const noexcept {                   \
      return NAME{data * o};                                                   \
    }                                                                          \
    constexpr explicit operator std::size_t() const noexcept {                 \
      return data;                                                             \
    }                                                                          \
  };                                                                           \
  static_assert(sizeof(NAME) == sizeof(std::size_t), "");                      \
  static_assert(alignof(NAME) == alignof(std::size_t), "")

SIZE_TYPE(relative_idx);
SIZE_TYPE(absolute_idx);

enum class Direction : uint8_t { LEFT, RIGHT };

template <typename T, std::size_t t_levels = 9>
struct static_tree {
  // TODO this is inclusive
  static constexpr std::size_t levels = t_levels;
  static constexpr std::size_t capacity = static_size<levels>::value;
  // static constexpr size = sp::static_size<level>::value;
  T storagex[capacity];
  static_tree() noexcept(noexcept(T{}))
      : storagex() {
  }

  static_tree(const static_tree<T> &) = delete;
  static_tree(const static_tree<T> &&) = delete;

  static_tree &
  operator=(const static_tree &) = delete;
  static_tree &
  operator=(const static_tree &&) = delete;

  T &operator[](sp::absolute_idx idx) noexcept {
    return storagex[std::size_t(idx)];
  }

  // To avoid std::size_t overflow
  // TODO number_of_bits(std::size_t)
  static_assert(levels < 64, "");
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
static sp::absolute_idx
base(std::size_t l, sp::relative_idx old_idx) noexcept {

  std::size_t start = level(l);
  std::size_t offset = std::size_t(old_idx) * CHILDREN;
  return sp::absolute_idx(start + offset);
}

static sp::absolute_idx
lookup(std::size_t new_lvl, sp::relative_idx old_idx, Direction dir) noexcept {

  sp::absolute_idx idx = base(new_lvl, old_idx);
  if (dir == Direction::RIGHT) {
    idx = idx + 1;
  }

  return idx;
}

template <std::size_t CHILDREN = 2>
static sp::relative_idx
lookup_relative(sp::relative_idx old_idx, Direction dir) noexcept {

  sp::relative_idx idx = old_idx * CHILDREN;
  if (dir == Direction::RIGHT) {
    idx = idx + 1;
  }

  return idx;
}

static sp::absolute_idx
translate(std::size_t l, sp::relative_idx idx) {
  std::size_t start = level(l);
  return sp::absolute_idx(std::size_t(idx) + start);
}

static sp::relative_idx
parent_relative(sp::relative_idx idx) noexcept {
  sp::relative_idx i(std::size_t(idx) / 2);

  if (lookup_relative(i, Direction::RIGHT) == idx) {
    return i;
  }

  if (lookup_relative(i, Direction::LEFT) == idx) {
    return i;
  }
  assert(false);
  return sp::relative_idx(0);
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
  sp::relative_idx idx(0);
  constexpr std::size_t capacity = static_tree<T, levels>::capacity;
Lstart:
  const sp::absolute_idx abs_idx = impl::translate(level, idx);
  if (abs_idx < capacity) {
    T &current = tree[abs_idx];
    if (bool(current)) {
      int c = impl::cmp(current, needle);
      if (c == 0) {
        return &current;
      }

      level++;
      Direction dir = c == -1 ? Direction::LEFT : Direction::RIGHT;
      idx = impl::lookup_relative(idx, dir);

      goto Lstart;
    }
  }
  return nullptr;
}

template <typename T, std::size_t levels>
bool
binary_insert(static_tree<T, levels> &tree, const T &data) noexcept {
  // printf("insert(%d)\n", data.data);
  std::size_t level = 0;
  sp::relative_idx idx(0);
  constexpr std::size_t capacity = static_tree<T, levels>::capacity;
Lstart:
  const sp::absolute_idx abs_idx = impl::translate(level, idx);
  if (abs_idx < capacity) {
    T &current = tree[abs_idx];
    if (bool(current)) {
      int c = impl::cmp(current, data);
      // printf("%s = cmp(current[%d], data[%d])\n", c == 1 ? "gt" : "lt",
      //        current.data, data.data);

      level++;
      Direction dir = c == -1 ? Direction::LEFT : Direction::RIGHT;
      const std::size_t b_idx = idx;
      idx = impl::lookup_relative(idx, dir);
      // printf("%zu = lookup(level[%zu], idx[%zu], %s)\n", //
      //        std::size_t(idx), level, b_idx,
      //        dir == Direction::LEFT ? "lt" : "gt");

      goto Lstart;
    } else {
      // printf("table[%zu] = %d\n", std::size_t(idx), data.data);
      current = data;
      return true;
    }
  }
  // printf("ERROR[%zu > %zu]\n", std::size_t(idx), capacity);
  return false;
}

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
  enum class Traversal : uint8_t { UP, DOWN };

  Direction d[levels + 1]{Direction::LEFT};
  Traversal t = Traversal::UP;

  auto set_direction = [&d](std::size_t lvl, Direction dir) {
    if (lvl <= levels) {
      d[lvl] = dir;
    }
  };

  std::size_t level = 0;
  sp::relative_idx idx(0);
Lstart:
  if (level <= levels) {
    if (t == Traversal::UP) {
      if (d[level] == Direction::LEFT) {
        level++;
        set_direction(level, Direction::LEFT);
        idx = impl::lookup_relative(idx, Direction::LEFT);
        // printf("up_left[idx[%zu], level[%zu]]\n", std::size_t(idx), level);
      } else {
        level++;
        set_direction(level, Direction::LEFT);
        idx = impl::lookup_relative(idx, Direction::RIGHT);
        // printf("up_right[idx[%zu], level[%zu]]\n", std::size_t(idx), level);
      }
      goto Lstart;
    } else /*t == DOWN*/ {
      if (d[level] == Direction::LEFT) {
        // We returned to this level after traversed left, now traverse right

        // Indicate that we have consumed right when returning to this level[0]
        d[level] = Direction::RIGHT;

        //
        f(tree[impl::translate(level, idx)]);

        //
        t = Traversal::UP;
        ++level;
        set_direction(level, Direction::LEFT);
        idx = impl::lookup_relative(idx, Direction::RIGHT);
        // printf("down_right[idx[%zu], level[%zu]]\n", std::size_t(idx),
        // level);
        goto Lstart;
      } else { //[0]
        if (level > 0) {
          idx = impl::parent_relative(idx);
          level--;
          // printf("down[idx[%zu], level[%zu]]\n", std::size_t(idx), level);
          goto Lstart;
        }
      }
    }
  } else {
    assert(t == Traversal::UP);
    // level and index now point to an out of bound node
    // printf("end[idx[%zu], level[%zu]]\n", std::size_t(idx), level);
    idx = impl::parent_relative(idx);
    level--;
    // we now point to the leaf node

    if (d[level] == Direction::LEFT) {
      // since we are in a leaf node
      // f(tree[impl::translate(level, idx)]);
    }

    // we now point to the node parent of the leaf
    // idx = impl::parent(level, idx);
    // level--;

    t = Traversal::DOWN;
    goto Lstart;
  }
}

} // namespace sp

#endif
