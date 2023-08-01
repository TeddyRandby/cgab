#ifndef GAB_H
#define GAB_H

#include "object.h"
#include <printf.h>
#include <stdio.h>

/**
 * # Create a gab_engine.
 * This struct stores data about the environment that gab
 * code executes in.
 *
 * @return A pointer to the struct on the heap.
 */
gab_engine *gab_create(void);

/**
 * # Destroy a gab_engine.
 * Free all memory associated with the engine.
 *
 * @param gab The engine.
 */
void gab_destroy(gab_engine *gab);

/**
 * # Trigger a garbage collection.
 *
 * @param gab The engine to reclaim garbage from.
 *
 * @param vm The vm to find garbage in. This may be NULL.
 */
void gab_collect(gab_engine *gab, gab_vm *vm);

/**
 * # Set the arguments that the engine will pass to
 *  the modules it compiles and blocks it runs. The arguments are shallow
 *  copied.
 *
 *  @param gab The engine.
 *
 *  @param argc The number of arguments.
 *
 *  @param argv_names The names of each argument.
 *
 *  @param argv_values The values of each argument.
 */
void gab_args(gab_engine *gab, u8 argc, gab_value argv_names[static argc],
              gab_value argv_values[static argc]);

/**
 * # Set the value of the argument at index to value.
 *
 *  @param gab The engine.
 *
 *  @param value The values of the argument.
 *
 *  @param index The index of the argument to change.
 */
void gab_args_put(gab_engine *gab, gab_value value, u64 index);

/**
 * # Push an additional argument onto the engine's argument list.
 *
 *  @param gab The engine.
 *
 *  @param name The name of the argument (Passed to the compiler).
 *
 *  @param value The values of the argument (Passed to the vm).
 */
u64 gab_args_push(gab_engine *gab, gab_value name);

/**
 * # Pop an argument off the engine's argument list.
 *
 *  @param gab The engine.
 */
void gab_args_pop(gab_engine *gab);

/**
 * # Give the engine ownership of the value.
 *
 *  When in c-code, it can be useful to have gab_values outlive the function
 * they are created in, but if they aren't referenced by another value they will
 * be collected. Thus, the scratch buffer is useful to hold a reference to the
 * value until the engine is destroyed.
 *
 *  @param gab The engine.
 *
 *  @param value The value.
 *
 *  @return The value.
 */
gab_value gab_scratch(gab_engine *gab, gab_value value);

/**
 * # Give the engine ownership of the value.
 *
 *  When in c-code, it can be useful to have gab_values outlive the function
 * they are created in, but if they aren't referenced by another value they will
 * be collected. Thus, the scratch buffer is useful to hold a reference to the
 * value until the engine is destroyed.
 *
 *  @param gab The engine.
 *
 *  @param len The number of values to scratch..
 *
 *  @param values The values.
 *
 *  @return The number of values pushed.
 */
i32 gab_vscratch(gab_engine *gab, u64 len, gab_value values[static len]);

/**
 * # Push a value(s) onto the vm's internal stack
 *
 * This is used to return values from c-builtins.
 *
 * @param vm The vm that will receive the values.
 *
 * @param value The value.
 */
gab_value gab_push(gab_vm *vm, gab_value value);

/**
 * # Push a value(s) onto the vm's internal stack
 *
 * This is used to return values from c-builtins.
 *
 * @param vm The vm that will receive the values.
 *
 * @param argc The number of values.
 *
 * @param argv The array of values.
 */
i32 gab_vpush(gab_vm *vm, u64 len, gab_value argv[static len]);

/**
 * # Compile a source string into a block.
 *
 * @see enum gab_flags
 *
 * @param gab The engine.
 *
 * @param name The name to give the module.
 *
 * @param source The source code.
 *
 * @param flags Options for the compiler.
 *
 * @return The block on a success, and GAB_VAL_UNDEFINED on error.
 */
gab_value gab_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags);

/**
 * # Execute a gab_block
 *
 * @param  gab The engine.
 *
 * @param main The block to run
 *
 * @return A heap-allocated slice of values returned by the block.
 */
a_gab_value *gab_run(gab_engine *gab, gab_value main, u8 flags);

/**
 * # Panic the VM with an error message. Useful for builtin functions.
 *
 * This is useful in builtins when an unrecoverable error occurs.
 *
 * @param  gab The engine.
 *
 * @param   vm The vm.
 *
 * @param  msg The message to display.
 *
 * @return A gab value wrapping the error.
 */
gab_value gab_panic(gab_engine *gab, gab_vm *vm, const char *msg);

/**
 * # Disassemble a module.
 *
 * This is useful for debugging bytecode.
 *
 * @param mod The module.
 */
void gab_dis(gab_module *mod);

/**
 * # Pry into the frame at the given depth in the callstack.
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param depth The depth.
 */
void gab_pry(gab_vm *vm, u64 depth);

