#include "Barrier.h"
#include "Util.h"
#include "gtest/gtest.h"
#include <LocalFreeList.h>
#include <LocalFreeList_debug.h>
#include <alloc.h>
#include <alloc_debug.h>
#include <forward_list>
#include <free.h>
#include <global_debug.h>
#include <shared.h>
#include <string>

using header::LocalFree;

//==================================================================================================
class AllocTestAllocSizePFixture : public testing::TestWithParam<size_t> {
public:
  AllocTestAllocSizePFixture() {
  }

  virtual void
  SetUp() {
  }

  virtual void
  TearDown() {
  }
};

INSTANTIATE_TEST_CASE_P(DefaultInstance, AllocTestAllocSizePFixture,
                        ::testing::Values(8, 16, 32, 64, 128, 256, 512, 1024,
                                          2048, 4096, 8192));

//==================================================================================================
/*util*/
static uint8_t *
setup(global::State &g, local::PoolsRAII &p, std::size_t alloc = 0) {
  g.skip_alloc = true;
  if (alloc > 0) {
    void *ptr = aligned_alloc(64, alloc);
    p.free_list.next = header::init_local_free(ptr, sp::node_size(alloc));

    return (uint8_t *)ptr;
  }
  return nullptr;
}

static std::size_t
buckets_possible(std::size_t asz, std::size_t arena) {
  std::size_t result = 0;

  const std::size_t hdr = header::SIZE;
  const std::size_t max_buckets = header::Extent::max_buckets;
start:
  if (hdr < arena) {
    arena -= hdr;
    std::size_t buckets = std::min(arena / asz, max_buckets);
    result += buckets;
    arena -= buckets * asz;
    goto start;
  }
  printf("unusable arena:%zu\n", arena);
  return result;
}

//==================================================================================================
TEST_P(AllocTestAllocSizePFixture, test_nullptr) {
  const size_t allocSz = GetParam();

  global::State global;
  local::PoolsRAII pools;
  setup(global, pools);

  void *ptr = shared::alloc(global, pools, allocSz);
  ASSERT_TRUE(ptr == nullptr);
}
//==================================================================================================
TEST(AllocTest, test_local_alloc_local_free_list) {
  using shared::FreeCode;
  global::State global;
  local::PoolsRAII pool;
  shared::State state{global, pool, pool};
  const std::size_t arena = 1024 * 8;
  uint8_t *mptr = setup(global, pool, arena);
  Range range(mptr, arena);

  test::MemStack<test::StackHeadNoSize> stack;
  for (auto allocSz : {8, 16}) {
    const std::size_t allocs = buckets_possible(allocSz, arena);
    // TODO shared::growth_hint(pool,factor[existing-sz+(existing_sz/factor)])
    // growth per extent
    printf("alloc(%zu)*%zu\n", std::size_t(allocSz), allocs);
    for (std::size_t i = 0; i < allocs; ++i) {
      void *ptr = shared::alloc(global, pool, allocSz);
      if (!ptr) {
        printf("failed on alloc: %zu\n", i);
      }
      ASSERT_FALSE(ptr == nullptr);

      auto actual_sz = shared::usable_size(pool, ptr);
      ASSERT_TRUE(actual_sz.get_or(sp::bucket_size(0)) == allocSz);

      assert_in_range(range, ptr, allocSz);

      test::enqueue(stack, ptr, allocSz);
    }
    {
      // assert that we have consumed all available memory
      void *ptr = shared::alloc(global, pool, allocSz);
      ASSERT_TRUE(ptr == nullptr);
      // assert that we have not allocated any overlapping memory
      assert_no_overlap(stack, allocSz);
      // tmp
      auto local_free = debug::local_free_get_free(pool);
      ASSERT_EQ(std::size_t(0), local_free.size());
    }
    {
      // TODO random free
      std::size_t deallocs = 0;
    Lstart:
      auto current = test::dequeue(stack);
      if (current) {
        void *const ptr = std::get<0>(current.get());
        ASSERT_EQ(shared::free(state, ptr), FreeCode::FREED);
        ++deallocs;

        // we can not tell when the bucket is empty from the previous free and
        // subsequently reclaimed. Therefore the upcoming double free invocation
        // will return NOT_FOUND when the extent is reclaimed and DOUBLE_FREE
        // when the extent is not reclaimed.
        auto dfree = shared::free(state, ptr);
        // printf("free cnt: %zu/%zu\n", deallocs, allocs);
        // PRINT_FreeCode("double free", dfree);
        ASSERT_TRUE(dfree == FreeCode::DOUBLE_FREE ||
                    dfree == FreeCode::NOT_FOUND);
        goto Lstart;
      }
      ASSERT_EQ(deallocs, allocs);
    }
    {
      // Assert state of free list after all memory reclamation
      ASSERT_EQ(std::size_t(0), debug::global_count_free(global));
      auto local_free = debug::local_free_get_free(pool);
      ASSERT_TRUE(local_free.size() > 0);
      assert_consecutive_range(local_free, range);
      assert_no_overlap(local_free);
    }
  }
  ::free(mptr);
}
//==================================================================================================
static std::size_t
tree_nodes(LocalFree *tree) {
  if (tree == nullptr) {
    return 0;
  }

  std::size_t result = 1;
  result += tree_nodes(tree->left);
  result += tree_nodes(tree->right);

  return result;
}

