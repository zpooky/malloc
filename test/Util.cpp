#include "Util.h"

Range
sub_range(Range &r, size_t ridx, size_t rlength) {
  size_t offset = (ridx * rlength);
  if (!(offset < r.length)) {
    assert(offset < r.length);
  }
  uint8_t *start = ptr_add(r.start, +offset);
  return Range(start, rlength);
}

bool
in_range(void *r, size_t rSz, void *e, size_t eLen) {
  uintptr_t in = reinterpret_cast<uintptr_t>(e);
  uintptr_t inEnd = in + eLen;

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(r);
  uintptr_t rangeEnd = rangeStart + rSz;

  return (in >= rangeStart && in < rangeEnd) && (inEnd <= rangeEnd);
}

void
assert_in_range(Range &range, void *current, size_t bSz) {
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
void
assert_in_range(Range &range, void *current, sp::node_size bSz) {
  return assert_in_range(range, current, std::size_t(bSz));
}

void
assert_no_overlap(const Points &ptrs) {
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

void
sort_points(Points &free) {
  auto cmp = [](const auto &first, const auto &second) -> bool {
    return std::get<0>(first) < std::get<0>(second);
  };
  std::sort(free.begin(), free.end(), cmp);
}

void
assert_consecutive_range(Points &free, Range range) { // spz
  uint8_t *const startR = range.start;

  sort_points(free);

  void *first = startR;
  for (auto it = free.begin(); it != free.end(); ++it) {
    ASSERT_EQ(std::get<0>(*it), first);

    first = ptr_add(std::get<0>(*it), std::get<1>(*it));
  }
  ASSERT_EQ(first, ptr_add(range.start, +range.length));
}

void
assert_no_gaps(Points &free) {
  sort_points(free);

  if (free.size() > 0) {
    auto first = std::get<0>(*free.begin());

    for (auto it = free.begin(); it != free.end(); ++it) {
      ASSERT_EQ(std::get<0>(*it), first);

      first = ptr_add(std::get<0>(*it), std::get<1>(*it));
    }
  }
}

namespace test {

StackHead::StackHead(StackHead *n, std::size_t l)
    : next(n)
    , length(l) {
}

MemStack::MemStack()
    : lock()
    , head(nullptr) {
}

void
enqueue(MemStack &s, void *data, std::size_t length) {
  assert(length >= sizeof(StackHead));
  uintptr_t p = reinterpret_cast<std::uintptr_t>(data);
  if (p % alignof(StackHead) != 0) {
    printf("alingof  assert fail\n");
    assert(false);
  }
  sp::EagerExclusiveLock guard(s.lock);
  if (guard) {
    StackHead *head = new (data) StackHead(s.head, length);
    s.head = head;
  }
}

util::maybe<std::tuple<void *, std::size_t>>
dequeue(MemStack &s) {
  sp::EagerExclusiveLock guard(s.lock);
  if (guard) {
    StackHead *head = s.head;
    if (head) {
      s.head = head->next;

      std::tuple<void *, std::size_t> r(head, head->length);
      return util::maybe<std::tuple<void *, std::size_t>>(r);
    }
  }
  return {};
}
} // namespace test
//
//==================================================================================================
std::size_t
roundAlloc(std::size_t sz) {
  for (std::size_t i(8); i < ~std::size_t(0); i <<= 1) {
    if (sz <= i) {
      // printf("sz(%zu)%zu\n", sz, i);
      return i;
    }
  }
  assert(false);
  return 0;
}
