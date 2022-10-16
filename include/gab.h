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
gab_engine *gab_create();

/**
 * Create a Gab Engine, forking from a parent engine.
 *
 * @return The allocated Gab Engine.
 */
gab_engine *gab_fork(gab_engine *gab);

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
gab_value gab_package_source(gab_engine* gab, s_i8 name, s_i8 source, u8 flags);

/**
 * Run a gab_obj_closure in the gab vm.
 *
 * @param gab The engine
 *
 * @param main The gab_obj_closure to call
 *
 * @return The return value of the closure
 */
gab_value gab_run(gab_engine* gab, gab_value main);

/**
 * Returns false if the engine has encountered an error.
 *
 * @param gab The engine
 *
 * @return state of the engine
 */
boolean gab_ok(gab_engine* gab);

/**
 * Returns a null-terminated 
 */
i8* gab_err(gab_engine* gab);

/**
 * Bundle a list of keys and values into a Gab object.
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
#define GAB_ARRAY(size, values)                                         \
  GAB_VAL_OBJ(gab_bundle_array(gab, size, values))

/**
 * A helper macro for creating a gab_obj_builtin
 */
#define GAB_BUILTIN(name, arity)                                               \
  GAB_VAL_OBJ(                                                                 \
      gab_obj_builtin_create(gab, gab_lib_##name, s_i8_cstr(#name), arity))

/**
 * A helper macro for creating a gab_obj_string
 */
#define GAB_STRING(cstr) \
    GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(cstr)))

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

/*
 * Manually trigger a garbage collection.
 *
 * @param gab The engine to collect.
 */
void gab_collect(gab_engine *gab);

/*
 * Increment the RC on the value
 *
 * @param gab The engine.
 *
 * @param obj The value.
 */
void gab_iref(gab_engine *gab, gab_value val);

/*
 * Decrement the RC on the value
 *
 * @param gab The engine.
 *
 * @param obj The value.
 */
void gab_dref(gab_engine *gab, gab_value val);
#endif
