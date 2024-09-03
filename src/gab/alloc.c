#include "alloc.h"
#include <stdint.h>

#define MAX_poolS 256
#define pool_MAX_SIZE 256

#define LINES_PER_POOL 64
#define OBJS_PER_LINE 64

struct pos {
  uint8_t line, idx;
};

struct pool {
  uint8_t size;

  uint64_t mask;

  uint64_t lines[LINES_PER_POOL];

  uint8_t bytes[];
};

struct gab_allocator {
  struct pool *free_pool[pool_MAX_SIZE];
  struct pool *pools[pool_MAX_SIZE];
};

static struct gab_allocator allocator;

#define ctz __builtin_ctzl

static inline struct pool *pool_create(struct gab_allocator *s, uint8_t size) {
  struct pool *self;

  if (s->free_pool[size] != nullptr) {
    self = s->free_pool[size];
    s->free_pool[size] = nullptr;
  } else {
    self =
        malloc(sizeof(struct pool) + (LINES_PER_POOL * OBJS_PER_LINE * size));
    if (self == nullptr)
      exit(1);
  }

  self->size = size;
  self->mask = 0;
  memset(self->lines, 0, sizeof(self->lines[0]) * LINES_PER_POOL);

  s->pools[size] = self;
  return self;
}

static inline void pool_destroy(struct gab_allocator *s, uint8_t size,
                                struct pool *c) {
  if (s->free_pool[size] == nullptr)
    s->free_pool[size] = c;
  else
    free(c);
}

static inline uint8_t *pool_begin(struct pool *pool) { return pool->bytes; }

static inline uint8_t *pool_end(struct pool *pool) {
  return pool->bytes + pool->size * OBJS_PER_LINE * LINES_PER_POOL;
}

static inline bool pool_contains(struct pool *pool, uint8_t *addr) {
  return (pool_begin(pool) <= addr) && (pool_end(pool) > addr);
}

static inline struct pos pool_indexof(struct pool *pool, void *addr) {
  assert(pool_contains(pool, addr));
  size_t true_idx = ((uint8_t *)addr - pool->bytes) / pool->size;
  size_t line_idx = true_idx / LINES_PER_POOL;
  size_t obj_idx = true_idx % OBJS_PER_LINE;
  assert(line_idx < 64);
  assert(obj_idx < 64);
  return (struct pos){line_idx, obj_idx};
}

static inline void pool_set(struct pool *pool, struct pos pos) {
  assert(!(pool->lines[pos.line] & ((uint64_t)1 << pos.idx)));
  pool->lines[pos.line] |= (uint64_t)1 << pos.idx;

  if (pool->lines[pos.line] == UINT64_MAX)
    pool->mask |= (uint64_t)1 << pos.line;
}

static inline void pool_unset(struct pool *pool, struct pos pos) {
  assert((pool->lines[pos.line] & ((uint64_t)1 << pos.idx)));
  pool->lines[pos.line] &= ~((uint64_t)1 << pos.idx);
  pool->mask &= ~((uint64_t)1 << pos.line);
}

static inline struct pos pool_next(struct pool *pool) {
  uint8_t line = ctz(~pool->mask);
  uint8_t pos = ctz(~pool->lines[line]);
  assert(pos < 64);
  return (struct pos){line, pos};
}

static inline void *pool_at(struct pool *pool, struct pos pos) {
  size_t offset = pos.line * OBJS_PER_LINE + pos.idx;
  void *result = pool->bytes + offset * pool->size;
  assert(pool_contains(pool, result));
  return result;
}

static inline bool pool_empty(struct pool *pool) {
  if (pool->mask != 0)
    return false;

  for (size_t i = 0; i < LINES_PER_POOL; i++) {
    if (pool->lines[i] != 0)
      return false;
  }

  return true;
}

static inline bool pool_full(struct pool *pool) {
  return pool->mask == UINT64_MAX;
}

static struct gab_obj *pool_alloc(struct gab_allocator *s, uint64_t size) {
  if (s->pools[size] == nullptr)
   pool_create(s, size);

  struct pool *pool = s->pools[size];

  if (pool_full(pool))
    pool = pool_create(s, size);

  struct pos pos = pool_next(pool);

  pool_set(pool, pos);

  struct gab_obj *ptr = pool_at(pool, pos);
  ptr->chunk = pool;

  return ptr;
}

void pool_dealloc(struct gab_allocator *s, uint64_t size, struct gab_obj *ptr) {
  struct pool *pool = ptr->chunk;

  struct pos index = pool_indexof(pool, ptr);

  pool_unset(pool, index);

  if (pool_empty(pool))
    pool_destroy(s, size, pool);
}

struct gab_obj *chunkalloc(struct gab_triple gab, struct gab_obj *obj,
                           uint64_t size) {
  if (size == 0) {
    assert(obj);

#if cGAB_CHUNK_ALLOCATOR
    uint64_t old_size = gab_obj_size(obj);

    if (old_size < pool_MAX_SIZE)
      pool_dealloc(&allocator, old_size, obj);
    else
      free(obj);
#else
    free(obj);
#endif

    return NULL;
  }

  assert(!obj);

#if cGAB_CHUNK_ALLOCATOR
  if (size < pool_MAX_SIZE) {
    return pool_alloc(&allocator, size);
  }
#endif

  return malloc(size);
}
