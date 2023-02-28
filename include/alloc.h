#ifndef GAB_ALLOC_H
#define GAB_ALLOC_H

#include "include/core.h"

#define CHUNK_MAX_SIZE 256
#define CHUNK_LEN 64

typedef struct gab_chunk gab_chunk;

struct gab_chunk {
  gab_chunk *prev;
  gab_chunk *next;
  u64 mask;
  u8 size;
  u8 bytes[FLEXIBLE_ARRAY];
};

typedef struct {
  gab_chunk *chunks[CHUNK_MAX_SIZE];
} gab_allocator;

gab_allocator *gab_allocator_create();

void gab_allocator_destroy(gab_allocator *chunks);

#endif
