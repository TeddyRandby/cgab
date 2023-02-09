#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

typedef enum gab_status gab_status;
typedef struct gab_bc gab_bc;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;

#include "gab.h"
#include "gc.h"

void *gab_reallocate(gab_engine* gab, void *loc, u64 old_size, u64 new_size);

static const char *gab_status_names[] = {
#define STATUS(name, message) message,
#include "include/status_code.h"
#undef STATUS
};

#define NAME strings
#define K gab_obj_string *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define NAME shapes
#define K gab_obj_shape *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define NAME messages
#define K gab_obj_message *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

struct gab_engine {
  gab_module *modules;
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
  gab_value types[GAB_KIND_NKINDS];

  u64 hash_seed;

  void* userdata;

  a_u64 *argv_values;
  a_s_i8 *argv_names;
};


gab_obj_string *gab_engine_find_string(gab_engine *gab, s_i8 str, u64 hash);
gab_obj_message *gab_engine_find_message(gab_engine *gab, s_i8 name, u64 hash);
gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]);

i32 gab_engine_intern(gab_engine *gab, gab_value value);
#endif
