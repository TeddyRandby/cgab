#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "compiler.h"
#include "vm.h"
#include "gab.h"

#include <pthread.h>

void *gab_reallocate(gab_engine *self, void *loc, u64 old_size, u64 new_size);

enum gab_error_k {
#define ERROR(name, message) GAB_ERROR_##name,
#include "include/error.h"
#undef ERROR
};

static const char* gab_error_names[] = {
#define ERROR(name, message) message,
#include "include/error.h"
#undef ERROR
};

enum gab_result_k {
  GAB_RESULT_OK = 0,
  GAB_RESULT_COMPILE_ERROR = -1,
  GAB_RESULT_RUN_ERROR = -2,
};

struct gab_result {
  gab_result_k k;
  gab_value result;
};

#define NAME gab_constant
#define K gab_value
#define HASH(a) (gab_val_intern_hash(a))
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

#define NAME gab_container_tag
#define K gab_value
#define V gab_obj_container_cb
#define HASH(a) (gab_val_intern_hash(a))
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "include/dict.h"

struct gab_engine {
  /*
    The constant table.
  */
  d_gab_constant *constants;

  /*
     A dictionary of container tags
     tag -> destructor
  */
  d_gab_container_tag *tags;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;

  /*
    The std lib object.
  */
  gab_value std;

  // The engine lock required in order to operate on the shared properties.
  pthread_mutex_t lock;

  // Per Engine
  gab_bc bc;
  gab_vm vm;
  gab_gc gc;
  a_i8* error;
  boolean owning;
};

boolean gab_result_ok(gab_result self);
gab_value gab_result_value(gab_result self);
#endif
