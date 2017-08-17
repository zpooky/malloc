#include <cassert>
#include <cstdio>
#include <iostream>
#include <mutex>

#include <algorithm>
#include <utility>

#include <unistd.h> //sbrk

#include "main.h"

/*
 *===========================================================
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {

namespace internal {
struct State {
  // atomic_list<Pair<void*,length>> free_space;
  // atomic_list<Pair<void*,length>> pending_reclaim;
  // std::atomic<void *> position;

  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;

  State() noexcept //
      : brk_lock{},
        brk_position{nullptr},
        brk_alloc{0} {
  }
};

static State state;

static std::pair<void *, size_t> find_free(size_t, size_t) noexcept {
  return std::make_pair(nullptr, 0);
}

static std::pair<void *, size_t> new_free(size_t atLeast, size_t) noexcept {
  void *res = nullptr;
  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // if (state.brk_position == nullptr) {
    //   state.brk_position = ::sbrk(0);
    // }
    // TODO some algorithm to determine optimal alloc size
    std::size_t allocSz = std::max(state.brk_alloc, SP_ALLOC_INITIAL_ALLOC);
    allocSz = std::max(atLeast, allocSz);
    // TODO check wrap around

    // void *newPos = state.brk_position + allocSz;
    res = ::sbrk(allocSz);
    if (res != (void *)-1) {
      state.brk_alloc = state.brk_alloc + allocSz;
      return std::make_pair(res, allocSz);
    }
  }

  return std::make_pair(nullptr, 0);
}

static void return_free(const std::pair<void *, size_t> &free) noexcept {
  if (std::get<1>(free) != 0) {
  }
}

} // namespace internal

static std::tuple<void *, std::size_t> alloc(std::size_t sz,
                                             std::size_t align) noexcept {
  auto free = internal::find_free(sz, align);
  if (std::get<0>(free) == nullptr) {
    free = internal::new_free(sz, align);
    if (std::get<0>(free) == nullptr) {
      // return nullptr;
    }
  }
  // TODO
  void *result = 0;
  internal::return_free(free);
  return std::make_tuple(result, std::size_t(0));
}

} // namespace global

namespace {

void *allign(void *const start, std::uint32_t alignment) noexcept {
  assert(alignment >= 8);
  assert(alignment % 8 == 0);
  uintptr_t ptr = reinterpret_cast<uintptr_t>(start);
  ptr = ptr + alignment - 1;
  ptr = ptr & alignment;
  return reinterpret_cast<void *>(ptr);
}

std::size_t round_even(std::size_t v) noexcept {
  // TODO support 64 bit word
  // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  // 8,16,32,64,...
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

std::size_t index_of(std::size_t size) noexcept {
  return (size / 8) - 1;
}

// enum class BlockSize : uint8_t {
//   BYTE8,
//   BYTE16,
//   BYTE32,
//   BYTE64,
//   BYTE128,
//   BYTE256 = 256
// };

} // namespace

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
static thread_local Pools pools;

static Pool &pool_for(Pools &pools, std::size_t sz) noexcept {
  std::size_t index = index_of(sz);
  return pools.buckets[index];
}

static std::tuple<void *, std::size_t> alloc(std::size_t sz) noexcept {
  return global::alloc(sz, 8);
}

static NodeHeader *node_header(void *const start) noexcept {
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(NodeHeader) == 0);
  return reinterpret_cast<NodeHeader *>(start);
}

static ExtentHeader *extent_header(void *const start) noexcept {
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(NodeHeader);
  assert(headerPtr % alignof(ExtentHeader) == 0);

  return reinterpret_cast<ExtentHeader *>(headerPtr);
}

static void * //
    init(void *const raw, std::size_t bucket, std::size_t nodeSz) noexcept {
  // TODO calc based on bucket and nodeSz
  std::size_t extentIdxs = 0;

  NodeHeader *nHdr = node_header(raw);
  new (nHdr) NodeHeader(nodeSz, extentIdxs);

  ExtentHeader *eHdr = extent_header(raw);
  new (eHdr) ExtentHeader;
  return raw;
}

static void *pointer_at(void *const start, std::size_t index) noexcept {
  // TODO
  return nullptr;
}

static void *reserve(void *const start) noexcept {
  NodeHeader *nHdr = node_header(start);
  ExtentHeader *eHdr = extent_header(start);

  // TODO find first but limit it to header->extSize;
  //, nodeHeader->extentSize
  const size_t index = eHdr->reserved.swap_first(true);
  if (index != eHdr->reserved.npos) {
    return pointer_at(start, index);
  }
  return nullptr;
}

static void *next_node(void *const start) noexcept {
  auto header = node_header(start);
  return header->next.load();
}

static void *next_extent(void *const start) noexcept {
  // TODO
  return nullptr;
}

} // namespace local

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */

void *sp_malloc(std::size_t sz) noexcept {
  const std::size_t bucket = round_even(sz);
  local::Pool &pool = pool_for(local::pools, bucket);

  std::shared_lock<std::shared_mutex> lock(pool.lock);
  void *start = pool.start.load();
  if (start) {
  start:
    void *result = local::reserve(start);
    if (result) {
      return result;
    } else {
      void *const next = local::next_extent(start);
      if (next) {
        start = next;
        goto start;
      } else {
        // TODO upgrade to exclusive lock, either extend extent or allocate new
        // extent
        // TODO does not need to do the local::alloc under the bucket lock since
        // we will only have 1 TL allocater at a time
      }
    }
  } else {
    // only TL allowed to malloc meaning no alloc contention
    auto mem = local::alloc(sz);
    if (std::get<0>(mem)) {
      void *initialized =
          local::init(std::get<0>(mem), bucket, std::get<1>(mem));
      void *const result = local::reserve(initialized);
      if (pool.start.compare_exchange_strong(start, initialized)) {
        return result;
      } else {
        // somehow we failed to cas
        assert(false);
      }
    } else {
      // out of memory
      return nullptr;
    }
  }
}

void sp_free(void *) {
}

int main() {
  // init();
  printf("size NodeHeader:%lu\n", sizeof(NodeHeader));
}
