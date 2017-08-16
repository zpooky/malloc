#ifndef _SP_MALLOC_H
#define _SP_MALLOC_H

#include <shared_mutex>

namespace sp {
class ReadWriteLock { //
private:
  std::shared_mutex lock;
public:
};
}

#endif
