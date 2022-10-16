#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

typedef enum gab_status gab_status;
typedef struct gab_bc gab_bc;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;

#include "compiler.h"
#include "vm.h"
#include "gab.h"

#include <pthread.h>

void *gab_reallocate(gab_engine *self, void *loc, u64 old_size, u64 new_size);

enum gab_status {
#define STATUS(name, message) GAB_##name,
#include "include/status_code.h"
#undef STATUS
};

static const char* gab_status_names[] = {
#define STATUS(name, message) message,
#include "include/status_code.h"
#undef STATUS
};

#define NAME gab_constant
#define K gab_value
#define HASH(a) (gab_val_intern_hash(a))
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define GAB_ENGINE_ERROR_LEN 4096
struct gab_engine {
  /////////////////////////////////////////
  // SHARED FIELDS
  /////////////////////////////////////////
  /*
    The constant table.
  */
  d_gab_constant *constants;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;

  /*
    The std lib object.
  */
  gab_value std;
  /////////////////////////////////////////
  boolean owning;

  // The engine lock required in order to operate on the shared properties.
  pthread_mutex_t lock;

  gab_status status;
  i8 err_msg[GAB_ENGINE_ERROR_LEN];

  gab_bc bc;
  gab_vm vm;
  gab_gc gc;
};

gab_obj_string *gab_engine_find_string(gab_engine *gab, s_i8 string, u64 hash);

gab_obj_shape *gab_engine_find_shape(gab_engine *gab, u64 size, u64 stride, u64 hash,
                              gab_value keys[size]);

u16 gab_engine_add_constant(gab_engine *gab, gab_value constant);

gab_module *gab_engine_add_module(gab_engine *gab, s_i8 name, s_i8 source);

u64 gab_engine_write_error(gab_engine* gab, u64 offset, const char* fmt, ...);
#endif
