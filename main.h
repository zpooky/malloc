#ifndef _SP_MAIN_H
#define _SP_MAIN_H

// #include "ReadWriteLock.h"
#include "bitset/Bitset.h"

#include <array>
#include <atomic>
#include <cstdint>

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

namespace {

enum class NodeHeaderType { //
  HEAD,
  INTERMEDIATE
};

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) NodeHeader { //
  const NodeHeaderType type;
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

  NodeHeader(std::size_t p_extenSz, std::size_t p_bucket,
             std::size_t p_nodeSz) noexcept //
      : type(NodeHeaderType::HEAD),
        next{nullptr},
        bucket(p_bucket),
        rawNodeSize(p_nodeSz),
        size(p_extenSz) {
  }
};

static_assert(alignof(NodeHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(sizeof(NodeHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) ExtentHeader { //
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

  ExtentHeader() noexcept //
      : reserved(false) {
  }
};

static_assert(sizeof(ExtentHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(ExtentHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");

} // namespace

namespace local {
/*Pool*/
struct Pool { //
  std::atomic<void *> start;
  // sp::ReadWriteLock lock;
  std::shared_mutex lock;
  // std::mutex lock;

  Pool() //
      : start{nullptr},
        lock{} {
  }

  Pool(const Pool &) = delete;
  Pool(Pool &&) = delete;

  Pool &operator=(const Pool &) = delete;
  Pool &operator=(Pool &&) = delete;
};

/*Pools*/
struct Pools { //
  std::array<Pool, 64> buckets;

  Pools() //
      : buckets{} {
  }

  Pools(const Pools &) = delete;
  Pools(Pools &&) = delete;

  Pools &operator=(const Pools &) = delete;
  Pools &operator=(Pools &&) = delete;
};

} // namespace local

#endif
