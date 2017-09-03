#ifndef SP_MALLOC_SHARED_H
#define SP_MALLOC_SHARED_H

#include "ReadWriteLock.h"
#include <atomic>
#include <bitset/Bitset.h>

#if __GNUC__ < 7
#include <mutex>

namespace std {
template <typename T>
using shared_lock = unique_lock<T>;
using shared_mutex = mutex;
}
#else

// #include <mutex>
#include <shared_mutex>

#endif

#define SP_MALLOC_PAGE_SIZE std::size_t(4 * 1024)
#define SP_MALLOC_CACHE_LINE_SIZE 64

#define SP_ALLOC_INITIAL_ALLOC std::size_t(SP_MALLOC_PAGE_SIZE)

/*
 *===========================================================
 *========HEADER=============================================
 *===========================================================
 */
namespace header {

enum class NodeType { //
  HEAD,
  INTERMEDIATE
};

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Node { //
  const NodeType type;
  // next node
  std::atomic<void *> next;
  // size of what is reserved(a bucket)
  const std::size_t bucket;
  // size of the node include the header itself
  // rawNodeSize = sizeof(Node[NodeHDR,...])
  const std::size_t rawNodeSize;
  // union {
  //   struct {
  // number of buckets available
  std::size_t size;
  // } head;
  // struct {
  // } intermediate;
  // };

  Node(std::size_t p_extenSz, std::size_t p_bucket,
       std::size_t p_nodeSz) noexcept //
      : type(NodeType::HEAD), next{nullptr}, bucket(p_bucket),
        rawNodeSize(p_nodeSz), size(p_extenSz) {
  }
};

static_assert(alignof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(sizeof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Extent { //
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
  sp::Bitset<512, std::uint64_t> reserved;

  Extent() noexcept //
      : reserved(false) {
  }
};

static_assert(sizeof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Free { //
  sp::ReadWriteLock next_lock;
  std::size_t size;
  std::atomic<Free *> next;

  Free(std::size_t sz, Free *nxt) noexcept //
      : next_lock{}, size(sz), next(nxt) {
  }
};

static_assert(sizeof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");

/*init*/
Free *init_free(void *const head, std::size_t length,
                Free *const next) noexcept;
Extent *init_extent(void *const raw, std::size_t bucket,
                    std::size_t nodeSz) noexcept;

/*cast*/
Free *free(void *const start);
Node *node(void *const start) noexcept;
Extent *extent(void *const start) noexcept;
}

/*
 *===========================================================
 *========UTIL===============================================
 *===========================================================
 */
namespace util {
void *align_pointer(void *const start, std::uint32_t alignment) noexcept;
std::size_t round_even(std::size_t v) noexcept;
void *ptr_math(void *const ptr, std::int64_t add) noexcept;
ptrdiff_t ptr_diff(void *const first, void *const second) noexcept;
}

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
/*Pool*/
struct Pool { //
  std::atomic<void *> start;
  // sp::ReadWriteLock lock;
  std::shared_mutex lock;
  // std::mutex lock;

  Pool() noexcept;

  Pool(const Pool &) = delete;
  Pool(Pool &&) = delete;

  Pool &operator=(const Pool &) = delete;
  Pool &operator=(Pool &&) = delete;
};

/*Pools*/
struct PoolsRAII { //
  std::array<Pool, 64> buckets;

  PoolsRAII() noexcept;
};

class Pools { //
private:
  PoolsRAII *pools;
  //no thread holds this instance but all memeory is not reclaimed
  std::atomic<bool> reclaimed;

public:
  Pools() noexcept;
  ~Pools() noexcept;

  Pools(const Pools &) = delete;
  Pools(Pools &&) = delete;

  Pools &operator=(const Pools &) = delete;
  Pools &operator=(Pools &&) = delete;

  Pool &operator[](std::size_t) noexcept;
};

} // namespace local

#endif
