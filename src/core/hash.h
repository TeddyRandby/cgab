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

static u64 hash(const void *buf, u64 len, u64 seed) {
  const u64 m = 0x880355f21e6d1965ULL;
  const u64 *pos = (const u64 *)buf;
  const u64 *end = pos + (len / 8);
  const unsigned char *pos2;
  u64 h = seed ^ (len * m);
  u64 v;

  while (pos != end) {
    v = *pos++;
    h ^= mix(v);
    h *= m;
  }

  pos2 = (const unsigned char *)pos;
  v = 0;

  switch (len & 7) {
  case 7:
    v ^= (u64)pos2[6] << 48;
  case 6:
    v ^= (u64)pos2[5] << 40;
  case 5:
    v ^= (u64)pos2[4] << 32;
  case 4:
    v ^= (u64)pos2[3] << 24;
  case 3:
    v ^= (u64)pos2[2] << 16;
  case 2:
    v ^= (u64)pos2[1] << 8;
  case 1:
    v ^= (u64)pos2[0];
    h ^= mix(v);
    h *= m;
  }

  return mix(h);
}

#undef mix

static inline u64 hash_bytes(u64 size, u8 bytes[size]) {
  u64 seed = 2166136261u;
  return hash(bytes, size * sizeof(u8), seed);
}

static inline u64 hash_words(u64 size, u64 words[size]) {
  u64 seed = 2166136261u;
  return hash(words, size * sizeof(u64), seed);
}
#endif
