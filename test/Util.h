#ifndef SP_MALLOC_TEST_UTIL_H
#define SP_MALLOC_TEST_UTIL_H

#include <tuple>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include "gtest/gtest.h"

using Point = std::tuple<void *, std::size_t>;
using Points = std::vector<Point>;

template <typename T>
static inline T *pmath(T *v, ptrdiff_t diff) {
  uintptr_t result = reinterpret_cast<uintptr_t>(v);
  result = result + diff;
  return reinterpret_cast<T *>(result);
}

struct Range {
  uint8_t *start;
  size_t length;
  Range(uint8_t *ps, size_t pl) //
      : start(ps), length(pl) {
  }
  Range() //
      : Range(nullptr, 0) {
  }
  uint8_t &operator[](size_t idx) {
    return start[idx];
  }
  const uint8_t &operator[](size_t idx) const {
    return start[idx];
  }

  uint8_t *raw_offset(size_t off) {
    assert(off < length);
    return pmath(start, +off);
  }
};

static inline Range sub_range(Range &r, size_t ridx, size_t rlength) {
  size_t offset = (ridx * rlength);
  if (!(offset < r.length)) {
    assert(offset < r.length);
  }
  uint8_t *start = pmath(r.start, +offset);
  return Range(start, rlength);
}

static inline bool in_range(void *r, size_t rSz, void *e, size_t eLen) {
  uintptr_t in = reinterpret_cast<uintptr_t>(e);
  uintptr_t inEnd = in + eLen;

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(r);
  uintptr_t rangeEnd = rangeStart + rSz;

  return (in >= rangeStart && in < rangeEnd) && (inEnd <= rangeEnd);
}

template <typename T>
static inline size_t size_of_free(const T &free) {
  size_t result(0);
  for (const auto &current : free) {
    result += std::get<1>(current);
  }
  return result;
}

template <typename T>
static inline bool in_range(T &b, T &e) {
  uintptr_t strtB = reinterpret_cast<uintptr_t>(std::get<0>(b));
  std::size_t lenB = std::get<1>(b);

  uintptr_t strtE = reinterpret_cast<uintptr_t>(std::get<0>(e));
  std::size_t lenE = std::get<1>(e);

  return (strtB >= strtE && strtB < (strtE + lenE)) ||
         (strtE >= strtB && strtE < (strtB + lenB));
}

static inline void assert_in_range(Range &range, void *current, size_t bSz) {
  if (!in_range(range.start, range.length, current, bSz)) {
    printf("in_range(%p, %zu, %p, "
           "%zu)|range(%zu-%zu[l:%zu])|cur(%zu-%zu[l:%zu])\n",      //
           range.start, range.length, current, bSz,                 //
           reinterpret_cast<uintptr_t>(range.start),                //
           reinterpret_cast<uintptr_t>(range.start) + range.length, //
           range.length,                                            //
           reinterpret_cast<uintptr_t>(current),                    //
           reinterpret_cast<uintptr_t>(current) + bSz,              //
           bSz);

    // uintptr_t rangeStartPtr = reinterpret_cast<uintptr_t>(range);
    // void *const rangeStart = reinterpret_cast<void *>(rangeStartPtr);
    // void *const rangeEnd = reinterpret_cast<void *>(rangeStartPtr + SIZE);
    //
    // printf("range[%p-%p]", rangeStart, rangeEnd);
    // printf("current[%p,%zu]\n", current, bucketSz);
    // printf("rangeStart=%p\n", rangeStart);
    // printf("rangeEnd=%p\n", rangeEnd);
    // printf("curStart=%p\n", current);
    // void *curEnd = (void *)(reinterpret_cast<uintptr_t>(current) +
    // bucketSz);
    // printf("curEnd=%p\n", curEnd);
    // printf("rangeStart > curStart and curStart < rangeEnd and curEnd <= "
    //        "rangeEnd\n");
    // printf("%p > %p and %p < %p and %p <= %p\n", //
    //        rangeStart, current, current, rangeEnd, curEnd, rangeEnd);
    ASSERT_TRUE(false);
  }
}

static inline void assert_no_overlap(const Points &ptrs) {
  // printf("assert_no_overlap(points:%zu)\n", ptrs.size());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    auto it = current;
    while (++it != ptrs.end()) {
      if (in_range(*current, *it)) {
        // printf("    current[%p, %zu]\n\t it[%p, %zu]",       //
        //        std::get<0>(*current), std::get<1>(*current), //
        //        std::get<0>(*it), std::get<1>(*it));
        printf(" == false\n");
        ASSERT_TRUE(false);
      } else {
        // printf(" == true\n");
      }
    }
    current++;
  }
}

#endif
