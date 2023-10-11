#ifndef GAB_ALLOC_H
#define GAB_ALLOC_H

#include "include/core.h"

#define CHUNK_MAX_SIZE 256

#define CHUNK_LEN 64

struct gab_chunk;

struct gab_eg;
struct gab_obj;

struct gab_chunk {
  struct gab_chunk *prev, *next;
  uint64_t old, young;
  uint8_t size;
  uint8_t bytes[FLEXIBLE_ARRAY];
};

struct gab_allocator {
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
void *gab_obj_alloc(struct gab_eg *gab, struct gab_obj *loc, uint64_t size);

/**
 * An under-the-hood allocator for objects.
 *
 * @param gab The gab engine.
 * @param loc The object whose memory is being reallocated, or NULL.
 * @param size The size to allocate. When size is 0, the object is freed.
 * @return The allocated memory, or NULL.
 */
void gab_obj_old(struct gab_eg *gab, struct gab_obj *loc);

void gab_mem_reset(struct gab_eg *gab);

#endif
