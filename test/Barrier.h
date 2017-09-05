#ifndef SP_CONCURRENCY_BARRIER_H
#define SP_CONCURRENCY_BARRIER_H

#include <atomic>
#include <condition_variable>

namespace sp {
class Barrier {
private:
  std::mutex m_mutex;
  std::condition_variable m_condition;

  std::atomic<size_t> m_remaining;

public:
  explicit Barrier(size_t) noexcept;
  ~Barrier() noexcept;

  Barrier(const Barrier &) = delete;
  Barrier(const Barrier &&) = delete;

public:
  void await() noexcept;
};

} // namespace sp

#endif
