#ifndef _SP_MALLOC_H
#define _SP_MALLOC_H

#include <atomic>
#include <cstdint>

namespace sp {
class ReadWriteLock { //
private:
  std::atomic<std::uint64_t> m_state;

public:
  ReadWriteLock();
  ~ReadWriteLock();

public:
  void shared_lock() noexcept;
  void shared_unlock() noexcept;

public:
  void exclusive_lock() noexcept;
  void exclusive_unlock() noexcept;
};

//TODO SharedLock increment shared count if exclusivebit is 0
//TODO EagerExclusiveLock swap exclusivebit to 1 and wait until shared 0
//TODO LazyExclusiveLock wap exclusivebut to 1 if shared is 0 otherwise retry
class ExclusiveLock { //
private:
  ReadWriteLock &m_lock;

public:
  explicit ExclusiveLock(ReadWriteLock &);
  ~ExclusiveLock();
};

class SharedLock { //
private:
  ReadWriteLock &m_lock;

public:
  explicit SharedLock(ReadWriteLock &);
  ~SharedLock();
};
}

#endif