static std::size_t
tree_byte_size(LocalFree *tree) {
  if (tree == nullptr) {
    return 0;
  }

  std::size_t result = std::size_t(tree->size);
  result += tree_byte_size(tree->left);
  result += tree_byte_size(tree->right);

  return result;
}

template <typename Predicate>
static LocalFree *
stack_find(LocalFree *stack, Predicate p) {
Lstart:
  if (stack) {
    if (p(stack)) {
      return stack;
    }
    stack = stack->next;
    goto Lstart;
  }
  return nullptr;
}

static std::size_t
stack_size(LocalFree *stack) {
  std::size_t result = 0;
Lstart:
  if (stack) {
    ++result;
    stack = stack->next;
    goto Lstart;
  }
  return result;
}

static bool
valid_binary_tree(LocalFree *const current) {
  if (current->left) {
    if (current->left < current) {
      if (!valid_binary_tree(current->left)) {
        return false;
      }
    } else {
      return false;
    }
  }

  if (current->right) {
    if (current->right > current) {
      if (!valid_binary_tree(current->right)) {
        return false;
      }
    } else {
      return false;
    }
  }

  return true;
}

template <std::size_t SIZE>
static void
random_offset(std::size_t (&offset)[SIZE]) {
  for (std::size_t i = 0; i < SIZE; ++i) {
    offset[i] = i;
  }

  for (std::size_t i = 0; i < SIZE; ++i) {
  Lretry:
    std::size_t dest = random() % SIZE;
    if (dest == offset[i]) {
      goto Lretry;
    }
    if (i == offset[dest]) {
      goto Lretry;
    }
    std::swap(offset[i], offset[dest]);
  }

  for (std::size_t i = 0; i < SIZE; ++i) {
    if (offset[i] == i) {
      printf("hit\n");
    }
  }
}

static std::size_t
b(uintptr_t p, Range &range) {
  return std::size_t(p - uintptr_t(range.start));
}

template <typename T>
static std::size_t
b(T *p, Range &range) {
  return b((uintptr_t)p, range);
}

struct SearchCtx {
  void *ptr;
  std::size_t length;
  SearchCtx(void *p, std::size_t l)
      : ptr(p)
      , length(l) {
  }
};

struct Search {
  SearchCtx *search;
  explicit Search(SearchCtx *s)
      : search(s) {
  }

  static bool
  in_range(LocalFree *tree, SearchCtx &ctx) {
    return in_rangex((void *)tree, std::size_t(tree->size), ctx.ptr,
                     ctx.length);
  }

  void
  print_tree(Range &base, std::string prefix, bool isTail, const char *ctx,
             LocalFree *tree) {
    if (tree) {
      char str[1024] = {0};

      const char *start = "";
      const char *end = "";
      if (search) {
        if (in_range(tree, *search)) {
          start = "\033[31m";
          end = "\033[0m";
        }
      }

      sprintf(str, "%s%s[%zu-%zu]%s", ctx, start, b(tree, base),
              b(((uintptr_t)tree) + std::size_t(tree->size), base), end);
      std::string name = str;
      printf("%s%s%s\n", prefix.c_str(), (isTail ? "└── " : "├── "),
             name.c_str());

      std::size_t cs = 0;
      if (tree->left) {
        cs++;
      }
      if (tree->right) {
        cs++;
      }

      const char *ls = (isTail ? "    " : "│   ");
      if (cs == 2) {
        print_tree(base, prefix + ls, false, "lt", tree->left);
        print_tree(base, prefix + ls, true, "gt", tree->right);
      } else {
        if (tree->left) {
          print_tree(base, prefix + ls, true, "lt", tree->left);
        } else if (tree->right) {
          print_tree(base, prefix + ls, true, "gt", tree->right);
        }
      }
    }
  }

