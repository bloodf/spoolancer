#pragma once
#include <cstdint>
namespace Discovery {
struct Range { uint32_t first, last, self, mask; };
inline Range range(uint32_t self, uint32_t mask) {
  const uint32_t local24 = self & 0xffffff00UL;
  const uint32_t network = self & mask, broadcast = network | ~mask;
  const uint32_t first = network + 1 > local24 + 1 ? network + 1 : local24 + 1;
  const uint32_t last = broadcast - 1 < local24 + 254 ? broadcast - 1 : local24 + 254;
  return {first, last, self, mask};
}
inline bool eligible(uint32_t candidate, const Range &scope) {
  const uint8_t first = candidate >> 24;
  return first != 0 && first != 127 && first < 224 && candidate != scope.self &&
         candidate > (scope.self & scope.mask) && candidate < ((scope.self & scope.mask) | ~scope.mask) &&
         (candidate & scope.mask) == (scope.self & scope.mask);
}
}
