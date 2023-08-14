#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "include/object.h"

typedef enum gab_status gab_status;
typedef struct gab_src gab_src;

#include "alloc.h"
#include "gc.h"
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
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define NAME shapes
#define K gab_obj_shape *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define NAME messages
#define K gab_obj_message *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define T gab_value
#include "vector.h"

struct gab_eg {
  gab_mod *modules;

  gab_src *sources;

  d_gab_imp imports;

  v_gab_value scratch;

  d_strings interned_strings;

  d_shapes interned_shapes;

  d_messages interned_messages;

  gab_allocator allocator;

  gab_value types[kGAB_NKINDS];

  u64 hash_seed;

  v_gab_value argv_names;

  v_gab_value argv_values;
};

gab_obj_string *gab_eg_find_string(gab_eg *gab, s_i8 str, u64 hash);

gab_obj_message *gab_eg_find_message(gab_eg *gab, gab_value name, u64 hash);

gab_obj_shape *gab_eg_find_shape(gab_eg *gab, u64 size, u64 stride, u64 hash,
                                 gab_value keys[size]);
#endif
