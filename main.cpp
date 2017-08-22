#include <cassert>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <tuple>

#include <algorithm>
#include <utility>

#include <unistd.h> //sbrk

#include "main.h"

// http://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect/

// # global
// TODO how to coaless global::free_list pages?
// # local
// TODO add local::free_list
// TODO coaless local::free_list when allocating thread looks for matching pages

namespace {

void *align_pointer(void *const start, std::uint32_t alignment) noexcept {
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

void *ptr_math(void *const ptr, std::int64_t add) noexcept {
  uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void *>(start + add);
}

ptrdiff_t ptr_diff(void *const first, void *const second) noexcept {
  uintptr_t firstPtr = reinterpret_cast<uintptr_t>(first);
  uintptr_t secondPtr = reinterpret_cast<uintptr_t>(second);
  return firstPtr - secondPtr;
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
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {
namespace internal {
struct State {
  // brk
  // {{{
  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;
  // }}}

  // free
  // {{{
  std::atomic<void *> free_list;
  sp::ReadWriteLock free_lock;
  // }}}

  State() noexcept                                       //
      : brk_lock{}, brk_position{nullptr}, brk_alloc{0}, //
        free_list{nullptr}, free_lock{} {
    std::atomic_thread_fence(std::memory_order_release);
  }
};

static State state;

static std::tuple<void *, size_t> find_free(size_t, size_t) noexcept {
  sp::SharedLock guard(state.free_lock);
retry:
  return std::make_tuple(nullptr, 0);
}

static std::tuple<void *, size_t> alloc_free(size_t atLeast, size_t) noexcept {
  void *res = nullptr;
  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // if (state.brk_position == nullptr) {
    //   state.brk_position = ::sbrk(0);
    // }
    // TODO some algorithm to determine optimal alloc size
    std::size_t allocSz = std::max(state.brk_alloc, SP_ALLOC_INITIAL_ALLOC);
    allocSz = std::max(atLeast, allocSz);
    // TODO check size wrap around

    // void *newPos = state.brk_position + allocSz;
    res = ::sbrk(allocSz);
    if (res != (void *)-1) {
      state.brk_alloc = state.brk_alloc + allocSz;
      return std::make_pair(res, allocSz);
    }
  }

  return std::make_tuple(nullptr, 0);
}

void * //
    init_free(void *const head, std::size_t length, void *const next) noexcept {
  assert(reinterpret_cast<uintptr_t>(head) % alignof(FreeHeader) == 0);
  assert(length >= sizeof(FreeHeader));
  return new (head) FreeHeader(length, next);
}

static void return_free(void *const ptr, size_t length) noexcept {
  if (length > 0) {
    sp::SharedLock guard(state.free_lock);
    void *next = state.free_list.load(std::memory_order_acquire);
  retry:
    void *const free = init_free(ptr, length, next);
    if (!state.free_list.compare_exchange_strong(next, free)) {
      goto retry;
    }
  }
}

static void return_free(const std::tuple<void *, size_t> &free) noexcept {
  return return_free(std::get<0>(free), std::get<1>(free));
}

} // namespace internal

static auto alloc(std::size_t p_length, std::size_t p_align) noexcept {
  auto free = internal::find_free(p_length, p_align);
  void *const empty = nullptr;
  if (std::get<0>(free) == empty) {
    free = internal::alloc_free(p_length, p_align);
    if (std::get<0>(free) == empty) {
      return std::make_tuple(empty, std::size_t(0));
    }
  }

  void *const unalign_ptr = std::get<0>(free);
  void *const align_ptr = align_pointer(unalign_ptr, p_align);
  ptrdiff_t unalign_length = ptr_diff(align_ptr, unalign_ptr);
  std::size_t align_length = std::get<1>(free) - unalign_length;

  if (align_length > p_length) {
    if (align_ptr != unalign_ptr) {
      internal::return_free(unalign_ptr, unalign_length);
      assert(false);
    }

    internal::return_free(ptr_math(align_ptr, +p_length),
                          align_length - p_length);
    return std::make_tuple(align_ptr, p_length);
  }

  internal::return_free(free);
  assert(false);
  return std::make_tuple(empty, std::size_t(0));
} // alloc()

static bool free(void *const) noexcept {
  // TODO
  return true;
} // free()

local::PoolsRAII *alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE, "");
  auto result = alloc(SP_MALLOC_PAGE_SIZE, alignof(PoolType));

  void *start = std::get<0>(result);
  if (start == nullptr) {
    assert(false);
  }
  return new (start) PoolType;
} // alloc_pool()

void release_pool(local::PoolsRAII *) noexcept {
  // TODO pool is of size SP_MALLOC_PAGE_SIZE
} // release_pool()

} // namespace global

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
// {{{
Pools::Pools() noexcept //
    : pools{nullptr} {
  pools = global::alloc_pool();
}

Pools::~Pools() noexcept {
  if (pools) {
    global::release_pool(pools);
    pools = nullptr;
  }
}

