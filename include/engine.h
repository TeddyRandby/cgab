#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

typedef enum gab_status gab_status;
typedef struct gab_bc gab_bc;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;

#include "compiler.h"
#include "vm.h"
#include "gab.h"

#include <threads.h>

void *gab_reallocate(void *loc, u64 old_size, u64 new_size);

static const char* gab_status_names[] = {
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

#define T gab_vm
#include "include/vector.h"

struct gab_engine {
  /*
   * Where all the interned values live.
   */
  d_gab_intern interned;

  /*
   * A vector of spawned vms
   */
  v_gab_vm vms;

  /*
   * Optional Flags
   */
  u8 flags;
};

gab_obj_string *gab_engine_find_string(gab_engine *gab, s_i8 string, u64 hash);

gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride, u64 hash,
                              gab_value keys[size]);

u16 gab_engine_intern(gab_engine *gab, gab_value value);
#endif
