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

static void
print_tree(std::string prefix, bool isTail, const char *ctx, LocalFree *tree) {
  if (tree) {
    char str[1024] = {0};
    sprintf(str, "%s[%zu-%zu]", ctx, (std::size_t)(uintptr_t)tree,
            ((uintptr_t)tree) + std::size_t(tree->size));
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

    if (cs == 2) {
      print_tree(prefix + (isTail ? "    " : "│   "), false, "lt", tree->left);
      print_tree(prefix + (isTail ? "    " : "│   "), true, "gt", tree->right);
    }
    if (cs == 1) {
      if (tree->left) {
        print_tree(prefix + (isTail ? "    " : "│   "), true, "lt", tree->left);
      } else if (tree->right) {
        print_tree(prefix + (isTail ? "    " : "│   "), true, "gt",
                   tree->right);
      }
    }
  }
}

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
assert_list_cycle(local::PoolsRAII &pool) {
  printf("assert_list_cycle\n");
  // TODO
}

static void
assert_list_in_range(local::PoolsRAII &pool, const Range &range) {
  printf("assert_list_in_range\n");
  for_each(&pool.free_list, [&](auto *current) {
    assert_in_range(range, current, std::size_t(current->size));
  });
}

static void
assert_list_consecutive_range(header::LocalFree *begin, const Range &range) {
  printf("assert_list_consecutive_range\n");
  Points points;
  auto *it = begin;
  // std::size_t i = 0;
  while (it != nullptr) {
    assert_in_range(range, it, it->size);
    points.emplace_back(it, std::size_t(it->size));
    it = it->next;
    // printf("i=%zu\n", i++);
  }
  printf("shuffled: %zu\n", points.size());
  assert_consecutive_range(points, range);
}

static void
assert_list_consecutive_range(local::PoolsRAII &pool, const Range &range) {
  Points points;
  for_each(&pool.free_list, [&](auto *current) {
    points.emplace_back(current, std::size_t(current->size));
  });
  printf("points: %zu\n", points.size());
  printf("assert_consecutive_range\n");
  assert_consecutive_range(points, range);
}

static void
assert_tree_consecutive_range(LocalFree &tree, const Range &range) {
  printf("assert_tree_consecutive_range\n");
  // Points points;
  // assert_consecutive_range(points, range);
  // TODO
}

TEST(AllocTest, test_sort) {
  srand(0);
  constexpr std::size_t arena = 1024;

  // std::size_t offset[arena] = {0};
  // random_offset(offset);

  constexpr sp::node_size ns(sizeof(LocalFree));
  std::size_t alloc = std::size_t(ns) * arena;

  LocalFree *const begin = new LocalFree[arena];
  Range range((uint8_t *)begin, alloc);
  LocalFree *const end = begin + arena;

  printf("init_local_free\n");
  for (LocalFree *it = begin; it != end; ++it) {
    // LocalFree *next = begin + offset[offset_it++];
    header::init_local_free(it, ns, nullptr);
  }
  //----
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
  // printf("\n");
  assert_list_consecutive_range(begin, range);

  printf("stack_size\n");
  // ASSERT_EQ(arena, stack_size(begin));

  printf("dealloc\n");
  global::State global;
  global.skip_alloc = true;
  local::PoolsRAII pool;
  local::dealloc(pool, begin, it);

  printf("merge_stack\n");
  ASSERT_FALSE(pool.free_stack.load() == nullptr);
  ASSERT_TRUE(debug::local_free_list_merge_stack_to_tree(pool));
  ASSERT_TRUE(pool.free_stack.load() == nullptr);

  printf("assert\n");
  assert_list_cycle(pool);
  assert_list_in_range(pool, range);
  assert_list_consecutive_range(pool, range);
  assert_tree_consecutive_range(pool.free_tree, range);

  LocalFree *tree = pool.free_tree.next;
  ASSERT_FALSE(tree == nullptr);
  printf("valid_bin_tree\n");
  ASSERT_TRUE(valid_binary_tree(tree));

  // print_tree("", true, "", tree);
  printf("tree_nodes\n");
  ASSERT_EQ(std::size_t(1), tree_nodes(tree));

  printf("others\n");
  ASSERT_EQ(alloc, std::size_t(tree->size));
  ASSERT_EQ(begin, tree);

  ASSERT_TRUE(begin->next == nullptr);
  ASSERT_TRUE(begin->priv == nullptr);
  ASSERT_TRUE(begin->left == nullptr);
  ASSERT_TRUE(begin->right == nullptr);

  delete[] begin;
}

// TODO shared::hint(pools,allocSz,allocs)
