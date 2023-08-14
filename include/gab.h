#ifndef GAB_H
#define GAB_H

#include <printf.h>
#include <stdint.h>

#include "include/value.h"

typedef struct gab_mod gab_mod;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;
typedef struct gab_eg gab_eg;
typedef struct gab_obj gab_obj;
typedef void (*gab_gcvisit_f)(gab_gc *gc, gab_obj *obj);
typedef void (*gab_builtin_f)(gab_eg *gab, gab_vm *vm, size_t argc,
                              gab_value argv[argc]);
typedef void (*gab_boxdestroy_f)(void *data);
typedef void (*gab_boxvisit_f)(gab_gc *gc, gab_gcvisit_f visitor, void *data);

/**
 * # Create a gab_eg.
 * This struct stores data about the environment that gab
 * code executes in.
 *
 *  @param argc The number of arguments.
 *
 *  @param argv_names The names of each argument.
 *
 *  @param argv_values The values of each argument.
 *
 * @return A pointer to the struct on the heap.
 */
gab_eg *gab_create(size_t argc, gab_value argv_names[argc],
                   gab_value argv_values[argc]);

/**
 * # Destroy a gab_eg.
 * Free all memory associated with the engine.
 *
 * @param gab The engine.
 */
void gab_destroy(gab_eg *gab);

/**
 * # Get a string corresponding to the last error encountered.
 *
 * @param gab The engine.
 *
 * @return A string describing the error.
 */
const char *gab_strerr(gab_eg *gab);

/**
 * # Set the value of the argument at index to value.
 *
 *  @param gab The engine.
 *
 *  @param value The values of the argument.
 *
 *  @param index The index of the argument to change.
 */
void gab_argput(gab_eg *gab, gab_value value, size_t index);

/**
 * # Push an additional argument onto the engine's argument list.
 *
 *  @param gab The engine.
 *
 *  @param name The name of the argument (Passed to the compiler).
 *
 *  @param value The values of the argument (Passed to the vm).
 */
size_t gab_argpush(gab_eg *gab, gab_value name);

/**
 * # Pop an argument off the engine's argument list.
 *
 *  @param gab The engine.
 */
void gab_argpop(gab_eg *gab);

/**
 * # Give the engine ownership of the values.
 *
 *  When in c-code, it can be useful to have gab_values outlive the function
 * they are created in, but if they aren't referenced by another value they will
 * be collected. Thus, the scratch buffer is useful to hold a reference to the
 * value until the engine is destroyed.
 *
 *  @param gab The engine.
 *
 *  @return The value.
 */
size_t gab_egkeep(gab_eg *gab, gab_value v);

/**
 * # Give the engine ownership of the values.
 *
 *  When in c-code, it can be useful to have gab_values outlive the function
 * they are created in, but if they aren't referenced by another value they will
 * be collected. Thus, the scratch buffer is useful to hold a reference to the
 * value until the engine is destroyed.
 *
 *  @param gab The engine.
 *
 *  @param len The number of values to scratch.
 *
 *  @param values The values.
 *
 *  @return The number of values pushed.
 */
size_t gab_negkeep(gab_eg *gab, size_t len, gab_value argv[static len]);

/**
 * # Get the GC of a vm
 *
 * @param vm The vm.
 *
 * @return The gc.
 */
gab_gc *gab_vmgc(gab_vm *vm);

/**
 * # Push a value(s) onto the vm's internal stack
 *
 * This is used to return values from c-builtins.
 *
 * @param vm The vm that will receive the values.
 *
 * @return The number of values pushed.
 */
size_t gab_vmpush(gab_vm *vm, gab_value v);

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
size_t gab_nvmpush(gab_vm *vm, size_t len, gab_value argv[static len]);

/**
 * # Execute a gab_block
 *
 * @param  gab The engine.
 *
 * @param main The block to run
 *
 * @param flags Options for the compiler.
 *
 * @return A heap-allocated slice of values returned by the block.
 */
a_gab_value *gab_run(gab_eg *gab, gab_value main, size_t flags);

/**
 * # Compile and run a source string
 *
 * @param gab The engine.
 *
 * @param name The name to give the module.
 *
 * @param source The source code.
 *
 * @param flags Options for the compiler.
 *
 * @return A heap-allocated slice of values returned by the block.
 */
