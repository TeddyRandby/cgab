#ifndef GAB_H
#define GAB_H

#include "object.h"

typedef struct gab_engine gab_engine;

typedef struct gab_bc gab_bc;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;

typedef enum gab_error_k gab_error_k;

typedef enum gab_result_k gab_result_k;
typedef struct gab_result gab_result;

typedef enum gab_import_k gab_import_k;
typedef struct gab_import gab_import;

/**
 * Create a Gab Engine. If you want libraries included, build and bind them
 * before running any code.
 *
 * @param error An output parameter for the engine to write errors into.
 *
 * @return The allocated Gab Engine.
 */
gab_engine *gab_create(a_i8* error);

/**
 * Create a Gab Engine, forking from a parent engine.
 *
 * @return The allocated Gab Engine.
 */
gab_engine *gab_fork(gab_engine *gab);

/**
 * Cleanup a Gab Engine.
 *
 * @param gab: The engine to clean up.
 */
void gab_destroy(gab_engine *gab);

/**
 * Bundle a list of KVPS into a Gab object.
 *
 * @param gab The engine
 *
 * @param size The length of keys and values arrays
 *
 * @param keys The keys of the gab object to bind.
 *
 * @param values The values of the gab object to bind.
 *
 * @return The gab value that the keys and values were bundled into
 */
gab_value gab_bundle(gab_engine *gab, u64 size, s_i8 keys[size],
                     gab_value values[size]);

/**
 * Bundle the keys and values into a gab object and bind it to the engine.
 *
 * @param gab The engine to bind to
 *
 * @param size The length of keys and values arrays
 *
 * @param keys The keys of the gab object to bind.
 *
 * @param values The values of the gab object to bind.
 */
void gab_bind(gab_engine *gab, u64 size, s_i8 keys[size],
              gab_value values[size]);

/**
 * Compile and run a c-str of gab code.
 *
 * @param gab The engine to run the code with.
 *
 * @param source The source code.
 *
 * @param module_name Give a name to the module that will be compiled from the
 * source.
 *
 * @param flags The set of flags to pass to the compiler and vm.
 *
 * @return A gab result object.
 */
gab_result gab_run(gab_engine *gab, s_i8 module_name, s_i8 source, u8 flags);

i32 gab_lock(gab_engine *gab);

i32 gab_unlock(gab_engine *gab);

gab_obj_string *gab_find_string(gab_engine *gab, s_i8 string, u64 hash);

gab_obj_shape *gab_find_shape(gab_engine *gab, u64 size, u64 stride, u64 has,
                              gab_value values[size]);

u16 gab_add_constant(gab_engine *gab, gab_value constant);

gab_module *gab_add_module(gab_engine *gab, s_i8 name, s_i8 source);

void gab_add_import(gab_engine *gab, s_i8 name, gab_import *import);

void gab_add_container_tag(gab_engine *gab, gab_value tag,
                           gab_obj_container_cb destructor);

void gab_collect(gab_engine *gab);

void gab_iref(gab_engine *gab, gab_value obj);

void gab_dref(gab_engine *gab, gab_value obj);

#endif
