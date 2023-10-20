#ifndef GAB_ALLOC_H
#define GAB_ALLOC_H

#include "include/gab.h"

#define CHUNK_MAX_SIZE 256

#define CHUNK_LEN 64

struct gab_chunk;

struct gab_chunk {
  struct gab_chunk *prev, *next;
  uint64_t old, young;
  uint8_t size;
  uint8_t bytes[FLEXIBLE_ARRAY];
};

struct gab_allocator {
  size_t heap_size;
  struct gab_chunk *chunks[CHUNK_MAX_SIZE];
};

/**
 * An under-the-hood allocator for objects.
 *
 * @param gab The gab engine.
 * @param loc The object whose memory is being reallocated, or NULL.
 * @param size The size to allocate. When size is 0, the object is freed.
 * @return The allocated memory, or NULL.
 */
void *gab_memalloc(struct gab_triple gab, struct gab_obj *loc, uint64_t size);

/**
 * Mark a memory location as *old*. This assures that it will not be collected
 * by gab_memclean.
 *
 * @param gab The gab engine.
 * @param loc The object whose memory is being reallocated, or NULL.
 */
void gab_memold(struct gab_eg *eg, struct gab_obj *loc);

/**
 * Mark a memory location as *old*. This assures that it will not be collected
 * by gab_memclean.
 *
 * @param gab The gab engine.
 */
void gab_memclean(struct gab_eg *eg);

#endif
