#pragma once
// SHA-256 content identifiers (FIPS 180-4). No authentication/key material.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
namespace s3g_thru_pack {
class Sha256 {
  std::array<std::uint32_t, 8> state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::array<unsigned char, 64> pending{};
  std::size_t used = 0;
  std::uint64_t bytes = 0;
  static std::uint32_t r(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  void block(const unsigned char *b) {
    static constexpr std::uint32_t k[]{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    std::uint32_t w[64]{};
    for (int i = 0; i < 16; ++i)
      w[i] = (std::uint32_t(b[i * 4]) << 24) | (std::uint32_t(b[i * 4 + 1]) << 16) |
             (std::uint32_t(b[i * 4 + 2]) << 8) | b[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
      const auto a = w[i - 15], z = w[i - 2];
      w[i] = w[i - 16] + (r(a, 7) ^ r(a, 18) ^ (a >> 3)) + w[i - 7] +
             (r(z, 17) ^ r(z, 19) ^ (z >> 10));
    }
    auto a = state[0], c = state[2], b1 = state[1], d = state[3], e = state[4], f = state[5],
         g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
      const auto t1 = h + (r(e, 6) ^ r(e, 11) ^ r(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      const auto t2 = (r(a, 2) ^ r(a, 13) ^ r(a, 22)) + ((a & b1) ^ (a & c) ^ (b1 & c));
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b1;
      b1 = a;
      a = t1 + t2;
    }
    state[0] += a;
    state[1] += b1;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
  }

public:
  void update(const void *data, std::size_t count) {
    const auto *p = static_cast<const unsigned char *>(data);
    bytes += count;
    while (count) {
      const auto n = std::min(count, pending.size() - used);
      std::memcpy(pending.data() + used, p, n);
      used += n;
      p += n;
      count -= n;
      if (used == 64) {
        block(pending.data());
        used = 0;
      }
    }
  }
  std::string finish() {
    const auto bits = bytes * 8;
    pending[used++] = 0x80;
    if (used > 56) {
      while (used < 64)
        pending[used++] = 0;
      block(pending.data());
      used = 0;
    }
    while (used < 56)
      pending[used++] = 0;
    for (int i = 7; i >= 0; --i)
      pending[used++] = static_cast<unsigned char>(bits >> (i * 8));
    block(pending.data());
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (auto x : state)
      for (int i = 7; i >= 0; --i)
        out += hex[(x >> (i * 4)) & 15];
    return out;
  }
};
inline std::string sha256(std::string_view text) {
  Sha256 hash;
  hash.update(text.data(), text.size());
  return hash.finish();
}
} // namespace s3g_thru_pack