auto &Pools::buckets() noexcept {
  return pools->buckets;
}
// }}}

// {{{
static thread_local Pools pools;
// }}}
std::size_t index_of(std::size_t size) noexcept {
  return (size / 8) - 1;
}

static Pool &pool_for(Pools &pools, std::size_t sz) noexcept {
  std::size_t index = index_of(sz);
  return pools.buckets()[index];
} // pool_for()

static std::tuple<void *, std::size_t> alloc(std::size_t sz) noexcept {
  return global::alloc(sz, 8);
} // alloc()

static NodeHeader *node_header(void *const start) noexcept {
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(NodeHeader) == 0);
  return reinterpret_cast<NodeHeader *>(start);
} // node_header()

static ExtentHeader *extent_header(void *const start) noexcept {
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(NodeHeader);
  assert(headerPtr % alignof(ExtentHeader) == 0);

  return reinterpret_cast<ExtentHeader *>(headerPtr);
} // extent_header()

static void * //
    init_extent(void *const raw, std::size_t bucket,
                std::size_t nodeSz) noexcept {
  // TODO calc based on bucket and nodeSz
  std::size_t extentIdxs = 0;

  NodeHeader *nHdr = node_header(raw);
  new (nHdr) NodeHeader(nodeSz, bucket, extentIdxs);

  ExtentHeader *eHdr = extent_header(raw);
  new (eHdr) ExtentHeader;
  return raw;
} // init()

/*
 * @start - node start
 * desc   -
 */
static void *next_node(void *const start) noexcept {
  auto header = node_header(start);
  return header->next.load();
} // next_node()

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
} // pointer_at()

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
} // next_intent()

/*
 * @start - extent start
 * desc  - Used by malloc
 */
static void *reserve(void *const start) noexcept {
  NodeHeader *nHdr = node_header(start);
  ExtentHeader *eHdr = extent_header(start);

  const size_t index = eHdr->reserved.swap_first(true, nHdr->size);
  if (index != eHdr->reserved.npos) {
    return pointer_at(start, index);
  }
  return nullptr;
} // reserve()

/*
 * @dealloc - the same pointer as received for mammloc
 * desc  - Used by free
 */
static bool free(void *const dealloc) noexcept {
  // TODO
  uintptr_t ptr = reinterpret_cast<uintptr_t>(dealloc);
  std::size_t maxIdx =
      (ptr & (sizeof(NodeHeader) + sizeof(ExtentHeader))) == 0 ? 63 : 0;

  // TODO fence where apporopriate
  std::atomic_thread_fence(std::memory_order_acquire);

  ExtentHeader *eHdr = extent_header(nullptr);
  const std::size_t idx = 0;
  if (true) {
    // a thread local alloc&free
    if (!eHdr->reserved.set(idx, false)) {
      // dubble free
      assert(false);
    }
    // TODO reclaim node?
  } else {
    // not the same free:ing as malloc:ing thread
    return global::free(dealloc);
  }
  return true;
} // free()

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
static void *alloc_extent(std::size_t bucketSz) noexcept {
  // TODO calculate size of extent
  std::size_t extentSz(0);
  auto mem = alloc(extentSz);
  void *const start = std::get<0>(mem);
  std::size_t length = std::get<1>(mem);
  if (start) {
    return init_extent(start, bucketSz, length);
  }
  return nullptr;
}

/*
 * @size - extent start
 * desc  - Used by malloc
 */
static void extend_extent(void *const start) noexcept {
  // TODO
}

} // namespace local

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */

void *sp_malloc(std::size_t sz) noexcept {
  const std::size_t bucketSz = round_even(sz);
  local::Pool &pool = pool_for(local::pools, bucketSz);

  std::shared_lock<std::shared_mutex> lock(pool.lock);
  void *start = pool.start.load();
  if (start) {
  reserve_start:
    void *result = local::reserve(start);
    if (result) {
      return result;
    } else {
      void *const next = local::next_extent(start);
      if (next) {
        start = next;
        goto reserve_start;
      } else {
        // TODO  either extend extent or allocate new extent
        // does not need to do the local::alloc under the exclusive lock
        // since we will only have one TL allocater at a time
      }
    }
  } else {
    // TODO shared lock not required here

    // only TL allowed to malloc meaning no alloc contention
    void *const extent = local::alloc_extent(bucketSz);
    if (extent) {
      void *const result = local::reserve(extent);
      std::atomic_thread_fence(std::memory_order_release);
      if (pool.start.compare_exchange_strong(start, extent)) {
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
} // sp_malloc()

void sp_free(void *const dealloc) noexcept {
  // TODO a initial std::thread_atomic_fence_acquire(); required
  // the allocating thread is required to atomuc_release it before the free
  // thread can see it and there fore only the first initial aquire fence.
  // Because any _aquire fence since it can be another thread that performs the
  // free than the one allocating it
  if (!local::free(dealloc)) {
    assert(false);
  }
} // sp_free()

int main() {
  // init();
  printf("size NodeHeader:%lu\n", sizeof(NodeHeader));
}
