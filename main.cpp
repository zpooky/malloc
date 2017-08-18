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
  new (nHdr) NodeHeader(nodeSz, bucket, extentIdxs);

  ExtentHeader *eHdr = extent_header(raw);
  new (eHdr) ExtentHeader;
  return raw;
}

static void *next_node(void *const start) noexcept {
  auto header = node_header(start);
  return header->next.load();
}

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *pointer_at(void *start, std::size_t index) noexcept {
  // Pool[Extent[Node[nodeHDR,extHDR],Node[nodeHDR]...]...]
  // The first NodeHeader in the extent contains data while intermediate
  // NodeHeader does not containt this data.
  NodeHeader *nH = node_header(start);
  size_t hdrSz(sizeof(NodeHeader) + sizeof(ExtentHeader));
  size_t buckets = nH->size;
node_start:
  const size_t dataSz(nH->rawNodeSize - hdrSz);

  size_t nodeBuckets = std::min(dataSz / nH->bucket, buckets);
  if (index < nodeBuckets) {
    // the index is in range of current node
    uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
    uintptr_t data_start = startPtr + hdrSz;

    return reinterpret_cast<void *>(data_start + (index * nH->bucket));
  } else {
    // the index is out of range go to next node or extent

    void *next = next_node(start);
    index = index - nodeBuckets;
    buckets = buckets - nodeBuckets;

    if (buckets > 0) {
      assert(next != nullptr);
      NodeHeader *iNHdr = node_header(next);
      assert(iNHdr->type == NodeHeaderType::INTERMEDIATE);

      start = next;
      // the same extent but a new node
      hdrSz = sizeof(NodeHeader);
      goto node_start;
    } else {
      // out of bound
      return nullptr;
    }
  }
}

/*
 * @start - extent start
 * desc  - Used by malloc & free
 */
static void *next_extent(void *start) noexcept {
  NodeHeader *nH = node_header(start);
  size_t hdrSz(sizeof(NodeHeader) + sizeof(ExtentHeader));
  size_t buckets = nH->size;
node_start:
  const size_t dataSz(nH->rawNodeSize - hdrSz);

  size_t nodeBuckets = std::min(dataSz / nH->bucket, buckets);
  buckets = buckets - nodeBuckets;

  void *const next = next_node(start);

  if (buckets > 0) {
    hdrSz = sizeof(NodeHeader);

    assert(next != nullptr);
    NodeHeader *iNHdr = node_header(next);
    assert(iNHdr->type == NodeHeaderType::INTERMEDIATE);

    start = next;

    goto node_start;
  } else {
    if (next) {
      NodeHeader *iNHdr = node_header(next);
      if (iNHdr->type == NodeHeaderType::INTERMEDIATE) {
        // We get here because a concurrent expand of this extent is going on by
        // the allocating thread.

        // TODO will this work?
        goto node_start;
      }
    }
    return next;
  }
}

/*
 * @start - extent start
 * desc  - Used by malloc
 */
static void *reserve(void *const start) noexcept {
  NodeHeader *nHdr = node_header(start);
  ExtentHeader *eHdr = extent_header(start);

  // TODO find first but limit it to header->size;
  const size_t index = eHdr->reserved.swap_first(true /*,header->size*/);
  if (index != eHdr->reserved.npos) {
    return pointer_at(start, index);
  }
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
      // TODO a fence here
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
