#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "../vm/vm.h"
#include "compiler.h"

#if GAB_LOG_GC
#include <stdio.h>
#endif

void *gab_reallocate(gab_engine *self, void *loc, u64 old_size, u64 new_size);

/*
  The result type returned by the compiler and vm.
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
      gab_compiler *compiler;
      const char *msg;
    } compile_fail;

    struct {
      gab_vm *vm;
      const char *msg;
    } run_fail;
  } as;
} gab_result;

gab_result *gab_compile_fail(gab_compiler *self, const char *msg);

gab_result *gab_run_fail(gab_vm *self, const char *msg);

gab_result *gab_compile_success(gab_obj_closure *main);

gab_result *gab_run_success(gab_value data);

boolean gab_result_has_error(gab_result *self);

void gab_result_dump_error(gab_result *self);

void gab_result_destroy(gab_result *self);

typedef enum gab_import_kind {
  IMPORT_SHARED,
  IMPORT_SOURCE,
} gab_import_kind;

typedef struct gab_import gab_import;
struct gab_import {
  gab_import_kind k;
  union {
    void *shared;
    s_u8 *source;
  };
  gab_value cache;
};

gab_import *gab_import_shared(void *, gab_value);
gab_import *gab_import_source(s_u8 *, gab_value);

void gab_import_destroy(gab_import *);

typedef struct gab_engine gab_engine;
struct gab_engine {
  // These properties are shared with forked engines.
  /*
    The constant table.
  */
  d_u64 *constants;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;

  /*
     A dictionary of imports

     import_name -> cached value
  */
  d_u64 *imports;

  /*
    The std lib object.
  */
  gab_value std;

  // These properties are unique per engine.
  /*
    A pointer to the vm running this module.
  */
  gab_vm vm;

  /*
    The gargabe collector for the engine
  */
  gab_gc gc;
};

gab_obj_string *gab_engine_find_string(gab_engine *, s_u8_ref, u64);

gab_obj_shape *gab_engine_find_shape(gab_engine *, u64, gab_value values[], u64,
                                     u64);

u16 gab_engine_add_constant(gab_engine *, gab_value);

gab_module *gab_engine_add_module(gab_engine *, s_u8_ref, s_u8_ref);

void gab_engine_add_import(gab_engine *, gab_import *, s_u8_ref);

void gab_engine_add_import();

void gab_engine_collect(gab_engine *eng);

/*
  Compile a single source string into a single contiguous module.

  DEFINED IN COMPILER/COMPILER.H
*/
gab_result *gab_engine_compile(gab_engine *, s_u8_ref, s_u8_ref, u8);

/*
  Run a gab closure.

  DEFINED IN VM/VM.H
*/
gab_result *gab_engine_run(gab_engine *, gab_obj_closure *);

static inline void gab_engine_obj_iref(gab_engine *self, gab_obj *obj) {
  ASSERT_TRUE(obj != NULL, "Don't try to gc null");
  self->gc.increments[self->gc.increment_count++] = obj;
  if (self->gc.increment_count == INC_DEC_MAX) {
    gab_engine_collect(self);
  }
}

static inline void gab_engine_obj_dref(gab_engine *self, gab_obj *obj) {
  ASSERT_TRUE(obj != NULL, "Don't try to gc null");
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
