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
gab_engine *gab_create();

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

void gab_args(gab_engine* gab, u8 argc, s_i8 argv_names[argc], gab_value argv_values[argc]);

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
gab_module *gab_compile(gab_engine *gab, s_i8 name, s_i8 source, u8 flags);

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
gab_value gab_run(gab_engine *gab, gab_module *main, u8 flags);

/**
 * Crash the given VM with the given message
 *
 * @param  gab The engine
 *
 * @param   vm The vm to panic
 *
 * @param  msg The message to present
 *
 */
void gab_panic(gab_engine *gab, gab_vm *vm, const char *msg);

/**
 * Decrement the reference count of a gab value
 *
 * @param val The value to clean up
 */
void gab_dref(gab_engine *gab, gab_vm *vm, gab_value value);

/**
 * Increment the reference count of a gab value
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
 * Create a specialization on the given message for the given receiver
 *
 * @param gab The engine
 *
 * @param name The message
 *
 * @param receiver The receiver of the message
 *
 * @param specialization The unique handler for this receiver
 *
 * @return The message that was updated
 */
gab_value gab_specialize(gab_engine *gab, s_i8 name, gab_value receiver,
                         gab_value specialization);

/**
 * Send the message to the receiver
 *
 * @param gab The engine
 *
 * @param name The message
 *
 * @param receiver The receiver of the message
 *
 * @return The return value of the message
 */
gab_value gab_send(gab_engine *gab, s_i8 name, gab_value receiver, u8 argc,
                   gab_value argv[argc]);

/**
 * Returns true if the value is truthy
 *
 * @param self The value to check
 */
static boolean gab_val_falsey(gab_value self);

static gab_value gab_typeof(gab_engine *gab, gab_value self);

gab_value gab_get_type(gab_engine *gab, gab_type t);

#define GAB_SEND(name, receiver, size, args)                                   \
  gab_send(gab, s_i8_cstr(name), receiver, size, args)

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_RECORD(vm, size, keys, values)                                     \
  gab_bundle_record(gab, vm, size, keys, values)

/**
 * A helper macro for creating a gab_obj_record
 */
#define GAB_ARRAY(vm, size, values) gab_bundle_array(gab, vm, size, values)

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
