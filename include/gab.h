#ifndef GAB_H
#define GAB_H

#include "object.h"
#include <printf.h>

typedef struct gab_engine gab_engine;
typedef struct gab_module gab_module;

/**
 * Create a Gab Engine. If you want libraries included, build and bind them
 *  before running any code.
 *
 * @return The allocated Gab Engine.
 */
gab_engine *gab_create();

/**
 * Cleanup a Gab Engine.
 *
 * This function calls gab_collect internally,
 *  so no need to call it manually.
 *
 * @param gab The engine to clean up.
 */
void gab_destroy(gab_engine *gab);

/**
 * Set the arguments that the engine will pass to
 *  the modules it compiles or runs. The arrays are shallow copied.
 *
 *  @param gab The engine
 *
 *  @param argc The number of arguments
 *
 *  @param argv_names The names of each argument (Passed to the compiler)
 *
 *  @param argv_values The values of each argument (Passed to the vm)
 */
void gab_args(gab_engine *gab, u8 argc, gab_value argv_names[argc],
              gab_value argv_values[argc]);

/**
 * Pass the ownership of a value to the engine.
 *
 *  @param gab The engine
 *
 *  @param value The value
 *
 *  @return The value that was added to scratch
 */
gab_value gab_scratch(gab_engine* gab, gab_value value);

/**
 * Push a value(s) onto the vm's stack (ie. as return values from a builtin)
 *
 * @param vm The vm that will receive the values
 *
 * @param argc The number of values
 *
 * @param argv The array of values
 */
i32 gab_push(gab_vm* vm, u8 argc, gab_value argv[argc]);

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
gab_value gab_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags);

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
gab_value gab_run(gab_engine *gab, gab_value main, u8 flags);

/**
 * Crash the given VM with the given message
 *
 * @param  gab The engine
 *
 * @param   vm The vm to panic
 *
 * @param  msg The message to present
 *
 * @return A gab value wrapping the state of the panic'd vm
 *
 */
gab_value gab_panic(gab_engine *gab, gab_vm *vm, const char *msg);

/**
 * Disassemble a module from start to end.
 *
 * @param mod The module the bytecode is in
 *
 * @param offset The beginning of the bytecode to dump
 *
 * @param len The amount of bytecode to dump
 */
void gab_dis(gab_module *mod);

/**
 * Pry into a vm for the frame at the given depth in the callstack.
 *
 * @param gab The engine
 *
 * @param vm The vm
 *
 * @param depth The depth
 */
void gab_pry(gab_engine* gab, gab_vm* vm, u64 depth);

/**
 * Deep-copy a value into the given gab engine.
 *
 * @param gab The engine
 *
 * @param value The value to copy
 *
 * @return The copied value
 *
 */
gab_value gab_val_copy(gab_engine *gab, gab_vm *vm, gab_value value);

/**
 * Decrement the reference count of a value
 *
 * @param val The value to clean up. If there is no VM, you may pass NULL.
 */
void gab_val_dref(gab_vm *vm, gab_value value);
void gab_val_dref_many(gab_vm *vm, u64 len, gab_value values[len]);

/**
 * Increment the reference count of a value
 *
 * @param val The value to clean up. If there is no VM, you may pass NULL.
 */
void gab_val_iref(gab_vm *vm, gab_value value);
void gab_val_iref_many(gab_vm *vm, u64 len, gab_value values[len]);

/**
 * Trigger a garbace collection.
 *
 * @param gab The engine to collect in
 *
 * @param vm The value to clean up. If there is no VM, you may pass NULL.
 */
void gab_collect(gab_engine *gab, gab_vm *vm);

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
gab_value gab_record(gab_engine *gab, gab_vm *vm, u64 size, s_i8 keys[size],
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
gab_value gab_tuple(gab_engine *gab, gab_vm *vm, u64 size,
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
gab_value gab_specialize(gab_engine *gab, gab_vm *vm, gab_value name,
                         gab_value receiver, gab_value specialization);

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
gab_value gab_send(gab_engine *gab, gab_vm *vm, gab_value message,
                   gab_value receiver, u8 argc, gab_value argv[argc]);

/**
 * Dump a gab value to stdout
 *
 * @param self The value to dump
 */
i32 gab_val_dump(FILE *stream, gab_value self);

int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args);

int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes);

/*
 * Get the value that corresponds to a given type.
 *
 * @param gab The engine
 *
 * @param kind The type to retrieve the value for.
 *
 * @return The gab value corresponding to that type.
 */
gab_value gab_type(gab_engine *gab, gab_kind kind);

static inline gab_kind gab_val_kind(gab_value value) {
  if (GAB_VAL_IS_NUMBER(value))
    return GAB_KIND_NUMBER;
  if (GAB_VAL_IS_NIL(value))
    return GAB_KIND_NIL;
  if (GAB_VAL_IS_UNDEFINED(value))
    return GAB_KIND_UNDEFINED;
  if (GAB_VAL_IS_BOOLEAN(value))
    return GAB_KIND_BOOLEAN;
  if (GAB_VAL_IS_OBJ(value))
    return GAB_VAL_TO_OBJ(value)->kind;

  return GAB_KIND_PRIMITIVE;
}

/**
 * Get the type of a gab value. This function is used internally to send
 * messages.
 *
 * @param gab The engine
 *
 * @param self The value
 *
 * @return The type of the value
 */
// This can be heavily optimized.
static inline gab_value gab_val_type(gab_engine *gab, gab_value value) {
  gab_kind k = gab_val_kind(value);

  switch (k) {
  case GAB_KIND_NIL:
  case GAB_KIND_UNDEFINED:
  case GAB_KIND_SYMBOL:
  case GAB_KIND_SHAPE:
    return value;

  case GAB_KIND_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return GAB_VAL_OBJ(obj->shape);
  }

  case GAB_KIND_CONTAINER: {
    gab_obj_container *con = GAB_VAL_TO_CONTAINER(value);
    return con->type;
  }
  default:
    return gab_type(gab, k);
  }
}

/**
 * Convert a gab value to a boolean.
 *
 * @param self The value to check
 *
 * @return False if the value is false or nil. Otherwise true.
 */
static inline boolean gab_val_falsey(gab_value self) {
  return GAB_VAL_IS_NIL(self) || GAB_VAL_IS_FALSE(self);
}

/**
 * Convert a gab value to a gab string
 *
 * @param gab The engine
 *
 * @param self The value to convert
 */
gab_value gab_val_to_string(gab_engine *gab, gab_value self);

/**
 * A helper macro for sending simply
 */
#define GAB_SEND(name, receiver, argc, argv)                                   \
  gab_send(gab, vm, GAB_STRING(name), receiver, argc, argv)

/**
 * A helper macro for creating a gab_obj_builtin
 */
#define GAB_BUILTIN(name)                                                      \
  GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_##name, GAB_STRING(#name)))

/**
 * A helper macro for creating a gab_obj_string
 */
#define GAB_STRING(cstr)                                                       \
  GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(cstr)))

/**
 * A helper macro for creating a gab_obj_container
 */
#define GAB_CONTAINER(type, cb, data)                                              \
  GAB_VAL_OBJ(gab_obj_container_create(gab, type, cb, data))

/**
 * A helper macro for creating a gab_obj_symbol
 */
#define GAB_SYMBOL(name)                                                       \
  GAB_VAL_OBJ(gab_obj_symbol_create(gab, GAB_STRING(name)))
#endif
