#include "alloc.h"
#include "core.h"
#include "engine.h"
#include "gab.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define ctz __builtin_ctzl

static inline struct gab_chunk *chunk_create(struct gab_allocator *s,
                                             uint8_t size) {
  struct gab_chunk *old_chunk = s->chunks[size];
  struct gab_chunk *self =
      malloc(sizeof(struct gab_chunk) + (CHUNK_LEN * size));

  self->mask = 0;
  self->size = size;

  self->prev = NULL;
  self->next = old_chunk;

  if (old_chunk != NULL)
    old_chunk->prev = self;

  s->chunks[size] = self;
  return self;
}

static inline void chunk_destroy(struct gab_allocator *s, struct gab_chunk *c) {
  if (c->prev != NULL)
    c->prev->next = c->next;
  else
    s->chunks[c->size] = c->next;

  if (c->next != NULL)
    c->next->prev = c->prev;

  free(c);
}

static inline uint8_t *chunk_begin(struct gab_chunk *chunk) {
  return chunk->bytes;
}

static inline uint8_t *chunk_end(struct gab_chunk *chunk) {
  return chunk->bytes + chunk->size * CHUNK_LEN;
}

static inline bool chunk_contains(struct gab_chunk *chunk, uint8_t *addr) {
  return (chunk_begin(chunk) <= addr) && (chunk_end(chunk) > addr);
}

static inline size_t chunk_indexof(struct gab_chunk *chunk, uint8_t *addr) {
  assert(chunk_contains(chunk, addr));
  return (addr - chunk->bytes) / chunk->size;
}

static inline void chunk_set(struct gab_chunk *chunk, int32_t index) {
  chunk->mask |= (uint64_t)1 << index;
}

static inline void chunk_unset(struct gab_chunk *chunk, int32_t index) {
  chunk->mask &= ~((uint64_t)1 << index);
}

static inline int32_t chunk_next(struct gab_chunk *chunk) {
  return ctz(~chunk->mask);
}

static inline void *chunk_at(struct gab_chunk *chunk, int32_t index) {
  return chunk->bytes + index * chunk->size;
}

static inline bool chunk_empty(struct gab_chunk *chunk) {
  return chunk->mask == 0;
}

static inline bool chunk_full(struct gab_chunk *chunk) {
  return chunk->mask == UINT64_MAX;
}

static void *chunk_alloc(struct gab_allocator *s, uint64_t size) {
  if (s->chunks[size] == NULL) {
    chunk_create(s, size);
  }

  struct gab_chunk *chunk = s->chunks[size];

  if (chunk_full(chunk)) {
    chunk = chunk_create(s, size);
  }

  int32_t index = chunk_next(chunk);

  chunk_set(chunk, index);

  void *ptr = chunk_at(chunk, index);

  return ptr;
}

struct gab_chunk *chunk_find(struct gab_allocator *s, uint64_t size,
                             void *ptr) {
  struct gab_chunk *chunk = s->chunks[size];

  while (chunk && !chunk_contains(chunk, ptr)) {
    chunk = chunk->next;
  }

  assert(chunk);

  return chunk;
}

//TODO Seriously optimize this, and how our heap is structured.
// Linked List is way too slow.
void chunk_dealloc(struct gab_allocator *s, uint64_t size, void *ptr) {
  struct gab_chunk *chunk = chunk_find(s, size, ptr);

  size_t index = chunk_indexof(chunk, ptr);

  chunk_unset(chunk, index);

  if (chunk_empty(chunk))
    chunk_destroy(s, chunk);
}

void *gab_memalloc(struct gab_triple gab, struct gab_obj *obj, uint64_t size) {
  if (size == 0) {
    assert(obj);

#if cGAB_CHUNK_ALLOCATOR
    uint64_t old_size = gab_obj_size(obj);

    if (old_size < CHUNK_MAX_SIZE)
      chunk_dealloc(&gab.eg->allocator, old_size, obj);
    else
      free(obj);
#else
    free(obj);
#endif

    return NULL;
  }

  assert(!obj);

#if cGAB_CHUNK_ALLOCATOR
  if (size < CHUNK_MAX_SIZE) {
    return chunk_alloc(&gab.eg->allocator, size);
  }
#endif

  return malloc(size);
}