  void
  print_tree(Range &range, local::PoolsRAII &pool) {
    LocalFree *tree = pool.free_tree.next;
    print_tree(range, "", true, "", tree);
  }
};

template <typename F>
static void
for_each(header::LocalFree *start, F f) {
  ASSERT_EQ(std::size_t(0), std::size_t(start->size));
  LocalFree *current = start->next;

Lnext:
  ASSERT_FALSE(current == nullptr);
  if (current != start) {
    f(current);
    current = current->next;
    goto Lnext;
  }
}

static void
assert_free_list_cycle(local::PoolsRAII &pool) {
  printf("assert_list_cycle\n");
  auto *const base = &pool.free_list;
  {
    auto *it = base->next;
  next_start:
    if (it) {
      if (it != base) {
        it = it->next;
        goto next_start;
      }
    } else {
      assert(false);
    }
  } //
  { //
    auto *it = base->priv;
  priv_start:
    if (it) {
      if (it != base) {
        it = it->priv;
        goto priv_start;
      }
    } else {
      assert(false);
    }
  }
  printf("==================\n");
}
static std::size_t
free_list_byte_size(local::PoolsRAII &pool) {
  printf("free_list_byte_size");
  std::size_t result = 0;
  for_each(&pool.free_list, [&](auto *current) { //
    result += std::size_t(current->size);
  });
  printf(": %zu\n==================\n", result);
  return result;
}

static void
assert_free_list_in_range(local::PoolsRAII &pool, const Range &range) {
  printf("assert_free_list_in_range\n");
  for_each(&pool.free_list, [&](auto *current) {
    assert_in_range(range, current, std::size_t(current->size));
  });
  printf("==================\n");
}

static void
assert_free_list_consecutive_range(header::LocalFree *begin,
                                   const Range &range) {
  printf("assert_free_list_consecutive_range");
  Points points;
  auto *it = begin;
  // std::size_t i = 0;
  while (it != nullptr) {
    assert_in_range(range, it, it->size);
    points.emplace_back(it, std::size_t(it->size));
    it = it->next;
    // printf("i=%zu\n", i++);
  }
  printf(": %zu\n", points.size());
  assert_consecutive_range(points, range);
  printf("==================\n");
}

static void
assert_free_list_consecutive_range(local::PoolsRAII &pool, const Range &range) {
  printf("assert_list_consecutive_range");
  Points points;
  for_each(&pool.free_list, [&](auto *current) {
    points.emplace_back(current, std::size_t(current->size));
  });
  printf(": %zu\n", points.size());
  assert_consecutive_range(points, range);
  printf("==================\n");
}

static void
assert_tree_consecutive_range(LocalFree *tree, const Range &range) {
  printf("assert_tree_consecutive_range");
  std::function<void(Points &, LocalFree *)> f;
  f = [&f](Points &points, LocalFree *tree) -> void {
    if (tree) {
      points.emplace_back(tree, std::size_t(tree->size));
      f(points, tree->left);
      f(points, tree->right);
    }
  };

  Points points;
  f(points, tree);
  printf(": %zu\n", points.size());
  assert_consecutive_range(points, range);
  printf("==================\n");
}

static void
dealloc_clear(local::PoolsRAII &pool, LocalFree *const begin,
              LocalFree *const last) {
  local::dealloc(pool, begin, last);

  ASSERT_FALSE(pool.free_stack.load() == nullptr);
  ASSERT_TRUE(debug::local_free_list_merge_stack_to_tree(pool));
  ASSERT_TRUE(pool.free_stack.load() == nullptr);
}

static void
print_tree(Range &range, local::PoolsRAII &pool) {
  Search s(nullptr);
  s.print_tree(range, pool);
}

static void
assert_after(local::PoolsRAII &pool, Range &range) {
  assert_free_list_cycle(pool);
  assert_free_list_in_range(pool, range);
  assert_free_list_consecutive_range(pool, range);
  ASSERT_EQ(range.length, free_list_byte_size(pool));

  LocalFree *tree = pool.free_tree.next;
  ASSERT_FALSE(tree == nullptr);
  assert_tree_consecutive_range(tree, range);
  ASSERT_TRUE(valid_binary_tree(tree));

  print_tree(range, pool);
  ASSERT_EQ(range.length, tree_byte_size(tree));
  // ASSERT_EQ(std::size_t(1), tree_nodes(tree));
  // ASSERT_EQ(range.length, std::size_t(tree->size));
}

static void
test_dealloc(LocalFree *const begin, LocalFree *const last, Range &range) {
  assert(last->next == nullptr);
  assert_free_list_consecutive_range(begin, range);

  global::State global;
  global.skip_alloc = true;
  local::PoolsRAII pool;

  dealloc_clear(pool, begin, last);

  assert_after(pool, range);
}

