#include "include/gab.h"
#include <stdint.h>

#define CHUNK_MAX_SIZE 256
#define CHUNK_LEN 64

#define ctz __builtin_ctzl

typedef struct Chunk Chunk;
struct Chunk {
  Chunk *prev;
  Chunk *next;
  u64 mask;
  u8 size;
  u8 bytes[FLEXIBLE_ARRAY];
};

typedef struct {
  Chunk *chunks[CHUNK_MAX_SIZE];
} Chunks;

static inline Chunk *chunk_create(Chunks *s, u8 size) {
  Chunk *old_chunk = s->chunks[size];
  Chunk *self = malloc(sizeof(Chunk) + (CHUNK_LEN * size));
  self->mask = 0;
  self->size = size;

  self->prev = NULL;
  self->next = old_chunk;

  if (old_chunk != NULL) {
    old_chunk->prev = self;
  }

  s->chunks[size] = self;
  return self;
}

static inline void chunk_destroy(Chunks *s, Chunk *c) {
  if (c->prev != NULL) {
    c->prev->next = c->next;
  } else {
    s->chunks[c->size] = c->next;
  }

  if (c->next != NULL) {
    c->next->prev = c->prev;
  }

  free(c);
}

static inline u8 *chunk_begin(Chunk *chunk) { return chunk->bytes; }

static inline u8 *chunk_end(Chunk *chunk) {
  return chunk->bytes + chunk->size * CHUNK_LEN;
}

static inline boolean chunk_contains(Chunk *chunk, u8 *addr) {
  return (chunk_begin(chunk) <= addr) && (chunk_end(chunk) > addr);
}

static inline i32 chunk_indexof(Chunk *chunk, u8 *addr) {
  assert(chunk_contains(chunk, addr));
  return (addr - chunk->bytes) / chunk->size;
}

static inline void chunk_set(Chunk *chunk, i32 index) {
  chunk->mask |= (u64)1 << index;
}

static inline void chunk_unset(Chunk *chunk, i32 index) {
  chunk->mask &= ~((u64)1 << index);
}

static inline i32 chunk_next(Chunk *chunk) { return ctz(~chunk->mask); }

static inline void *chunk_at(Chunk *chunk, i32 index) {
  return chunk->bytes + index * chunk->size;
}

static inline boolean chunk_empty(Chunk *chunk) { return chunk->mask == 0; }

static inline boolean chunk_full(Chunk *chunk) {
  return chunk->mask == UINT64_MAX;
}

static void *chunk_alloc(Chunks *s, u64 size) {
  if (s->chunks[size] == NULL) {
    chunk_create(s, size);
  }

  Chunk *chunk = s->chunks[size];

  if (chunk_full(chunk)) {
    chunk = chunk_create(s, size);
  }

  i32 index = chunk_next(chunk);

  chunk_set(chunk, index);

  return chunk_at(chunk, index);
}

void chunk_dealloc(Chunks *s, u64 size, void *ptr) {
  Chunk *chunk = s->chunks[size];

  while (chunk && !chunk_contains(chunk, ptr)) {
    chunk = chunk->next;
  }

  i32 index = chunk_indexof(chunk, ptr);

  chunk_unset(chunk, index);

  if (chunk_empty(chunk))
    chunk_destroy(s, chunk);
}

void *alloc_setup() {
  void *ud = malloc(sizeof(Chunks));
  memset(ud, 0, sizeof(Chunks));
  return ud;
}

void alloc_teardown(void *ud) { free(ud); }

void *gab_reallocate(gab_engine *gab, void *loc, u64 old_count, u64 new_count) {

  if (new_count == 0) {

    if (old_count <= CHUNK_MAX_SIZE)
      chunk_dealloc(gab_user(gab), old_count, loc);
    else
      free(loc);

    return NULL;
  }

  if (new_count <= CHUNK_MAX_SIZE) {
    return chunk_alloc(gab_user(gab), new_count);
  }

  void *new_ptr = realloc(loc, new_count);

  if (!new_ptr) {
    exit(1);
  }

  return new_ptr;
}
