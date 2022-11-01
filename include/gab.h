#ifndef GAB_H
#define GAB_H

#include "object.h"

typedef struct gab_engine gab_engine;

/**
 * Create a Gab Engine. If you want libraries included, build and bind them
 * before running any code.
 *
 * @return The allocated Gab Engine.
 */
gab_engine *gab_create(u8 flags);

/**
 * Cleanup a Gab Engine.
 *
 * @param gab The engine to clean up.
 */
void gab_destroy(gab_engine *gab);

/**
 * Compile a source string into a gab_obj_closure.
 *
 * @param gab The engine
 *
 * @param source The source code
 *
 * @param name A name for the package
 *
 * @return The gab_obj_closure on a success, and GAB_VAL_NULL on error.
 */
gab_value gab_compile(gab_engine *gab, s_i8 name, s_i8 source);

/**
 * Compile a source string into a gab_obj_closure.
 *
 * @param gab The engine
 *
 * @param source The source code
 *
 * @return The gab_obj_closure on a success, and GAB_VAL_NULL on error.
 */
gab_value gab_compile_main(gab_engine *gab, s_i8 source);

/*
 * Spawn a new VM to run gab closures.
 *
 * @param gab The engine
 *
 * @return The new vm
 *
 */
i32 gab_spawn(gab_engine* gab);

/**
 * Run a gab_obj_closure in the gab vm.
 *
 * @param gab The engine
 *
 * @param main The gab_obj_closure to call
 *
 * @param argc The number of arguments to the closure
 *
 * @param argv The array of arguments to pass to the main closure
 *
 * @return The return value of the closure
 */
gab_value gab_run(gab_engine *gab, i32 vm, gab_value main, u8 argc,
                  gab_value argv[argc]);

/**
 * Run a gab_obj_closure in the gab vm. This will implicitly pass
 * the globals record as the single argument to the function.
 *
 * @param gab The engine
 *
 * @param main The gab_obj_closure to call
 *
 * @return The return value of the closure
 */
gab_value gab_run_main(gab_engine* gab, i32 vm, gab_value main, gab_value globals);

/**
 * Decrement the RC of a gab value
 *
 * @param val The value to clean up
 */
void gab_dref(gab_engine *gab, i32 vm, gab_value value);

/**
 * Increment the RC of a gab value
 *
 * @param val The value to clean up
 */
void gab_iref(gab_engine *gab, i32 vm, gab_value value);

/**
 * Bundle a list of keys and values into a Gab object.
 *
 * @param gab The engine
 *
 * @param size The length of keys and values arrays
 *
 * @param keys The keys of the gab object to bundle.
 *
 * @param values The values of the gab object to bundle.
 *
 * @return The gab value that the keys and values were bundled into
 */
gab_value gab_bundle_record(gab_engine *gab, u64 size, s_i8 keys[size],
                            gab_value values[size]);

/**
 * Bundle a list of values into a Gab object.
 *
 * @param gab The engine
 *
 * @param size The length of keys and values arrays
 *
 * @param values The values of the gab object to bind.
 *
 * @return The gab value that the keys and values were bundled into
 */
gab_value gab_bundle_array(gab_engine *gab, u64 size, gab_value values[size]);

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_RECORD(size, keys, values)                                         \
  GAB_VAL_OBJ(gab_bundle_record(gab, size, keys, values))

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_ARRAY(size, values) GAB_VAL_OBJ(gab_bundle_array(gab, size, values))

/**
 * A helper macro for creating a gab_obj_builtin
 */
#define GAB_BUILTIN(name)                                                      \
  GAB_VAL_OBJ(gab_obj_builtin_create(gab_lib_##name, s_i8_cstr(#name)))

/**
 * A helper macro for creating a gab_obj_string
 */
#define GAB_STRING(cstr)                                                       \
  GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(cstr)))

/**
 * A helper macro for creating a gab_obj_container
 */
#define GAB_CONTAINER(cb, data) GAB_VAL_OBJ(gab_obj_container_create(cb, data))
#endif
