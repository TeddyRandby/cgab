#ifndef CORE_HASH_H
#define CORE_HASH_H

#include <stddef.h>
#include <stdint.h>

//https://github.com/amakukha/minimal_hashes
static inline uint64_t FNV1a_64(const uint8_t* data, size_t size) {
    uint64_t h = 0xcbf29ce484222325UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 0x00000100000001B3UL;
    }
    return h;
}

static inline uint64_t hash_bytes(uint64_t len, uint8_t bytes[len]) {
  return FNV1a_64(bytes, len * sizeof(uint8_t));
}
#endif
