#ifndef _SP_MAIN_H
#define _SP_MAIN_H

#include "bitset/Bitset.h"
#include <array>
#include <atomic>
#include <cstdint>

#define SP_MALLOC_PAGE_SIZE std::size_t(4 * 1024)
#define SP_MALLOC_CACHE_LINE_SIZE 64

#define SP_ALLOC_INITIAL_ALLOC std::size_t(SP_MALLOC_PAGE_SIZE)

namespace {

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) NodeHeader { //
  std::atomic<std::uintptr_t> next;
  std::size_t size;
};

static_assert(sizeof(NodeHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(NodeHeader) == SP_MALLOC_CACHE_LINE_SIZE, "");

struct alignas(SP_MALLOC_CACHE_LINE_SIZE) Header { //
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
  sp::con::Bitset<512, std::uint64_t> reserved;
  Header() //
      : reserved(false) {
  }
};

static_assert(sizeof(Header) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Header) == SP_MALLOC_CACHE_LINE_SIZE, "");

using byte = uint8_t;

struct Byte8Pool {
  Header h;
  byte data[SP_MALLOC_PAGE_SIZE - sizeof(h)];
  Byte8Pool() noexcept //
      : h{} {
  }
}; //

static_assert(sizeof(Byte8Pool) == SP_MALLOC_PAGE_SIZE, "");
static_assert(alignof(Byte8Pool) == SP_MALLOC_CACHE_LINE_SIZE, "");

static void init(void *const raw) {
  // TODO assert alinement of raw
  new (raw) NodeHeader;
}

} // namespace

namespace local {
struct ReadWriteLock { //
};
struct Pool { //
  std::atomic<void *> start;
  ReadWriteLock lock;
  Pool() //
      : start{nullptr},
        lock{} {
  }
  Pool(const Pool &) = delete;
  Pool(Pool &&) = delete;
  Pool &operator=(const Pool &) = delete;
  Pool &operator=(Pool &&) = delete;
};
struct Pools { //
  std::array<Pool, 7> buckets;
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
