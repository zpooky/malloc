#ifndef SP_MALLOC_SHARED_H
#define SP_MALLOC_SHARED_H

#include "ReadWriteLock.h"
#include <atomic>
#include <bitset/Bitset.h>
#include <type_traits>

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
} // namespace header

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
std::size_t trailing_zeros(std::size_t) noexcept;
std::size_t leading_zeros(std::size_t) noexcept;
std::size_t round_up(std::size_t data, std::size_t eventMultiple) noexcept;

template <typename T>
class maybe {
private:
  typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
  bool present;

public:
  maybe() noexcept //
      : data{}, present{false} {
  }
  template <typename I>
  explicit maybe(I &&d) // noexcept(if construct)
      : data{}, present(true) {
    new (&data) T{std::forward<I>(d)};
  }
  // TODO inplace construction

  ~maybe() noexcept {
    if (present) {
      present = false;
      T *ptr = reinterpret_cast<T *>(&data);
      ptr->~T();
    }
  }

  operator bool() const noexcept {
    return present;
  }

  const T &get() const &noexcept {
    T *ptr = reinterpret_cast<T *>(&data);
    return *ptr;
  }
  T &get() & noexcept {
    T *ptr = reinterpret_cast<T *>(&data);
    return *ptr;
  }
  T &&get() && noexcept {
    T *ptr = reinterpret_cast<T *>(&data);
    return std::move(*ptr);
  }

  const T &get_or(T &def) const noexcept {
    if (present) {
      return get();
    }
    return def;
  }

  T &get_or(T &def) noexcept {
    if (present) {
      return get();
    }
    return def;
  }
};
} // namespace util

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

  PoolsRAII() noexcept;
}; // struct PoolsRAII

/*Pools*/
class Pools { //
private:
  PoolsRAII *pools;
  // no thread holds this instance but all memeory is not reclaimed
  std::atomic<bool> reclaimed;

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

} // namespace local

#endif
