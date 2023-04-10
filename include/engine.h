#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

typedef enum gab_status gab_status;

#include "include/object.h"
typedef struct gab_source gab_source;
typedef struct gab_bc gab_bc;
typedef struct gab_vm_frame gab_vm_frame;

#include "gc.h"
#include "alloc.h"
#include "import.h"

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
  gab_obj *objects;

  gab_module* modules;

  gab_source* sources;

  d_gab_import imports;

  v_gab_value scratch;

  /*
   * Where all the interned values live.
   */
  d_strings interned_strings;

  d_shapes interned_shapes;

  d_messages interned_messages;

  /*
   * A simple allocator
   */
  gab_allocator allocator;

  /*
   * The Engine's builtin types
   */
  gab_value types[GAB_KIND_NKINDS];

  u64 hash_seed;

  v_gab_value argv_names;

  v_gab_value argv_values;
};

gab_obj_string *gab_engine_find_string(gab_engine *gab, s_i8 str, u64 hash);

gab_obj_message *gab_engine_find_message(gab_engine *gab, gab_value name,
                                         u64 hash);

gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]);

i32 gab_engine_intern(gab_engine *gab, gab_value value);

void gab_engine_collect(gab_engine* gab);
#endif
