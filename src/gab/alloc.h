#ifndef GAB_ALLOC_H
#define GAB_ALLOC_H

#include "gab.h"

/**
 * An under-the-hood allocator for objects.
 *
 * @param gab The gab engine.
 * @param loc The object whose memory is being reallocated, or NULL.
 * @param size The size to allocate. When size is 0, the object is freed.
 * @return The allocated memory, or NULL.
 */
struct gab_obj *chunkalloc(struct gab_triple gab, struct gab_obj *loc, size_t size);

#endif