a_gab_value *gab_execute(gab_eg *gab, const char *name, const char *source,
                         size_t flags);

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
a_gab_value *gab_send(gab_eg *gab, gab_vm *vm, gab_value message,
                      gab_value receiver, size_t len, gab_value argv[len]);

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
gab_value gab_panic(gab_eg *gab, gab_vm *vm, const char *msg);

/**
 * # Trigger a garbage collection.
 *
 * @param gab The engine to reclaim garbage from.
 *
 * @param vm The vm to find garbage in. This may be NULL.
 */
void gab_collect(gab_eg *gab, gab_vm *vm);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
void gab_gciref(gab_gc *gc, gab_vm *vm, gab_value value);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_ngciref(gab_gc *gc, gab_vm *vm, size_t len, gab_value values[len]);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
void gab_gcdref(gab_gc *gc, gab_vm *vm, gab_value value);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_ngcdref(gab_gc *gc, gab_vm *vm, size_t len, gab_value values[len]);

/**
 * # Create a gab_value from a c-string
 *
 * @param gab The engine.
 *
 * @param data The data.
 *
 * @return The value.
 */
gab_value gab_string(gab_eg *gab, const char *data);

/**
 * # Create a gab_value from a bounded c-string
 *
 * @param gab The engine.
 *
 * @param len The length of the string.
 *
 * @param data The data.
 *
 * @return The value.
 */
gab_value gab_nstring(gab_eg *gab, size_t len, const char *data);

gab_value gab_builtin(gab_eg *gab, const char *name, gab_builtin_f f);

struct gab_box_argt {
  gab_value type;
  gab_boxdestroy_f destructor;
  gab_boxvisit_f visitor;
  void *data;
};
/**
 * # Create a gab value which wraps some user data.
 *
 * @param gab The engine.
 *
 * @param args The arguments.
 * @see struct gab_box_argt
 *
 * @return The value.

 */
gab_value gab_box(gab_eg *gab, gab_vm *vm, struct gab_box_argt args);

/**
 * # Get the user data from a boxed value.
 *
 * @param value The value.
 *
 * @return The user data.
 */
void *gab_boxdata(gab_value value);

struct gab_block_argt {
  const char *name;
  const char *source;
  size_t flags;
};
/**
 * # Compile a source string into a block.
 *
 * @see enum gab_flags
 *
 * @param gab The engine.
 *
 * @param args The arguments.
 * @see struct gab_block_argt
 *
 * @return The block.
 */
gab_value gab_block(gab_eg *gab, struct gab_block_argt args);

/**
 * # Create a nil-initialized (empty) record of shape shape.
 *
 * @param gab The engine.
 *
 * @param shape The shape of the record.
 */
gab_value gab_erecord(gab_eg *gab, gab_value shape);

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
gab_value gab_record(gab_eg *gab, gab_vm *vm, size_t len,
                     const char *keys[static len],
                     gab_value values[static len]);

/**
 * # Get the shape of a record.
 *
 * @param value The record.
 *
 * @return The shape of the record.
 */
gab_value gab_recshp(gab_value value);

/**
 * # Get the length of a record.
 *
 * @param value The record.
 *
 * @return The length of the record.
 */
size_t gab_reclen(gab_value value);

/**
 * # Get a pointer to the members of a record.
 *
 * @param value The record.
 *
 * @return The members of the record.
 *
 */
gab_value *gab_recdata(gab_value value);

/**
 * # Returns true if the record has the given key.
 *
 * @param value The record.
 * @param key The key.
 *
 * @return True if the record has the given key.

 */
bool gab_rechas(gab_value value, gab_value key);

/**
 * # Returns true if the record has the given key.
 *
 * @param value The record.
 * @param key The key.
 *
 * @return True if the record has the given key.
 */
bool gab_srechas(gab_eg *gab, gab_value value, const char *key);

/**
 * # Get the value of a key in a record.
 *
 * @param value The record.
 *
 * @param key The key.
 *
 * @return The value of the key.
 */
gab_value gab_recat(gab_value value, gab_value key);

/**
 * # Get the value of a key in a record.
 *
 * @param value The record.
 *
 * @param key The key.
 *
 * @return The value of the key.
 */
gab_value gab_srecat(gab_eg *gab, gab_value value, const char *key);

