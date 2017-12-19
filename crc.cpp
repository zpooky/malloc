#include "crc.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>

// https://github.com/Michaelangel007/crc32
// http://www.ross.net/crc/download/crc_v3.txt

// https://www.lammertbies.nl/comm/info/crc-calculation.html
// http://crccalc.com/

namespace sp {

using byte = unsigned char;
using crc32_t = std::uint32_t;

template <typename T>
static T
reverse(const T &d) noexcept {
  // TODO
  return d;
}

template <typename crc_t>
static crc_t
crc_reversed(const void *b, std::size_t length, crc_t polynomial) noexcept {
  // - reversed polynomial
  // - reversed the reversed bytes
  // - reversed the reversed final CRC value

  constexpr std::size_t bits = 8;
  constexpr std::size_t crc_bits = sizeof(crc_t) * bits;
  constexpr crc_t high_bit = crc_t(1) << crc_t(crc_bits - 1);

  const byte *it = (const byte *)b;
  crc_t crc = -1;

  while (length--) {
    constexpr std::size_t left_shift = (crc_bits - bits);
    const crc_t current = crc_t(reverse(*it++)) << left_shift;
    crc = crc ^ current;

    // While (more message bits)
    //    Begin
    //    Shift the register left by one bit, reading the next bit of the
    //       augmented message into register bit position 0.
    //    If (a 1 bit popped out of the register during step 3)
    //       Register = Register XOR Poly.
    //    End
    // The register now contains the remainder.
    for (std::size_t bit = 0; bit < bits; ++bit) {
      // check if the bit being shifted left is 1
      const bool popped = crc & high_bit == high_bit;
      crc = (crc << 1);

      if (popped) {
        crc = crc ^ polynomial;
      }
    }
  }

  return reverse(~crc);
}

template <typename crc_t>
static crc_t
crc(const void *b, std::size_t length, std::uint32_t polynomial) noexcept {
  // This version of the algorithm is the faster implementation in hardware,
  // implemented by shifting bits right-to-left using a barrel shifter. As
  // oposed to the reversed algorithm which is the faster software
  // implementation.

  constexpr std::size_t bits = 8;

  const byte *it = (const byte *)b;
  crc_t crc = ~crc_t(0);

  while (length--) {
    const crc_t current = crc_t(*it++);
    crc = crc ^ current;

    for (std::size_t bit = 0; bit < bits; ++bit) {
      const bool popped = crc & crc_t(1);
      crc = (crc >> 1);

      if (popped) {
        crc = crc ^ polynomial;
      }
    }
  }
  return ~crc;
}

std::uint32_t
crc32c(const void *b, std::size_t len) noexcept {
  // return crc_reversed<crc32_t>(b, len, crc32_t(0x1EDC6F41));
  return crc<crc32_t>(b, len, crc32_t(0x82F63B78));
}

std::uint32_t
crc32(const void *b, std::size_t len) noexcept {
  // return crc_reversed<crc32_t>(b, len, crc32_t(0x04C11DB7));
  return crc<crc32_t>(b, len, crc32_t(0xEDB88320));
}

//==========================================================
static void
dht_instance(std::uint8_t *ip, int num_octets, std::uint8_t (&node_id)[20]) {
  std::uint8_t v4_mask[] = {0x03, 0x0f, 0x3f, 0xff};
  std::uint8_t v6_mask[] = {0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
  std::uint8_t *mask = num_octets == 4 ? v4_mask : v6_mask;

  for (int i = 0; i < num_octets; ++i) {
    ip[i] &= mask[i];
  }

  std::uint32_t rand = std::rand() & 0xff;
  std::uint8_t r = rand & 0x7;
  ip[0] |= r << std::uint8_t(5);

  std::uint32_t crc = crc32c(ip, num_octets);

  // only take the top 21 bits from crc
  node_id[0] = (crc >> 24) & std::uint8_t(0xff);
  node_id[1] = (crc >> 16) & std::uint8_t(0xff);
  node_id[2] = ((crc >> 8) & std::uint8_t(0xf8)) | (std::rand() & 0x7);
  for (int i = 3; i < 19; ++i) {
    node_id[i] = std::rand();
  }
  node_id[19] = rand;
}

template <std::size_t size>
static bool
FromHex(byte (&theDest)[size], const char *theSource) {
  const char *it = theSource;
  std::uint8_t lookup['F' + 1];
  lookup['0'] = 0x0;
  lookup['1'] = 0x1;
  lookup['2'] = 0x2;
  lookup['3'] = 0x3;
  lookup['4'] = 0x4;
  lookup['5'] = 0x5;
  lookup['6'] = 0x6;
  lookup['7'] = 0x7;
  lookup['8'] = 0x8;
  lookup['9'] = 0x9;
  lookup['A'] = 0xA;
  lookup['B'] = 0xB;
  lookup['C'] = 0xC;
  lookup['D'] = 0xD;
  lookup['E'] = 0xE;
  lookup['F'] = 0xF;

  std::size_t i = 0;
  while (*it) {
    if (i > size) {
      return false;
    }

    std::uint8_t f = lookup[static_cast<int>(*it++)];
    f <<= std::uint8_t(4);
    std::uint8_t s = lookup[static_cast<int>(*it++)];
    theDest[i++] = f | s;
  }
  return true;
}

template <std::size_t size>
static bool
eq(std::uint8_t (&node_id)[size], const char *hex) {
  std::uint8_t cmp_id[size] = {0};
  assert(FromHex(cmp_id, hex));
  return std::memcmp(node_id, cmp_id, size) == 0;
}

static void
dht_test() {
  {
    std::srand(1);
    std::uint8_t node_id[20];
    std::uint8_t ip[4] = {124, 31, 75, 21};
    dht_instance(ip, sizeof(ip), node_id);
    assert(eq(node_id, "5fbfbff10c5d6a4ec8a88e4c6ab4c28b95eee401"));
  }
}

static void
crc32c_test() {
  {
    const char in[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::uint32_t res = crc32c(in, sizeof(in));
    std::uint32_t cmp = 0xe3069283;
    assert(res == cmp);
  }
  {
    char in[32];
    std::memset(in, 0, sizeof(in));

    std::uint32_t res = crc32c(in, sizeof(in));
    std::uint32_t cmp = 0x8A9136AA;
    assert(res == cmp);
  }
  {
    unsigned char in[32];
    std::memset(in, 0xff, sizeof(in));

    std::uint32_t res = crc32c(in, sizeof(in));
    std::uint32_t cmp = 0x62A8AB43;
    assert(res == cmp);
  }
  // {
  //   char in[32] = {0};
  //   for (char i = 0; i < sizeof(in); ++i) {
  //     in[(int)i] = i;
  //   }
  //
  //   std::uint32_t res = crc32c(in, sizeof(in));
  //   std::uint32_t cmp = 0x4e79dd46;
  //   assert(res == cmp);
  // }
  // {
  //   char in[32] = {0};
  //
  //   std::size_t it = 0;
  //   for (char i = 32; i-- > 0;) {
  //     in[(int)it++] = i;
  //   }
  //   std::uint32_t res = crc32c(in, sizeof(in));
  //   std::uint32_t cmp = 0x5cdb3f11;
  //   assert(res == cmp);
  // }
  // {
  //   unsigned char in[] = {
  //       0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  //       0x00,
  //       0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  //       0x00,
  //       0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00,
  //       0x00,
  //       0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  //       0x00};
  //   std::uint32_t res = crc32c(in, sizeof(in));
  //   std::uint32_t cmp = 0x563a96d9;
  //   assert(res == cmp);
  // }
}

static void
crc32_test() {
  {
    const char in[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::uint32_t res = crc32(in, sizeof(in));
    std::uint32_t cmp = 0xCBF43926;
    assert(res == cmp);
  }
  {
    const char in[] = "The quick brown fox jumps over the lazy dog";
    std::uint32_t res = crc32(in, sizeof(in) - 1);
    std::uint32_t cmp = 0x414fa339;
    assert(res == cmp);
  }
}

void
crc_test() noexcept {
  crc32_test();
  crc32c_test();
  // dht_test();
}

// void
// crc_test() noexcept {
// }
} // namespace sp
