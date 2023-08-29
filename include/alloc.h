#ifndef GAB_ALLOC_H
#define GAB_ALLOC_H

#include "include/core.h"

#define CHUNK_MAX_SIZE 256

#define CHUNK_LEN 64

typedef struct gab_chunk gab_chunk;

struct gab_chunk {
  gab_chunk *prev;
  gab_chunk *next;
  uint64_t mask;
  uint8_t size;
  uint8_t bytes[FLEXIBLE_ARRAY];
};

typedef struct {
  gab_chunk *chunks[CHUNK_MAX_SIZE];
} gab_allocator;

#endif
