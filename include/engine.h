#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

typedef enum gab_status gab_status;
typedef struct gab_bc gab_bc;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;

#include "compiler.h"
#include "gab.h"
#include "vm.h"

#include <threads.h>

void *gab_reallocate(void *loc, u64 old_size, u64 new_size);

static const char *gab_status_names[] = {
#define STATUS(name, message) message,
#include "include/status_code.h"
#undef STATUS
};

#define NAME gab_intern
#define K gab_value
#define HASH(a) (gab_val_intern_hash(a))
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

struct gab_engine {
  /*
   * Where all the interned values live.
   */
  d_gab_intern interned;

  /*
   * The GC for the vm
   */
  gab_gc gc;

  /*
   * The Engine's builtin types
   */
  gab_value types[GAB_NTYPES];

  /*
   * Optional Flags
   */
  u8 flags;
};

// This can be heavily optimized.
static inline gab_value gab_val_type(gab_engine *gab, gab_value value) {
  // The type of a record is it's shape
  if (GAB_VAL_IS_RECORD(value)) {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return GAB_VAL_OBJ(obj->shape);
  }

  // The type of null or symbols is themselves
  if (GAB_VAL_IS_NULL(value) || GAB_VAL_IS_SYMBOL(value)) {
      return value;
  }

  if (GAB_VAL_IS_NUMBER(value)) {
    return gab->types[TYPE_NUMBER];
  }

  if (GAB_VAL_IS_BOOLEAN(value)) {
    return gab->types[TYPE_BOOLEAN];
  }

  return gab->types[GAB_VAL_TO_OBJ(value)->kind];
}

gab_obj_string *gab_engine_find_string(gab_engine *gab, s_i8 str, u64 hash);
gab_obj_function*gab_engine_find_function(gab_engine *gab, s_i8 name, u64 hash);

gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]);

u16 gab_engine_intern(gab_engine *gab, gab_value value);
#endif
