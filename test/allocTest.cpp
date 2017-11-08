#include "Barrier.h"
#include "Util.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <alloc.h>

//==================================================================================================
class AllocTestAllocSizePFixture : public testing::TestWithParam<size_t> {
public:
  AllocTestAllocSizePFixture() {
  }

  virtual void
  SetUp() {
  }

  virtual void
  TearDown() {
  }
};

INSTANTIATE_TEST_CASE_P(DefaultInstance, AllocTestAllocSizePFixture,
                        ::testing::Values(8, 16, 32, 64, 128, 256, 512, 1024,
                                          2048, 4096, 8192));

//==================================================================================================
TEST_P(AllocTestAllocSizePFixture, test_alloc) {
  const size_t allocSz = GetParam();
  local::PoolsRAII pools;
  void*ptr = shared::alloc(pools, allocSz);
  ASSERT_TRUE(ptr == nullptr);
}
//==================================================================================================
