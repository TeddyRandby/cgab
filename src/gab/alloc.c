#include "include/alloc.h"
#include "include/engine.h"
#include <stdint.h>
#include <unistd.h>

#define ctz __builtin_ctzl

static inline gab_chunk *chunk_create(gab_allocator *s, uint8_t size) {
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

static inline uint8_t *chunk_begin(gab_chunk *chunk) { return chunk->bytes; }

static inline uint8_t *chunk_end(gab_chunk *chunk) {
  return chunk->bytes + chunk->size * CHUNK_LEN;
}

static inline bool chunk_contains(gab_chunk *chunk, uint8_t *addr) {
  return (chunk_begin(chunk) <= addr) && (chunk_end(chunk) > addr);
}

static inline int32_t chunk_indexof(gab_chunk *chunk, uint8_t *addr) {
  assert(chunk_contains(chunk, addr));
  return (addr - chunk->bytes) / chunk->size;
}

static inline void chunk_set(gab_chunk *chunk, int32_t index) {
  chunk->mask |= (uint64_t)1 << index;
}

static inline void chunk_unset(gab_chunk *chunk, int32_t index) {
  chunk->mask &= ~((uint64_t)1 << index);
}

static inline int32_t chunk_next(gab_chunk *chunk) { return ctz(~chunk->mask); }

static inline void *chunk_at(gab_chunk *chunk, int32_t index) {
  return chunk->bytes + index * chunk->size;
}

static inline bool chunk_empty(gab_chunk *chunk) { return chunk->mask == 0; }

static inline bool chunk_full(gab_chunk *chunk) {
  return chunk->mask == UINT64_MAX;
}

static void *chunk_alloc(gab_allocator *s, uint64_t size) {
  if (s->chunks[size] == NULL) {
    chunk_create(s, size);
  }

  gab_chunk *chunk = s->chunks[size];

  if (chunk_full(chunk)) {
    chunk = chunk_create(s, size);
  }

  int32_t index = chunk_next(chunk);

  chunk_set(chunk, index);

  return chunk_at(chunk, index);
}

void chunk_dealloc(gab_allocator *s, uint64_t size, void *ptr) {
  gab_chunk *chunk = s->chunks[size];

  while (chunk && !chunk_contains(chunk, ptr)) {
    chunk = chunk->next;
  }

  assert(chunk);

  int32_t index = chunk_indexof(chunk, ptr);

  chunk_unset(chunk, index);

  if (chunk_empty(chunk))
    chunk_destroy(s, chunk);
}

void *gab_obj_alloc(struct gab_eg *gab, struct gab_obj *obj, uint64_t size) {

  if (size == 0) {
    assert(obj);

#if cGAB_CHUNK_ALLOCATOR
    uint64_t old_size = gab_obj_size(obj);

    if (old_size < CHUNK_MAX_SIZE)
      chunk_dealloc(&gab->allocator, old_size, obj);
    else
#endif
      free(obj);

    return NULL;
  }

  assert(!obj);

#if cGAB_CHUNK_ALLOCATOR
  if (size < CHUNK_MAX_SIZE) {
    return chunk_alloc(&gab->allocator, size);
  }
#endif

  return malloc(size);
}
