#include "Util.h"
#include "malloc.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <tuple>
#include <vector>

static constexpr std::size_t HEADER_SIZE =
    sizeof(header::Node) + sizeof(header::Extent);

/*Test*/
static std::size_t calc_min_extent(std::size_t bucketSz) noexcept {
  assert(bucketSz >= 8);
  assert(bucketSz % 8 == 0);

  // TODO make not a loop

  constexpr std::size_t min_alloc = SP_MALLOC_PAGE_SIZE;
  constexpr std::size_t max_min_alloc = min_alloc * 2;

  std::size_t result = HEADER_SIZE;
  std::size_t indicies = 0;
  while (indicies < header::Extent::MAX_BUCKETS && result < max_min_alloc) {
    result += bucketSz;
    ++indicies;
  }
  if(indicies == header::Extent::MAX_BUCKETS){
  }
  return result;
}

TEST(MallocTest, test) {
  Points allocs;
  for (size_t x = 1; x < 3; ++x) {
    for (size_t i = 1; i < 3; ++i) {
      // void *const ptr = sp_malloc(i);
      // allocs.emplace_back(ptr, i);
      // sp_free(ptr);
    }
  }
}

TEST(MallocTest, test_calc_min_extent) {
  std::size_t bucketSz = 8;
  size_t idx = 0;
  while (bucketSz > 0) {
    std::size_t extSz = calc_min_extent(bucketSz);

    std::size_t idxs = (extSz - HEADER_SIZE) / bucketSz;
    printf("%zu. bucket:%zu|ext:%zu|idxs:%zu\n", //
           idx++, bucketSz, extSz, idxs);
    bucketSz = bucketSz << 1;
  }
}

// ./test/thetest --gtest_filter="*MallocTest*"
