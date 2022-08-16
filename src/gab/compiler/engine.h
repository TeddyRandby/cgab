#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "../vm/vm.h"
#include "compiler.h"

#include <pthread.h>

#if GAB_LOG_GC
#include <stdio.h>
#endif

void *gab_reallocate(gab_engine *self, void *loc, u64 old_size, u64 new_size);

/*
   -------------========= RESULTS =========-------------
*/
typedef enum gab_result_kind {
  RESULT_COMPILE_FAIL,
  RESULT_COMPILE_SUCCESS,
  RESULT_RUN_FAIL,
  RESULT_RUN_SUCCESS,
} gab_result_kind;

typedef struct gab_result {
  gab_result_kind type;
  union {
    /* Successful compile */
    gab_obj_closure *main;
    /* Successful run */
    gab_value result;

    struct {
      gab_bc *compiler;
      const char *msg;
    } compile_fail;

    struct {
      gab_vm *vm;
      const char *msg;
    } run_fail;
  } as;
} gab_result;

gab_result *gab_compile_fail(gab_bc *self, const char *msg);

gab_result *gab_run_fail(gab_vm *self, const char *msg);

gab_result *gab_compile_success(gab_obj_closure *main);

gab_result *gab_run_success(gab_value data);

boolean gab_result_has_error(gab_result *self);

void gab_result_dump_error(gab_result *self);

void gab_result_destroy(gab_result *self);

/*
   -------------========= IMPORTS =========-------------
*/
typedef enum gab_import_kind {
  IMPORT_SHARED,
  IMPORT_SOURCE,
} gab_import_kind;

typedef struct gab_import gab_import;
struct gab_import {
  gab_import_kind k;
  union {
    void *shared;
    a_i8 *source;
  };
  gab_value cache;
};

gab_import *gab_import_shared(void *, gab_value);
gab_import *gab_import_source(a_i8 *, gab_value);

void gab_import_destroy(gab_import *);

#define K gab_value
#define HASH(a) (gab_val_intern_hash(a))
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "../../core/dict.h"

typedef struct gab_engine gab_engine;
struct gab_engine {
  /*
    The constant table.
  */
  d_gab_value *constants;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;

  /*
     A dictionary of imports

     import_name -> cached value
  */
  d_s_i8 *imports;

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
  boolean owning;
};

i32 gab_engine_lock(gab_engine*);

i32 gab_engine_unlock(gab_engine*);

gab_obj_string *gab_engine_find_string(gab_engine *, s_i8, u64);

gab_obj_shape *gab_engine_find_shape(gab_engine *, u64, gab_value values[], u64,
                                     u64);

u16 gab_engine_add_constant(gab_engine *, gab_value);

gab_module *gab_engine_add_module(gab_engine *, s_i8, s_i8);

void gab_engine_add_import(gab_engine *, gab_import *, s_i8);

void gab_engine_add_import();

void gab_engine_collect(gab_engine *eng);

/*
  Compile a single source string into a single contiguous module.

  DEFINED IN COMPILER/COMPILER.H
*/
gab_result *gab_engine_compile(gab_engine *, s_i8, s_i8, u8);

/*
  Run a gab closure.

  DEFINED IN VM/VM.H
*/
gab_result *gab_engine_run(gab_engine *, gab_obj_closure *);

static inline void gab_engine_obj_iref(gab_engine *self, gab_obj *obj) {
  self->gc.increments[self->gc.increment_count++] = obj;
  if (self->gc.increment_count == INC_DEC_MAX) {
    gab_engine_collect(self);
  }
}

static inline void gab_engine_obj_dref(gab_engine *self, gab_obj *obj) {
  if (self->gc.decrement_count == INC_DEC_MAX) {
    gab_engine_collect(self);
  }
  self->gc.decrements[self->gc.decrement_count++] = obj;
}

static inline void gab_engine_val_iref(gab_engine *self, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_engine_obj_iref(self, GAB_VAL_TO_OBJ(obj));
}

static inline void gab_engine_val_dref(gab_engine *self, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_engine_obj_dref(self, GAB_VAL_TO_OBJ(obj));
}

#endif
