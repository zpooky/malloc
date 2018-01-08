#ifndef SP_UTIL_COLLECTION_BITSET_H
#define SP_UTIL_COLLECTION_BITSET_H

namespace sp {
  template<typename T,std::size_t SIZE >
struct StaticBiset {
  T[SIZE] buffer;
  Bitset() noexcept  : buffer{0} {
  }
};

struct DynamicBitset {
  T*buffer;
  Bitset(T*b,std::size_t length): buffer{p} {

  }
};

template<typename Bitset>
bool test(const Bitset&,std::size_t) noexcept;

/*=======
 */
template<typename Bitset>
bool test(const Bitset&b,std::size_t idx) noexcept {
  //TODO
  return true;
}

}


#endif
