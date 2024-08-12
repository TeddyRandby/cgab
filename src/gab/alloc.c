#include "alloc.h"
#include <stdint.h>

#define MAX_CHUNKS 256
#define CHUNK_MAX_SIZE 256

#define CHUNKS_PER_PAGE 64
#define OBJS_PER_PAGE 64

struct page {
  uint8_t size;

  uint64_t mask;

  uint8_t bytes[FLEXIBLE_ARRAY];
};

struct gab_allocator {
  uint8_t npages[MAX_CHUNKS];
  struct page *newest_pages[CHUNK_MAX_SIZE];
  struct page *all_pages[MAX_CHUNKS][CHUNK_MAX_SIZE];
};

static struct gab_allocator allocator;

#define ctz __builtin_ctzl

static inline struct page *page_create(struct gab_allocator *s, uint8_t size) {
  struct page *self = malloc(sizeof(struct page) + (OBJS_PER_PAGE * size));

  self->size = size;

  self->mask = 0;

  s->newest_pages[size] = self;
  s->npages[size]++;
  return self;
}

static inline void page_destroy(struct gab_allocator *s, struct page *c) {
  uint8_t last_page = s->npages[c->size]--;

  if (last_page < UINT8_MAX)
    s->newest_pages[c->size] = s->all_pages[c->size][last_page];

  free(c);
}

static inline uint8_t *page_begin(struct page *chunk) { return chunk->bytes; }

static inline uint8_t *page_end(struct page *chunk) {
  return chunk->bytes + chunk->size * OBJS_PER_PAGE;
}

static inline bool page_contains(struct page *chunk, uint8_t *addr) {
  return (page_begin(chunk) <= addr) && (page_end(chunk) > addr);
}

static inline uint8_t page_indexof(struct page *chunk, void *addr) {
  assert(page_contains(chunk, addr));
  size_t idx = ((uint8_t *)addr - chunk->bytes) / chunk->size;
  assert(idx < 64);
  return idx;
}

static inline void page_set(struct page *chunk, uint8_t pos) {
  assert(!(chunk->mask & ((uint64_t)1 << pos)));
  chunk->mask |= (uint64_t)1 << pos;
}

static inline void page_unset(struct page *chunk, uint8_t pos) {
  assert((chunk->mask & ((uint64_t)1 << pos)));
  chunk->mask &= ~((uint64_t)1 << pos);
}

static inline uint8_t page_next(struct page *chunk) {
  uint8_t pos = ctz(~chunk->mask);
  assert(pos < 64);
  return pos;
}

static inline void *page_at(struct page *chunk, uint8_t pos) {
  size_t offset = pos * chunk->size;
  void *result = chunk->bytes + offset;
  assert(page_contains(chunk, result));
  return result;
}

static inline bool page_mostlyempty(struct page *page) {
  return __builtin_popcountl(page->mask) < 32;
}

static inline bool page_empty(struct page *page) {
  return page->mask == 0;
}

static inline bool chunk_full(struct page *chunk) {
  return chunk->mask == UINT64_MAX;
}

static struct gab_obj *page_alloc(struct gab_allocator *s, uint64_t size) {
  if (s->newest_pages[size] == nullptr)
    page_create(s, size);

  struct page *page = s->newest_pages[size];

  if (chunk_full(page))
    page = page_create(s, size);

  uint8_t pos = page_next(page);

  page_set(page, pos);

  struct gab_obj *ptr = page_at(page, pos);
  ptr->chunk = page;

  return ptr;
}

void page_dealloc(struct gab_allocator *s, uint64_t size, struct gab_obj *ptr) {
  struct page *chunk = ptr->chunk;

  uint8_t index = page_indexof(chunk, ptr);

  page_unset(chunk, index);

  if (page_empty(chunk))
    page_destroy(s, chunk);
  else if (page_mostlyempty(chunk))
    s->newest_pages[size] = chunk;
}

struct gab_obj *chunkalloc(struct gab_triple gab, struct gab_obj *obj,
                           uint64_t size) {
  if (size == 0) {
    assert(obj);

#if cGAB_CHUNK_ALLOCATOR
    uint64_t old_size = gab_obj_size(obj);

    if (old_size < CHUNK_MAX_SIZE)
      page_dealloc(&allocator, old_size, obj);
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
    return page_alloc(&allocator, size);
  }
#endif

  return malloc(size);
}