/**
 * # Set the value of a key in a record.
 *
 * @param vm The vm.
 *
 * @param value The record.
 *
 * @param key The key.
 *
 * @param val The value.
 */
void gab_recput(gab_vm *vm, gab_value value, gab_value key, gab_value val);

/**
 * # Set the value of a key in a record.
 *
 * @param vm The vm.
 *
 * @param value The record.
 *
 * @param key The key.
 *
 * @param val The value.
 */
void gab_srecput(gab_eg *gab, gab_vm *vm, gab_value value, const char *key,
                 gab_value val);

/**
 * # Get the length of a shape.
 *
 * @param value The shape.
 *
 * @return The length of the shape.
 */
size_t gab_shplen(gab_value value);

/**
 * # Get a pointer to the keys of a shape
 *
 * @param value The shape.
 *
 * @return The keys
 */
gab_value *gab_shpdata(gab_value shp);

/**
 * # Find the index of a key in a shape.
 *
 * @param value The shape.
 *
 * @param needle The key.
 */
size_t gab_shpfind(gab_value shp, gab_value needle);

/**
 * # Find the index of a spec for a message
 */
size_t gab_msgfind(gab_value msg, gab_value needle);

/**
 * # Get the length of a string
 *
 */
size_t gab_strlen(gab_value str);

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
gab_value gab_tuple(gab_eg *gab, gab_vm *vm, size_t len,
                    gab_value values[static len]);

struct gab_spec_argt {
  const char *name;
  gab_value receiver, specialization;
};
/**
 * # Create a specialization on the given message for the given receiver
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param args The arguments.
 * @see struct gab_spec_argt
 *
 * @return The message that was updated.
 */
gab_value gab_spec(gab_eg *gab, gab_vm *vm, struct gab_spec_argt args);

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
gab_value gab_valcpy(gab_eg *gab, gab_vm *vm, gab_value value);

/**
 * # Get the runtime value that corresponds to the given kind.
 *
 * @param gab The engine
 *
 * @param kind The type to retrieve the value for.
 *
 * @return The runtime value corresponding to that type.
 */
gab_value gab_typ(gab_eg *gab, gab_kind kind);

/**
 * # Get the kind of a value.
 * @see enum gab_kind
 * *NOTE* This is not the **runtime type** of the value. For that, use
 * `gab_valtyp`.
 *
 * @param value The value.
 *
 * @return The kind of the value.
 *
 */
gab_kind gab_valknd(gab_value value);

/**
 * # Get the **runtime type** of a gab value.
 *
 * *NOTE* This function is used internally to determine which specialization to
 * dispatch to when sending messages.
 *
 * @param gab The engine
 *
 * @param value The value
 *
 * @return The runtime value corresponding to the type of the given value
 */
gab_value gab_valtyp(gab_eg *gab, gab_value value);

/**
 * # Coerce a value to a boolean.
 *
 * @param self The value to check.
 *
 * @return False if the value is false or nil. Otherwise true.
 */
bool gab_valtob(gab_value value);
/**
 * # Coerce the given value to a gab string.
 *
 * @param gab The engine
 *
 * @param self The value to convert
 *
 * @return The string representation of the value.
 */
gab_value gab_valtos(gab_eg *gab, gab_value value);

/**
 * # Coerce the given value to a string, and return a mallocd copy.
 * *NOTE*: The caller is responsible for feeing the returned string.
 *
 *  @param gab The engine
 *
 *  @param value The value to convert.
 *
 *  @return An allocated c-string.
 */
char *gab_valtocs(gab_eg *gab, gab_value value);

/**
 * # builtin printf handlers so that you can easily debug with gab values.
 */
int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args);
int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes);

/**
 * # Dump a gab value to a file stream.
 *
 * @param stream The file stream to dump to.
 *
 * @param self The value to dump
 *
 * @return The number of bytes written.
 */
i32 gab_fdump(FILE *stream, gab_value self);

/**
 * # Disassemble a module.
 *
 * This is useful for debugging bytecode.
 *
 * @param mod The module.
 */
void gab_fdis(FILE *stream, gab_mod *mod);

/**
 * # Pry into the frame at the given depth in the callstack.
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param depth The depth.
 */
void gab_fpry(FILE *stream, gab_vm *vm, u64 depth);
#endif
