#ifndef SP_MALLOC_SHARED_H
#define SP_MALLOC_SHARED_H

#include "ReadWriteLock.h"
#include "util.h"
#include <atomic>
#include <bitset/Bitset.h>
#include <cassert>

#define SP_MALLOC_PAGE_SIZE std::size_t(4 * 1024)
#define SP_MALLOC_CACHE_LINE_SIZE 64
#define SP_ALLOC_INITIAL_ALLOC sp::node_size(SP_MALLOC_PAGE_SIZE)

#define SP_TYPED_NUMERIC
#ifdef SP_TYPED_NUMERIC

namespace sp {

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
    constexpr bool                                                             \
    operator==(const NAME &o) {                                                \
      return data == o.data;                                                   \
    }                                                                          \
    constexpr explicit operator std::size_t() const noexcept {                 \
      return data;                                                             \
    }                                                                          \
  };                                                                           \
  static_assert(sizeof(NAME) == sizeof(std::size_t), "");                      \
  static_assert(alignof(NAME) == alignof(std::size_t), "")

SIZE_TYPE(bucket_size);
SIZE_TYPE(node_size);
SIZE_TYPE(buckets);
SIZE_TYPE(index);

} // namespace sp

bool
operator<(const sp::index &, const sp::buckets &);

sp::buckets
operator/(const sp::node_size &, const sp::bucket_size &);

std::ptrdiff_t operator*(const sp::index &, const sp::bucket_size &);

sp::index
operator-(const sp::index &, const sp::buckets &);

sp::index
operator+(const sp::index &, const sp::buckets &);

bool
operator>(const sp::bucket_size &, const sp::node_size &);

#else

namespace sp {
using bucket_size = std::size_t;
using node_size = std::size_t;
using buckets = std::size_t;
using index = std::size_t;
} // namespace sp

#endif

/*
 *===========================================================
 *========HEADER=============================================
 *===========================================================
 */
namespace header {

/*Free*/
struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Free { //
  sp::ReadWriteLock next_lock;
  sp::node_size size;
  std::atomic<Free *> next;

  Free(sp::node_size, Free *) noexcept;
  explicit Free(sp::node_size) noexcept;
};

bool
is_consecutive(const Free *const head, const Free *const tail) noexcept;

void
coalesce(Free *head, Free *tail, Free *const next) noexcept;

Free *
init_free(void *const head, sp::node_size length) noexcept;

Free *
reduce(Free *, sp::node_size) noexcept;

Free *
free(void *const start) noexcept;

/*LocalFree*/
struct LocalFree {
  sp::ReadWriteLock lock;
  LocalFree *next;
  LocalFree *priv;
  sp::node_size size;

  LocalFree() noexcept;
  explicit LocalFree(sp::node_size) noexcept;
};

LocalFree *
reduce(LocalFree *, sp::node_size) noexcept;

/*Extent*/
struct Node;

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Extent { //
  static constexpr std::size_t MAX_BUCKETS = 512;

  sp::Bitset<MAX_BUCKETS, std::uint64_t> reserved;

  Extent() noexcept;
};

Extent *
extent(Node *const start) noexcept;

bool
is_empty(Extent *) noexcept;

/*Node*/
enum class NodeType : uint8_t { //
  HEAD,
  LINK,
  SPECIAL
};

struct /*alignas(SP_MALLOC_CACHE_LINE_SIZE)*/ Node { //
  static constexpr std::size_t ALIGNMENT = 64;
  // TODO padding based on arch(for pointer size)
  uint8_t pad0[16];
  // next node
  std::atomic<Node *> next;
  // size of bucket
  const sp::bucket_size bucket_size;
  // size of the node include the header itself sizeof(Node[NodeHDR,...])
  const sp::node_size node_size;
  // union {
  //   struct {
  // number of buckets available

  // only present in NodeType::HEAD
  sp::buckets buckets;
  // } head;
  // struct {
  // } intermediate;
  // };
  const NodeType type;
  // TODO const std::size_t offset; for where the first bucket start
  uint8_t pad1[15];

  Node(NodeType, sp::node_size, sp::bucket_size, sp::buckets) noexcept;
};
Node *
init_extent(void *const, sp::node_size, sp::bucket_size) noexcept;
Node *
node(void *const start) noexcept;

static constexpr std::size_t SIZE(sizeof(header::Node) +
                                  sizeof(header::Extent));

sp::node_size
node_data_size(Node *) noexcept;
std::uintptr_t
node_data_start(Node *) noexcept;

} // namespace header

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
/*Pool*/
struct Pool { //
  header::Node start;
  sp::ReadWriteLock lock;

  Pool() noexcept;

  Pool(const Pool &) = delete;
  Pool(Pool &&) = delete;

  Pool &
  operator=(const Pool &) = delete;
  Pool &
  operator=(Pool &&) = delete;

}; // struct Pool

/*Pools*/
struct PoolsRAII { //
  // TODO restructure for tighter alignment
  static constexpr std::size_t BUCKETS = (sizeof(std::size_t) * 8) - 3;
  std::array<Pool, BUCKETS> buckets;
  std::atomic<std::size_t> total_alloc;

  // global list of pools {
  PoolsRAII *priv;
  PoolsRAII *next;
  //}

  // reclaim flag {
  std::atomic<bool> reclaim;
  //}

  // local free list {
  header::LocalFree base_free;
  // }

  PoolsRAII() noexcept;

  Pool &operator[](std::size_t) noexcept;
}; // struct PoolsRAII

/*Pools*/
class Pools { //
public:
  PoolsRAII *pools;

public:
  static constexpr std::size_t BUCKETS = PoolsRAII::BUCKETS;

  Pools() noexcept;
  ~Pools() noexcept;

  Pools(const Pools &) = delete;
  Pools(Pools &&) = delete;

  Pools &
  operator=(const Pools &) = delete;
  Pools &
  operator=(Pools &&) = delete;

  Pool &operator[](std::size_t) noexcept;

  void
  init() noexcept;
}; // struct Pool

template <typename Res, typename Arg>
using PFind = util::maybe<Res> (*)(Pool &, void *, Arg &);

template <typename Res, typename Arg>
util::maybe<Res>
pools_find(PoolsRAII &pools, void *const search, PFind<Res, Arg> f,
           Arg &arg) noexcept {
  std::uintptr_t rawSearch = reinterpret_cast<std::uintptr_t>(search);
  std::size_t trail0 = util::trailing_zeros(rawSearch);

  const std::size_t offset = 1 << trail0;
  if (offset < 8) {
    // runtime fault, the minimum alignment is 8
    assert(false);
    return {};
  }
  // TODO
  // std::size_t max = offset >= sizeof(header::Node) ? Pools::BUCKETS : tz + 1;
  const std::size_t max = Pools::BUCKETS;
  for (std::size_t i(0); i < max; ++i) {
    auto result = f(pools[i], search, arg);
    if (result) {
      return result;
    }
  }

  return {};
} // local::pools_find()

template <typename Res, typename Arg>
util::maybe<Res>
pools_find(Pools &pools, void *search, PFind<Res, Arg> f, Arg &arg) noexcept {
  return pools_find<Res, Arg>(*pools.pools, search, f, arg);
}
} // namespace local

namespace shared {

enum class FreeCode { FREED, FREED_RECLAIM, NOT_FOUND, DOUBLE_FREE };

sp::bucket_size bucket_size_for(std::size_t) noexcept;

std::size_t pool_index(sp::bucket_size) noexcept;

local::Pool &
pool_for(local::PoolsRAII &, sp::bucket_size) noexcept;

} // namespace shared

#endif
