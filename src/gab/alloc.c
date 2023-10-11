#include "include/alloc.h"
#include "include/engine.h"
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
  self->old = 0;
  self->young = 0;
  self->size = size;

  self->prev = NULL;
  self->next = old_chunk;

  if (old_chunk != NULL) {
    old_chunk->prev = self;
  }

  s->chunks[size] = self;
  return self;
}

static inline void chunk_destroy(struct gab_allocator *s, struct gab_chunk *c) {
  if (c->prev == NULL)
    s->chunks[c->size] = c->next;
  else
    c->prev->next = c->next;

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

static inline void chunk_setyng(struct gab_chunk *chunk, int32_t index) {
  chunk->young |= (uint64_t)1 << index;
}

static inline void chunk_setold(struct gab_chunk *chunk, int32_t index) {
  chunk->old |= (uint64_t)1 << index;
}

static inline void chunk_unsetold(struct gab_chunk *chunk, int32_t index) {
  chunk->old &= ~((uint64_t)1 << index);
}

static inline int32_t chunk_next(struct gab_chunk *chunk) {
  return ctz(~(chunk->old | chunk->young));
}

static inline void *chunk_at(struct gab_chunk *chunk, int32_t index) {
  return chunk->bytes + index * chunk->size;
}

static inline bool chunk_empty(struct gab_chunk *chunk) {
  return (chunk->old | chunk->young) == 0;
}

static inline bool chunk_full(struct gab_chunk *chunk) {
  return (chunk->old | chunk->young) == UINT64_MAX;
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

  chunk_setyng(chunk, index);

  return chunk_at(chunk, index);
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

void chunk_dealloc(struct gab_allocator *s, uint64_t size, void *ptr) {
  struct gab_chunk *chunk = chunk_find(s, size, ptr);

  size_t index = chunk_indexof(chunk, ptr);

  chunk_unsetold(chunk, index);

  if (chunk_empty(chunk))
    chunk_destroy(s, chunk);
}

void gab_obj_old(struct gab_eg *gab, struct gab_obj *obj) {
  uint64_t size = gab_obj_size(obj);

  struct gab_chunk *chunk = chunk_find(&gab->allocator, size, obj);

  size_t index = chunk_indexof(chunk, (void *)obj);

  chunk_setold(chunk, index);
}

void gab_mem_reset(struct gab_eg *gab) {
  for (int i = 0; i < CHUNK_MAX_SIZE; i++) {
    struct gab_chunk *chunk = gab->allocator.chunks[i], *old = NULL;
    while (chunk) {
      chunk->young = 0;
      
      old = chunk;
      chunk = chunk->next;

      if (chunk_empty(old))
        chunk_destroy(&gab->allocator, old);
    }
  }
}

void *gab_obj_alloc(struct gab_eg *gab, struct gab_obj *obj, uint64_t size) {
  if (size == 0) {
    assert(obj);

    uint64_t old_size = gab_obj_size(obj);

    if (old_size < CHUNK_MAX_SIZE)
      chunk_dealloc(&gab->allocator, old_size, obj);
    else
      free(obj);

    return NULL;
  }

  assert(!obj);

  if (size < CHUNK_MAX_SIZE) {
    return chunk_alloc(&gab->allocator, size);
  }

  fprintf(stderr, "Unsupported size %lu", size);
  exit(1);
}
