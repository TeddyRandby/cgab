#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H
#include "compiler.h"

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

boolean gab_result_has_error(gab_result *self);

void gab_result_dump_error(gab_result *self);

gab_result *gab_compile_fail(gab_compiler *self, const char *msg);

gab_result *gab_run_fail(gab_vm *self, const char *msg);

gab_result *gab_compile_success(gab_obj_closure *main);

gab_result *gab_run_success(gab_value data);

void gab_result_destroy(gab_result *self);

typedef struct gab_engine gab_engine;

struct gab_engine {
  /*
    The constant table.
  */
  d_u64 constants;

  /*
    The std lib object.
  */
  gab_value std;

  /*
    A pointer to the vm running this module.
  */
  gab_vm *vm;

  /*
    The gargabe collector for the engine
  */
  gab_gc *gc;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;
};

gab_engine *gab_engine_create();
void gab_engine_destroy(gab_engine *self);

gab_obj_string *gab_engine_find_string(gab_engine *self, s_u8_ref str,
                                       u64 hash);
gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size,
                                     gab_value values[size], u64 stride,
                                     u64 hash);

u16 gab_engine_add_constant(gab_engine *self, gab_value value);

gab_module *gab_engine_create_module(gab_engine *self, s_u8 *src,
                                     s_u8_ref name);

/*
  Compile a single source string into a single contiguous module.

  DEFINED IN COMPILER/COMPILER.H
*/
gab_result *gab_engine_compile(gab_engine *eng, s_u8 *src, s_u8_ref name,
                               u8 compile_flags);
/*
  Run a gab function.

  A value of 0 means an error was encountered.

  DEFINED IN VM/VM.H
*/
gab_result *gab_engine_run(gab_engine *eng, gab_vm *vm, gab_obj_closure *main);

#endif
