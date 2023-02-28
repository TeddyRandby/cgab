#include "include/alloc.h"
#include "include/engine.h"
#include <stdint.h>
#include <unistd.h>

#define ctz __builtin_ctzl

static inline gab_chunk *chunk_create(gab_allocator *s, u8 size) {
  gab_chunk *old_chunk = s->chunks[size];
  gab_chunk *self = malloc(sizeof(gab_chunk) + (CHUNK_LEN * size));
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

static inline void chunk_destroy(gab_allocator *s, gab_chunk *c) {
  if (c->prev == NULL)
    s->chunks[c->size] = c->next;
  else
    c->prev->next = c->next;

  if (c->next != NULL)
    c->next->prev = c->prev;

  free(c);
}

static inline u8 *chunk_begin(gab_chunk *chunk) { return chunk->bytes; }

static inline u8 *chunk_end(gab_chunk *chunk) {
  return chunk->bytes + chunk->size * CHUNK_LEN;
}

static inline boolean chunk_contains(gab_chunk *chunk, u8 *addr) {
  return (chunk_begin(chunk) <= addr) && (chunk_end(chunk) > addr);
}

static inline i32 chunk_indexof(gab_chunk *chunk, u8 *addr) {
  assert(chunk_contains(chunk, addr));
  return (addr - chunk->bytes) / chunk->size;
}

static inline void chunk_set(gab_chunk *chunk, i32 index) {
  chunk->mask |= (u64)1 << index;
}

static inline void chunk_unset(gab_chunk *chunk, i32 index) {
  chunk->mask &= ~((u64)1 << index);
}

static inline i32 chunk_next(gab_chunk *chunk) { return ctz(~chunk->mask); }

static inline void *chunk_at(gab_chunk *chunk, i32 index) {
  return chunk->bytes + index * chunk->size;
}

static inline boolean chunk_empty(gab_chunk *chunk) { return chunk->mask == 0; }

static inline boolean chunk_full(gab_chunk *chunk) {
  return chunk->mask == UINT64_MAX;
}

static void *chunk_alloc(gab_allocator *s, u64 size) {
  if (s->chunks[size] == NULL) {
    chunk_create(s, size);
  }

  gab_chunk *chunk = s->chunks[size];

  if (chunk_full(chunk)) {
    chunk = chunk_create(s, size);
  }

  i32 index = chunk_next(chunk);

  chunk_set(chunk, index);

  return chunk_at(chunk, index);
}

void chunk_dealloc(gab_allocator *s, u64 size, void *ptr) {
  gab_chunk *chunk = s->chunks[size];

  while (chunk && !chunk_contains(chunk, ptr)) {
    chunk = chunk->next;
  }

  assert(chunk);

  i32 index = chunk_indexof(chunk, ptr);

  chunk_unset(chunk, index);

  if (chunk_empty(chunk))
    chunk_destroy(s, chunk);
}

gab_allocator *gab_allocator_create() {
  void *ud = malloc(sizeof(gab_allocator));
  memset(ud, 0, sizeof(gab_allocator));
  return ud;
}

void gab_allocator_destroy(gab_allocator *chunks) { free(chunks); }

void *gab_reallocate(gab_engine *gab, void *loc, u64 old_count, u64 new_count) {

  if (new_count == 0) {

#if CHUNK_ALLOCATOR
    if (old_count <= CHUNK_MAX_SIZE)
      chunk_dealloc(gab->allocator, old_count, loc);
    else
#endif
      free(loc);

    return NULL;
  }

#if CHUNK_ALLOCATOR
  if (new_count <= CHUNK_MAX_SIZE) {
    return chunk_alloc(gab->allocator, new_count);
  }
#endif

  void *new_ptr = realloc(loc, new_count);

  if (!new_ptr) {
    exit(1);
  }

  return new_ptr;
}