/**
 * # Decrement the reference count of a value
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_val_dref(gab_vm *vm, gab_value value);

/**
 * # Decrement the reference count of many values
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
void gab_val_vdref(gab_vm *vm, u64 len, gab_value values[static len]);

/**
 * # Increment the reference count of a value
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_val_iref(gab_vm *vm, gab_value value);

/**
 * # Increment the reference count of a many values
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
void gab_val_viref(gab_vm *vm, u64 len, gab_value values[static len]);

/**
 * # Bundle a list of keys and values into a record.
 *
 * @param gab The engine.
 *
 * @param vm The vm. This can be NULL.
 *
 * @param len The length of keys and values arrays
 *
 * @param keys The keys of the record to bundle.
 *
 * @param values The values of the record to bundle.
 *
 * @return The new record.
 */
gab_value gab_record(gab_engine *gab, gab_vm *vm, u64 len,
                     s_i8 keys[static len], gab_value values[static len]);

/**
 * # Bundle a list of values into a tuple.
 *
 * @param gab The engine
 *
 * @param vm The vm. This can be NULL.
 *
 * @param len The length of values array.
 *
 * @param values The values of the record to bundle.
 *
 * @return The new record.
 */
gab_value gab_tuple(gab_engine *gab, gab_vm *vm, u64 len,
                    gab_value values[static len]);

/**
 * # Create a specialization on the given message for the given receiver
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param name The name of the message.
 *
 * @param receiver The receiver of the specialization.
 *
 * @param specialization The specialization.
 *
 * @return The message that was updated.
 */
gab_value gab_specialize(gab_engine *gab, gab_vm *vm, gab_value name,
                         gab_value receiver, gab_value specialization);

/**
 * # Send the message to the receiver.
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param name The name of the message to send.
 *
 * @param receiver The receiver.
 *
 * @param len The number of arguments.
 *
 * @param argv The arguments.
 *
 * @return A heap-allocated slice of values returned by the message.
 */
a_gab_value *gab_send(gab_engine *gab, gab_vm *vm, gab_value message,
                      gab_value receiver, u8 len, gab_value argv[len]);

/**
 * # Deep-copy a value.
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param value The value to copy.
 *
 * @return The copy.
 */
gab_value gab_val_copy(gab_engine *gab, gab_vm *vm, gab_value value);

/**
 * # Dump a gab value to a file stream.
 *
 * @param stream The file stream to dump to.
 *
 * @param self The value to dump
 *
 * @return The number of bytes written.
 */
i32 gab_val_dump(FILE *stream, gab_value self);

/**
 * # builtin printf handlers so that you can easily debug with gab values.
 */
int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args);
int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes);

/**
 * # Get the value that corresponds to a given type.
 *
 * @param gab The engine
 *
 * @param kind The type to retrieve the value for.
 *
 * @return The runtime value corresponding to that type.
 */
gab_value gab_type(gab_engine *gab, gab_kind kind);

/**
 * Get the kind that corresponds to a value.
 *
 * @param value The value.
 *
 * @return The kind of the value.
 */
static inline gab_kind gab_val_kind(gab_value value) {
  if (GAB_VAL_IS_NUMBER(value))
    return kGAB_NUMBER;

  if (GAB_VAL_IS_NIL(value))
    return kGAB_NIL;

  if (GAB_VAL_IS_UNDEFINED(value))
    return kGAB_UNDEFINED;

  if (GAB_VAL_IS_BOOLEAN(value))
    return kGAB_BOOLEAN;

  if (GAB_VAL_IS_OBJ(value))
    return GAB_VAL_TO_OBJ(value)->kind;

  return kGAB_PRIMITIVE;
}

/**
 * # Get the type of a gab value. This function is used internally to send
 * messages.
 *
 * @param gab The engine
 *
 * @param value The value
 *
 * @return The runtime value corresponding to the type of the given value
 */
static inline gab_value gab_val_type(gab_engine *gab, gab_value value) {
  gab_kind k = gab_val_kind(value);

  switch (k) {
  case kGAB_NIL:
  case kGAB_UNDEFINED:
    return value;

  case kGAB_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return GAB_VAL_OBJ(obj->shape);
  }

  case kGAB_CONTAINER: {
    gab_obj_container *con = GAB_VAL_TO_CONTAINER(value);
    return con->type;
  }
  default:
    return gab_type(gab, k);
  }
}

/**
 * # Check if a value is falsey.
 *
 * @param self The value to check.
 *
 * @return False if the value is false or nil. Otherwise true.
 */
static inline boolean gab_val_falsey(gab_value self) {
  return GAB_VAL_IS_NIL(self) || GAB_VAL_IS_FALSE(self);
}

/**
 * # Convert a value to a string
 *
 * @param gab The engine
 *
 * @param self The value to convert
 *
 * @return The string representation of the value.
 */
gab_value gab_val_to_s(gab_engine *gab, gab_value self);

#define GAB_SEND(name, receiver, len, argv)                                    \
  gab_send(gab, vm, GAB_STRING(name), receiver, len, argv)

#define GAB_BUILTIN(name)                                                      \
  GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_##name, GAB_STRING(#name)))

#define GAB_STRING(cstr)                                                       \
  GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(cstr)))

#define GAB_CONTAINER(type, des, vis, data)                                    \
  GAB_VAL_OBJ(gab_obj_container_create(gab, vm, type, des, vis, data))

#endif
