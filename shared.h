#ifndef SP_MALLOC_SHARED_H
#define SP_MALLOC_SHARED_H

#include "ReadWriteLock.h"
#include "util.h"
#include <atomic>
#include <bitset/Bitset.h>
#include <cassert>

#define SP_MALLOC_PAGE_SIZE std::size_t(4 * 1024)
#define SP_MALLOC_CACHE_LINE_SIZE 64
#define SP_ALLOC_INITIAL_ALLOC std::size_t(SP_MALLOC_PAGE_SIZE)

/*
 *===========================================================
 *========HEADER=============================================
 *===========================================================
 */
namespace header {

/*Free*/
struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Free { //
  sp::ReadWriteLock next_lock;
  std::size_t size;
  std::atomic<Free *> next;

  Free(std::size_t sz, Free *nxt) noexcept;
};
bool is_consecutive(const Free *const head, const Free *const tail) noexcept;
void coalesce(Free *head, Free *tail, Free *const next) noexcept;
Free *init_free(void *const head, std::size_t length) noexcept;
Free *reduce(Free *free, std::size_t length) noexcept;
Free *free(void *const start) noexcept;

/*Extent*/
struct Node;

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Extent { //
  static constexpr std::size_t MAX_BUCKETS = 512;
  // const uint8_t block_size;
  // fill out bitset so it occopies the whole cache_line
  // bitset
  // # block_size(uint32) + 4byte bitset_block(uint32)
  // cache_line - sizeof(block_size) = 60
  // 60/bitset_block = 15
  // 15 * bits_in_bitset_bloc(32) = 480
  // page_size(4k) - header_size(64B) = 4032
  // 4032 / 480 = 8.4Byte
  //
  //# block_size(uint8) + 1byte bitset_block(uint8)
  // cache_line - sizeof(block_size) = 63
  // 63/bitset_block = 63
  // 63 * bits_in_bitset_bloc(8) = 504
  // page_size(4k) - header_size(64B) = 4032
  // 4032 / 504 = 8Byte
  sp::Bitset<MAX_BUCKETS, std::uint64_t> reserved;

  Extent() noexcept;
};
Extent *extent(Node *const start) noexcept;

/*Node*/
enum class NodeType { //
  HEAD,
  INTERMEDIATE
};

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Node { //
  // TODO padding of bytes to avoid +1 errors sizeof Node should still be 64
  const NodeType type;
  // next node
  std::atomic<Node *> next;
  // size of bucket
  const std::size_t bucket_size;
  // size of the node include the header itself sizeof(Node[NodeHDR,...])
  const std::size_t node_size;
  // union {
  //   struct {
  // number of buckets available
  std::size_t buckets;
  // } head;
  // struct {
  // } intermediate;
  // };
  // TODO const std::size_t offset; for where the first bucket start
  // TODO padding of bytes to avoid -1 errors sizeof Node should still be 64

  Node(std::size_t node_size, std::size_t bucket_size,
       std::size_t buckets) noexcept;
};
Node *init_node(void *const raw, std::size_t size,
                std::size_t bucketSz) noexcept;
Node *node(void *const start) noexcept;

static constexpr std::size_t SIZE(sizeof(header::Node) +
                                  sizeof(header::Extent));

std::size_t node_data_size(Node *) noexcept;
uintptr_t node_data_start(Node *) noexcept;

} // namespace header

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
/*Pool*/
struct Pool { //
  std::atomic<header::Node *> start;
  sp::ReadWriteLock lock;
  // std::shared_mutex lock;
  // std::mutex lock;

  Pool() noexcept;

  Pool(const Pool &) = delete;
  Pool(Pool &&) = delete;

  Pool &operator=(const Pool &) = delete;
  Pool &operator=(Pool &&) = delete;
}; // struct Pool

/*Pools*/
struct PoolsRAII { //
  static constexpr std::size_t BUCKETS = 60;
  std::array<Pool, BUCKETS> buckets;
  std::atomic<std::size_t> total_alloc;

  //{
  std::atomic<PoolsRAII *> global;
  std::atomic<bool> reclaim;
  //}

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

  Pools &operator=(const Pools &) = delete;
  Pools &operator=(Pools &&) = delete;

  Pool &operator[](std::size_t) noexcept;
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
    // should be a runtime fault, the minimum alignment is 8
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

#endif
