#include "Barrier.h"
#include <mutex>

namespace sp {
Barrier::Barrier(size_t threads) noexcept //
    : m_mutex{}, m_condition{}, m_remaining{threads} {
}

Barrier::~Barrier() noexcept {
}

void Barrier::await() noexcept {
  size_t expect = m_remaining.load(std::memory_order_acquire);
  size_t replace;
  do {
    if (expect == size_t(0)) {
      /**
       * someone else will notify
       */
      return;
    }
    replace = expect - 1;
  } while (!m_remaining.compare_exchange_strong(expect, replace));

  if (replace == size_t(0)) {
    // TODO if initial threads is 1 then this is not required
    std::unique_lock<std::mutex> guard(m_mutex);
    m_condition.notify_all();
  } else {
    std::unique_lock<std::mutex> guard(m_mutex);
    size_t remaining = m_remaining.load(std::memory_order_acquire);
    if (remaining != size_t(0)) {
      m_condition.wait(guard, [this] { //
        size_t remaining = this->m_remaining.load(std::memory_order_acquire);
        return remaining == size_t(0);
      });
    }
  }
}
} // namespace sp
