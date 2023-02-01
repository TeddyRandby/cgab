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

#define NAME strings
#define K gab_obj_string*
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define NAME shapes
#define K gab_obj_shape*
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define NAME messages
#define K gab_obj_message*
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

struct gab_engine {
  /*
   * Where all the interned values live.
   */
  d_strings interned_strings;
  d_shapes interned_shapes;
  d_messages interned_messages;

  /*
   * The GC for the vm
   */
  gab_gc gc;

  /*
   * The Engine's builtin types
   */
  gab_value types[GAB_NTYPES];

  u64 hash_seed;

  u8 argc;
  gab_value *argv_values;
  s_i8 *argv_names;
};

// This can be heavily optimized.
static inline gab_value gab_typeof(gab_engine *gab, gab_value value) {
  // The type of a record is it's shape
  if (GAB_VAL_IS_RECORD(value)) {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return GAB_VAL_OBJ(obj->shape);
  }

  // The type of null or symbols is themselves
  if (GAB_VAL_IS_NIL(value) || GAB_VAL_IS_UNDEFINED(value) || GAB_VAL_IS_SYMBOL(value)) {
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
gab_obj_message* gab_engine_find_message(gab_engine *gab, s_i8 name, u64 hash);
gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]);

i32 gab_engine_intern(gab_engine *gab, gab_value value);
#endif