TEST(AllocTest, test_sort_inorder) {
  constexpr std::size_t arena = 1024;

  constexpr sp::node_size ns(sizeof(LocalFree));
  std::size_t alloc = std::size_t(ns) * arena;
  printf("allocsize:%zu\n", alloc);

  LocalFree *const begin = new LocalFree[arena];
  Range range((uint8_t *)begin, alloc);
  LocalFree *const end = begin + arena;

  for (LocalFree *it = begin; it != end; ++it) {
    LocalFree *next = (it + 1) != end ? it + 1 : nullptr;
    header::init_local_free(it, ns, next);
  }
  LocalFree *last = &begin[arena - 1];

  std::size_t stackSz = stack_size(begin);
  printf("stack_size():%zu\n", stackSz);
  ASSERT_EQ(arena, stackSz);

  test_dealloc(begin, last, range);
  delete[] begin;
}

TEST(AllocTest, test_sort_reverse_order) {
  constexpr std::size_t arena = 1024;

  constexpr sp::node_size ns(sizeof(LocalFree));
  std::size_t alloc = std::size_t(ns) * arena;
  printf("allocsize:%zu\n", alloc);

  LocalFree *const begin = new LocalFree[arena];
  Range range((uint8_t *)begin, alloc);
  LocalFree *last = &begin[arena - 1];

  for (std::size_t i = arena; i-- > 0;) {
    LocalFree *next = i > 0 ? &begin[i - 1] : nullptr;
    // printf("-[%zu]nxt[%p]\n", i, next);
    header::init_local_free(&begin[i], ns, next);
  }
  std::size_t stackSz = stack_size(last);
  printf("stack_size():%zu\n", stackSz);
  ASSERT_EQ(arena, stackSz);

  test_dealloc(last, begin, range);
  delete[] begin;
}

TEST(AllocTest, test_sort_random_batch) {
  srand(0);
  // constexpr std::size_t arena = 64;
  constexpr std::size_t arena = 1024;

  constexpr sp::node_size ns(sizeof(LocalFree));
  std::size_t alloc = std::size_t(ns) * arena;

  printf("allocsize:%zu\n", alloc);
  while (true) {
    LocalFree *const begin = new LocalFree[arena];
    Range range((uint8_t *)begin, alloc);
    LocalFree *const end = begin + arena;

    {
      // printf("init_local_free\n");
      for (LocalFree *it = begin; it != end; ++it) {
        header::init_local_free(it, ns, nullptr);
      }
    }

    LocalFree *it = begin;
    for (std::size_t i = 0; i < arena - 1; ++i) {
    retry:
      // TODO think of something better
      std::size_t nextIdx = random() % arena;
      LocalFree *next = begin + nextIdx;
      if (it == next) {
        goto retry;
      }
      if (next->next != nullptr) {
        goto retry;
      }
      // printf("%zu-", nextIdx);
      // printf("%zu,", nextIdx);
      it->next = next;
      it = next;
      // printf("cnt:%zu\n", i);
    }

    test_dealloc(begin, it, range);
    delete[] begin;
  }
}

TEST(AllocTest, test_sort_random_nonbatch) {
  srand(0);
  constexpr std::size_t arena = 64;
  // constexpr std::size_t arena = 1024;

  constexpr sp::node_size ns(sizeof(LocalFree));
  std::size_t alloc = std::size_t(ns) * arena;
  printf("allocsize:%zu\n", alloc);

  LocalFree *const begin = new LocalFree[arena];
  Range range((uint8_t *)begin, alloc);

  global::State global;
  global.skip_alloc = true;
  local::PoolsRAII pool;

  {
    bool handled[arena] = {false};
    for (std::size_t i = 0; i < arena; ++i) {
    retry:
      // TODO think of something better
      std::size_t nextIdx = random() % arena;
      if (handled[nextIdx]) {
        goto retry;
      }
      handled[nextIdx] = true;
      LocalFree *c = header::init_local_free(begin + nextIdx, ns, nullptr);
      printf(".%zu <---", i);
      printf("[%zu-%zu]\n", b(c, range),
             b(((uintptr_t)c) + std::size_t(c->size), range));
      SearchCtx ctx{c, std::size_t(ns)};

      dealloc_clear(pool, c, c);

      Search s(&ctx);
      s.print_tree(range, pool);
      printf("==================================\n");
    }
  }
  assert_after(pool, range);
}

// TODO shared::hint(pools,allocSz,allocs)
