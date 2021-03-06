#ifndef SP_MALLOC_TEST_UTIL_H
#define SP_MALLOC_TEST_UTIL_H

#include "gtest/gtest.h"
#include <concurrent/ReadWriteLock.h>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <pthread.h>
#include <shared.h>
#include <tuple>
#include <util.h>
#include <vector>

using Point = std::tuple<void *, std::size_t>;
using Points = std::vector<Point>;

namespace test {
// template<std::size_t length>
struct StackHeadNoSize {
  StackHeadNoSize *next;

  StackHeadNoSize(StackHeadNoSize *n, std::size_t /*l*/)
      : next(n) {
    // assert(length ==l);
  }

  std::size_t
  get_length() const {
    // return length;
    return 0;
  }
};

struct StackHead {
  StackHead *next;
  std::size_t length;

  StackHead(StackHead *n, std::size_t l)
      : next(n)
      , length(l) {
  }

  std::size_t
  get_length() const {
    return length;
  }
};

template <typename Stack = StackHead>
struct MemStack {
  sp::ReadWriteLock lock;
  Stack *head;

  MemStack()
      : lock()
      , head(nullptr) {
  }

  ~MemStack() {
    sp::EagerExclusiveLock guard(lock);
  }
};

template <typename Stack>
void
enqueue(MemStack<Stack> &s, void *data, std::size_t length) {
  // printf("length[%zu] >= Stack[%zu]\n", length, sizeof(Stack));
  assert(length >= sizeof(Stack));
  uintptr_t p = reinterpret_cast<std::uintptr_t>(data);
  if (p % alignof(Stack) != 0) {
    printf("alingof  assert fail\n");
    assert(false);
  }
  sp::EagerExclusiveLock guard(s.lock);
  if (guard) {
    Stack *head = new (data) Stack(s.head, length);
    s.head = head;
  }
}

template <typename Stack>
sp::maybe<std::tuple<void *, std::size_t>>
dequeue(MemStack<Stack> &s) {
  sp::EagerExclusiveLock guard(s.lock);
  if (guard) {
    Stack *head = s.head;
    if (head) {
      s.head = head->next;

      std::tuple<void *, std::size_t> r(head, head->get_length());
      return sp::maybe<std::tuple<void *, std::size_t>>(r);
    }
  }
  return {};
}
} // namespace test

template <typename T>
static inline T *
ptr_add(T *v, ptrdiff_t diff) {
  uintptr_t result = reinterpret_cast<uintptr_t>(v);
  result = result + diff;
  return reinterpret_cast<T *>(result);
}

struct Range {
  uint8_t *start;
  size_t length;
  Range(uint8_t *ps, size_t pl) //
      : start(ps)
      , length(pl) {
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

  uint8_t *
  raw_offset(sp::node_size off) {
    assert(off < length);
    return ptr_add(start, +std::size_t(off));
  }
};

Range
sub_range(Range &r, size_t ridx, size_t rlength);

bool
in_range(void *r, size_t rSz, void *e, size_t eLen);

static inline bool
in_rangex(void *r, size_t rSz, void *e, size_t eLen) {
  return in_range(r, rSz, e, eLen);
}

template <typename T>
static inline size_t
size_of_free(const T &free) {
  size_t result(0);
  for (const auto &current : free) {
    result += std::get<1>(current);
  }
  return result;
}

template <typename T>
static inline bool
in_range(T &b, T &e) {
  uintptr_t strtB = reinterpret_cast<uintptr_t>(std::get<0>(b));
  std::size_t lenB = std::get<1>(b);

  uintptr_t strtE = reinterpret_cast<uintptr_t>(std::get<0>(e));
  std::size_t lenE = std::get<1>(e);

  return (strtB >= strtE && strtB < (strtE + lenE)) ||
         (strtE >= strtB && strtE < (strtB + lenB));
}

void
assert_in_range(const Range &range, void *current, size_t bSz);

void
assert_in_range(const Range &range, void *current, sp::node_size bSz);

void
assert_no_overlap(const Points &ptrs);

// void
// assert_no_overlap(const test::MemStack<test::StackHeadNoSize> &ptrs,
//                   std::size_t);

void
sort_points(Points &free);

void
assert_consecutive_range(Points &free, Range range);

void
assert_no_gaps(Points &free);

template <typename Function>
static inline void
time(const char *msg, Function f) {
  auto t1 = std::chrono::high_resolution_clock::now();
  f();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << msg << ":"
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
}

template <typename T>
bool
memeq(T *ptr, std::size_t capacity, T value) noexcept {
  for (std::size_t i = 0; i < capacity; ++i) {
    if (*ptr != value) {
      return false;
    }
    ++ptr;
  }
  return true;
}

//==================================================================================================
std::size_t
roundAlloc(std::size_t sz);

using Worker_t = void *(*)(void *);

template <typename Argument>
static void
threads(Argument &arg, std::vector<Worker_t> workers) {
  std::vector<pthread_t> tids;
  for (auto worker : workers) {
    pthread_t tid = 0;
    int ret = pthread_create(&tid, nullptr, worker, &arg);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(tid == 0);
    tids.push_back(tid);
  }

  for (auto tid : tids) {
    int ret = pthread_join(tid, nullptr);
    ASSERT_EQ(0, ret);
  }
}

template <typename Argument>
static void
threads(std::size_t threads, Argument &arg, Worker_t worker) {
  std::vector<pthread_t> tids;
  for (std::size_t i(0); i < threads; ++i) {
    pthread_t tid = 0;
    int ret = pthread_create(&tid, nullptr, worker, &arg);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(tid == 0);
    tids.push_back(tid);
  }

  for (auto tid : tids) {
    int ret = pthread_join(tid, nullptr);
    ASSERT_EQ(0, ret);
  }
}

#endif
