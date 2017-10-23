#include "Util.h"

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
