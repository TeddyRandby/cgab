#ifndef GAB_H
#define GAB_H

#include "object.h"

typedef struct gab_engine gab_engine;
typedef struct gab_module gab_module;
typedef struct gab_vm gab_vm;

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
// Deref all referenced objects
void gab_cleanup(gab_engine *gab);

/**
 * Cleanup a Gab Module.
 *
 * @param mod The module to clean up
 */
void gab_module_destroy(gab_module *mod);
// Deref all referenced objects
void gab_module_cleanup(gab_engine *gab, gab_module *mod);

/**
 * Compile a source string into a Gab Module.
 *
 * @param gab The engine
 *
 * @param source The source code
 *
 * @param name A name for the package
 *
 * @return The gab_obj_closure on a success, and GAB_VAL_NULL on error.
 */
gab_module *gab_compile(gab_engine *gab, s_i8 name, s_i8 source, u8 narguments,
                        s_i8 args[narguments]);

/**
 * Run a module in the gab vm.
 *
 * @param  gab The engine
 *
 * @param   vm The vm to run the module on
 *
 * @param main The module to run
 *
 * @param argc The number of arguments to the closure
 *
 * @param argv The array of arguments to pass to the main closure
 *
 * @return The return value of the closure
 */
gab_value gab_run(gab_engine *gab, gab_module *main, u8 argc,
                  gab_value argv[argc]);

void gab_panic(gab_engine* gab, gab_vm* vm, const char* msg);

/**
 * Decrement the RC of a gab value
 *
 * @param val The value to clean up
 */
void gab_dref(gab_engine *gab, gab_vm *vm, gab_value value);

/**
 * Increment the RC of a gab value
 *
 * @param val The value to clean up
 */
void gab_iref(gab_engine *gab, gab_vm *vm, gab_value value);

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
gab_value gab_bundle_record(gab_engine *gab, gab_vm *vm, u64 size,
                            s_i8 keys[size], gab_value values[size]);

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
gab_value gab_bundle_array(gab_engine *gab, gab_vm *vm, u64 size,
                           gab_value values[size]);

/**
 *
 */
gab_value gab_specialize(gab_engine *gab, s_i8 name, gab_value receiver,
                    gab_value specialization);

static boolean gab_val_falsey(gab_value self);

static gab_value gab_typeof(gab_engine *gab, gab_value self);

gab_value gab_get_type(gab_engine *gab, gab_type t);

gab_value gab_result_ok(gab_engine* gab, gab_vm* vm, gab_value val);

gab_value gab_result_err(gab_engine* gab, gab_vm* vm, gab_value err);

gab_value gab_option_some(gab_engine* gab, gab_vm* vm, gab_value some);

gab_value gab_option_none(gab_engine* gab, gab_vm* vm);

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_RECORD(vm, size, keys, values)                                     \
  gab_bundle_record(gab, vm, size, keys, values)

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_ARRAY(vm, size, values)                                            \
  gab_bundle_array(gab, vm, size, values)

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

/**
 *
 */
#define GAB_SYMBOL(name) GAB_VAL_OBJ(gab_obj_symbol_create(s_i8_cstr(name)))
#endif
