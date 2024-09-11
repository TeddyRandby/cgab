#include "alloc.h"

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
