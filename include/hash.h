#ifndef CORE_HASH_H
#define CORE_HASH_H
#include "types.h"

/*
   https://github.com/ztanml/fast-hash/blob/master/fasthash.c
*/

#define mix(h)                                                                 \
  ({                                                                           \
    (h) ^= (h) >> 23;                                                          \
    (h) *= 0x2127599bf4325c37ULL;                                              \
    (h) ^= (h) >> 47;                                                          \
  })

static uint64_t hash(const char *buf, uint64_t len, uint64_t seed) {
  const uint64_t m = 0x880355f21e6d1965ULL;
  const uint64_t *pos = (const uint64_t *)buf;
  const uint64_t *end = pos + (len / 8);
  const unsigned char *pos2;
  uint64_t h = seed ^ (len * m);
  uint64_t v;

  while (pos != end) {
    v = *pos++;
    h ^= mix(v);
    h *= m;
  }

  pos2 = (const unsigned char *)pos;
  v = 0;

  switch (len & 7) {
  case 7:
    v ^= (uint64_t)pos2[6] << 48;
  case 6:
    v ^= (uint64_t)pos2[5] << 40;
  case 5:
    v ^= (uint64_t)pos2[4] << 32;
  case 4:
    v ^= (uint64_t)pos2[3] << 24;
  case 3:
    v ^= (uint64_t)pos2[2] << 16;
  case 2:
    v ^= (uint64_t)pos2[1] << 8;
  case 1:
    v ^= (uint64_t)pos2[0];
    h ^= mix(v);
    h *= m;
  }

  return mix(h);
}

#undef mix

static inline uint64_t hash_bytes(uint64_t seed, uint64_t len,
                                  uint8_t bytes[len]) {
  return hash((char *)bytes, len * sizeof(uint8_t), seed);
}

static inline uint64_t hash_words(uint64_t seed, uint64_t len,
                                  uint64_t words[len]) {
  return hash((char *)words, len * sizeof(uint64_t), seed);
}
#endif
