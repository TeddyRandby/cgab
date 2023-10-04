#ifndef GAB_H
#define GAB_H

#include <printf.h>
#include <stdint.h>
#include <stdio.h>

#include "core.h"

/**
 An IEEE 754 double-precision float is a 64-bit value with bits laid out like:

 1 Sign bit
 |   11 Exponent bits
 |   |          52 Mantissa
 |   |          |
 [S][Exponent---][Mantissa------------------------------------------]

 The details of how these are used to represent numbers aren't really
 relevant here as long we don't interfere with them. The important bit is NaN.

 An IEEE double can represent a few magical values like NaN ("not a number"),
 Infinity, and -Infinity. A NaN is any value where all exponent bits are set:

     NaN bits
     |
 [-][11111111111][----------------------------------------------------]

 The bits set above are the only relevant ones. The rest of the bits are unused.

 NaN values come in two flavors: "signalling" and "quiet". The former are
 intended to halt execution, while the latter just flow through arithmetic
 operations silently. We want the latter.

 Quiet NaNs are indicated by setting the highest mantissa bit:

                Highest mantissa bit
                |
 [-][....NaN....][1---------------------------------------------------]

 This leaves the rest of the following bits to play with.

 Pointers to objects with data on the heap set the highest bit.

 We are left with 51 bits of mantissa to store an address.
 That's more than enough room for a 32-bit address. Even 64-bit machines
 only actually use 48 bits for addresses.

  Pointer bit set    Pointer data
  |                  |
 [1][....NaN....][1][---------------------------------------------------]

 Immediate values store a tag in the lowest three bits.

 Some immediates (like primitive messages, for example) store data.

                     Immediate data                                    Tag
                     |                                                 |
 [0][....NaN....][1][------------------------------------------------][---]
*/

typedef uint64_t gab_value;

#define T gab_value
#include "array.h"

typedef enum gab_kind {
  kGAB_NAN = 0,
  kGAB_UNDEFINED = 1,
  kGAB_NIL = 2,
  kGAB_FALSE = 3,
  kGAB_TRUE,
  kGAB_PRIMITIVE,
  kGAB_NUMBER,
  kGAB_SUSPENSE,
  kGAB_STRING,
  kGAB_MESSAGE,
  kGAB_BLOCK_PROTO,
  kGAB_SUSPENSE_PROTO,
  kGAB_BUILTIN,
  kGAB_BLOCK,
  kGAB_BOX,
  kGAB_RECORD,
  kGAB_SHAPE,
  kGAB_NKINDS,
} gab_kind;

#define __GAB_QNAN ((uint64_t)0x7ffc000000000000)

#define __GAB_SIGN_BIT ((uint64_t)1 << 63)

#define __GAB_TAGMASK (7)

#define __GAB_VAL_TAG(val) ((uint64_t)((val)&__GAB_TAGMASK))

static inline double __gab_valtod(gab_value value) {
  return *(double *)(&value);
}

static inline gab_value __gab_dtoval(double value) {
  return *(gab_value *)(&value);
}

#define __gab_valisn(val) (((val)&__GAB_QNAN) != __GAB_QNAN)

#define __gab_obj(val)                                                         \
  (gab_value)(__GAB_SIGN_BIT | __GAB_QNAN | (uint64_t)(uintptr_t)(val))

#define gab_valiso(val)                                                        \
  (((val) & (__GAB_QNAN | __GAB_SIGN_BIT)) == (__GAB_QNAN | __GAB_SIGN_BIT))

/* The gab value 'nil'*/
#define gab_nil ((gab_value)(uint64_t)(__GAB_QNAN | kGAB_NIL))

/* The gab value 'false'*/
#define gab_false ((gab_value)(uint64_t)(__GAB_QNAN | kGAB_FALSE))

/* The gab value 'true'*/
#define gab_true ((gab_value)(uint64_t)(__GAB_QNAN | kGAB_TRUE))

/* The gab value 'undefined'*/
#define gab_undefined ((gab_value)(uint64_t)(__GAB_QNAN | kGAB_UNDEFINED))

/* Convert a bool into the corresponding gab value */
#define gab_bool(val) (val ? gab_true : gab_false)

/* Convert a double into a gab value */
#define gab_number(val) (__gab_dtoval(val))

/* Create the gab value for a primitive operation */
#define gab_primitive(op)                                                      \
  ((gab_value)(kGAB_PRIMITIVE | __GAB_QNAN | ((uint64_t)op << 8)))

/* Cast a gab value to a number */
#define gab_valton(val) (__gab_valtod(val))

/* Cast a gab value to the generic object pointer */
#define gab_valtoo(val)                                                        \
  ((struct gab_obj *)(uintptr_t)((val) & ~(__GAB_SIGN_BIT | __GAB_QNAN)))

/* Cast a gab value to a primitive operation */
#define gab_valtop(val) ((uint8_t)((val >> 8) & 0xff))

/*
 * Gab uses a purely RC garbage collection approach.
 *
 * The algorithm is described in this paper:
 * https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon03Pure.pdf
 */
#define fGAB_OBJ_BUFFERED (1 << 0)
#define fGAB_OBJ_BLACK (1 << 1)
#define fGAB_OBJ_GRAY (1 << 2)
#define fGAB_OBJ_WHITE (1 << 3)
#define fGAB_OBJ_PURPLE (1 << 4)
#define fGAB_OBJ_GREEN (1 << 5)
#define fGAB_OBJ_GARBAGE (1 << 6)
#define fGAB_OBJ_FREED (1 << 7) // Used for debug purposes

#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & fGAB_OBJ_BUFFERED)
#define GAB_OBJ_IS_BLACK(obj) ((obj)->flags & fGAB_OBJ_BLACK)
#define GAB_OBJ_IS_GRAY(obj) ((obj)->flags & fGAB_OBJ_GRAY)
#define GAB_OBJ_IS_WHITE(obj) ((obj)->flags & fGAB_OBJ_WHITE)
#define GAB_OBJ_IS_PURPLE(obj) ((obj)->flags & fGAB_OBJ_PURPLE)
#define GAB_OBJ_IS_GREEN(obj) ((obj)->flags & fGAB_OBJ_GREEN)
#define GAB_OBJ_IS_GARBAGE(obj) ((obj)->flags & fGAB_OBJ_GARBAGE)
#define GAB_OBJ_IS_FREED(obj) ((obj)->flags & fGAB_OBJ_FREED)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags |= fGAB_OBJ_BUFFERED)

#define GAB_OBJ_GARBAGE(obj) ((obj)->flags |= fGAB_OBJ_GARBAGE)

#define GAB_OBJ_FREED(obj) ((obj)->flags |= fGAB_OBJ_FREED)

#define __KEEP_FLAGS (fGAB_OBJ_BUFFERED | fGAB_OBJ_GARBAGE | fGAB_OBJ_FREED)

#define GAB_OBJ_GREEN(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_GREEN)

#define GAB_OBJ_BLACK(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_BLACK)

#define GAB_OBJ_GRAY(obj)                                                      \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_GRAY)

#define GAB_OBJ_WHITE(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_WHITE)

#define GAB_OBJ_PURPLE(obj)                                                    \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_PURPLE)

#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~fGAB_OBJ_BUFFERED)

typedef enum gab_opcode {
#define OP_CODE(name) OP_##name,
#include "bytecode.h"
#undef OP_CODE
} gab_opcode;

static const char *gab_opcode_names[] = {
#define OP_CODE(name) #name,
#include "bytecode.h"
#undef OP_CODE
};

struct gab_mod;
struct gab_gc;
struct gab_vm;
struct gab_eg;

struct gab_obj;

struct gab_obj_string;
struct gab_obj_block_proto;
struct gab_obj_suspense_proto;
struct gab_obj_builtin;
struct gab_obj_block;
struct gab_obj_message;
struct gab_obj_shape;
struct gab_obj_record;
struct gab_obj_box;
struct gab_obj_suspense;

typedef void (*gab_gcvisit_f)(struct gab_gc *gc, struct gab_obj *obj);

typedef void (*gab_builtin_f)(struct gab_eg *gab, struct gab_gc *gc,
                              struct gab_vm *vm, size_t argc,
                              gab_value argv[argc]);

typedef void (*gab_boxdestroy_f)(void *data);

typedef void (*gab_boxvisit_f)(struct gab_gc *gc, gab_gcvisit_f visitor,
                               void *data);

/**
 * This header is the first member of all heap-allocated objects.
 *
 * This way, garbage collection code can refer to all heap objects through a
 * single interface.
 */
struct gab_obj {
  int32_t references;
  gab_kind kind;
  uint8_t flags;
};

/**
 * An under-the-hood allocator for objects.
 *
 * @param gab The gab engine.
 *
 * @param loc The object whose memory is being reallocated, or NULL.
 *
 * @param size The size to allocate. When size is 0, the object is freed.
 *
 * @return The allocated memory, or NULL.
 */
void *gab_obj_alloc(struct gab_eg *gab, struct gab_obj *loc, uint64_t size);

/**
 * Free any memory owned by the object, but not the object itself.
 *
 * @param self The object whose memory should be freed.
 */
void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *obj);

/**
 * Get the size of the object in bytes.
 *
 * @param obj The object to calculate the size of.
 *
 * @return The size of the object in bytes.
 */
uint64_t gab_obj_size(struct gab_obj *obj);

/**
 * # Create a gab_eg.
 * This struct stores data about the environment that gab
 * code executes in.
 *
 * @return A pointer to the struct on the heap.
 */
struct gab_eg *gab_create();

/**
 * # Destroy a gab_eg.
 * Free all memory associated with the engine.
 *
 * @param gab The engine.
 */
void gab_destroy(struct gab_eg *gab);

/**
 * # Set the value of the argument at index to value.
 *
 *  @param gab The engine.
 *
 *  @param value The values of the argument.
 *
 *  @param index The index of the argument to change.
 */
void gab_argput(struct gab_eg *gab, gab_value value, size_t index);

/**
 * # Push an additional argument onto the engine's argument list.
 *
 *  @param gab The engine.
 *
 *  @param name The name of the argument (Passed to the compiler).
 *
 *  @param value The values of the argument (Passed to the vm).
 */
size_t gab_argpush(struct gab_eg *gab, gab_value name);

/**
 * # Pop an argument off the engine's argument list.
 *
 *  @param gab The engine.
 */
void gab_argpop(struct gab_eg *gab);

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
size_t gab_egkeep(struct gab_eg *gab, gab_value v);

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
size_t gab_negkeep(struct gab_eg *gab, size_t len, gab_value argv[static len]);

/**
 * # Push a value(s) onto the vm's internal stack
 *
 * This is used to return values from c-builtins.
 *
 * @param vm The vm that will receive the values.
 *
 * @return The number of values pushed.
 */
size_t gab_vmpush(struct gab_vm *vm, gab_value v);

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
size_t gab_nvmpush(struct gab_vm *vm, size_t len, gab_value argv[static len]);

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
a_gab_value *gab_run(struct gab_eg *gab, gab_value main, size_t flags);

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
a_gab_value *gab_execute(struct gab_eg *gab, const char *name,
                         const char *source, size_t flags);

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
a_gab_value *gab_send(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                      gab_value message, gab_value receiver, size_t len,
                      gab_value argv[len]);

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
gab_value gab_panic(struct gab_eg *gab, struct gab_vm *vm, const char *msg);

#if cGAB_LOG_GC

gab_value __gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value val, const char *file, int line);

#define gab_gcdref(gab, gc, vm, val)                                           \
  (__gab_gcdref(gab, gc, vm, val, __FILE__, __LINE__))

gab_value __gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value val, const char *file, int line);

#define gab_gciref(gab, gc, vm, val)                                           \
  (__gab_gciref(gab, gc, vm, val, __FILE__, __LINE__))

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void __gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line);
#define gab_ngciref(gab, gc, vm, stride, len, values)                          \
  (__gab_ngciref(gab, gc, vm, stride, len, values, __FILE__, __LINE__))

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void __gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line);
#define gab_ngcdref(gab, gc, vm, stride, len, values)                          \
  (__gab_ngcdref(gab, gc, vm, stride, len, values, __FILE__, __LINE__))

/**
 * # Run the garbage collector.
 *
 * @param gab The engine.
 *
 * @param gc The garbage collector.
 *
 * @param vm The vm.
 */
void __gab_gcrun(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 const char *file, int line);
#define gab_gcrun(gab, gc, vm) (__gab_gcrun(gab, gc, vm, __FILE__, __LINE__))

#else

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
gab_value gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param len The number of values.
 *
 * @param values The values.
 */
gab_value gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 *
 * @param value The value.
 */
void gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]);

/**
 * # Run the garbage collector.
 *
 * @param gab The engine.
 *
 * @param gc The garbage collector.
 *
 * @param vm The vm.
 */
void gab_gcrun(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm);

#endif

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
static inline gab_kind gab_valknd(gab_value value) {
  if (__gab_valisn(value))
    return kGAB_NUMBER;

  if (gab_valiso(value))
    return gab_valtoo(value)->kind;

  return __GAB_VAL_TAG(value);
}

/**
 * An immutable sequence of characters.
 */
struct gab_obj_string {
  struct gab_obj header;

  uint64_t hash;

  size_t len;

  char data[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_string*) */
#define GAB_VAL_TO_STRING(value) ((struct gab_obj_string *)gab_valtoo(value))

/**
 * # Create a gab_value from a c-string
 *
 * @param gab The engine.
 *
 * @param data The data.
 *
 * @return The value.
 */
gab_value gab_string(struct gab_eg *gab, const char data[static 1]);

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
gab_value gab_nstring(struct gab_eg *gab, size_t len,
                      const char data[static len]);

/**
 * # Concatenate two gab strings
 *
 * @param gab The engine.
 *
 * @param a The first string.
 *
 * @param b The second string.
 *
 * @return The value.
 */
gab_value gab_strcat(struct gab_eg *gab, gab_value a, gab_value b);

/**
 * # Get the length of a string
 *
 */
static inline size_t gab_strlen(gab_value str) {
  assert(gab_valknd(str) == kGAB_STRING);
  return GAB_VAL_TO_STRING(str)->len;
};

/**
 * A wrapper for a native c function.
 */
struct gab_obj_builtin {
  struct gab_obj header;

  gab_builtin_f function;

  gab_value name;
};

/* Cast a value to a (gab_obj_builtin*) */
#define GAB_VAL_TO_BUILTIN(value) ((struct gab_obj_builtin *)gab_valtoo(value))

/**
 * The prototype of a block. Encapsulates everything known about a block
 * at compile time.
 */
struct gab_obj_block_proto {
  struct gab_obj header;

  uint8_t narguments, nupvalues, nslots, nlocals;

  struct gab_mod *mod;

  uint8_t upv_desc[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_block_proto*) */
#define GAB_VAL_TO_BLOCK_PROTO(value)                                          \
  ((struct gab_obj_block_proto *)gab_valtoo(value))

/**
 * Create a new prototype object.
 *
 * @param gab The gab engine.
 *
 * @param mod The bytecode module that the prototype should wrap.
 *
 * @param narguments The number of arguments the function takes.
 *
 * @param nslots The number of slots the function uses.
 *
 * @param nlocals The number of locals the function uses.
 *
 * @param nupvalues The number of upvalues the function uses.
 *
 * @param var Whether the function is variadic.
 *
 * @param flags The flags of the upvalues.
 *
 * @param indexes The indexes of the upvalues.
 */
gab_value gab_blkproto(struct gab_eg *gab, struct gab_mod *mod,
                       uint8_t narguments, uint8_t nslots, uint8_t nlocals,
                       uint8_t nupvalues, uint8_t flags[static nupvalues],
                       uint8_t indexes[static nupvalues]);

/**
 * A block - aka a prototype and it's captures.
 */
struct gab_obj_block {
  struct gab_obj header;

  uint8_t nupvalues;

  struct gab_obj_block_proto *p;

  gab_value upvalues[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_block*) */
#define GAB_VAL_TO_BLOCK(value) ((struct gab_obj_block *)gab_valtoo(value))

/**
 * Create a new block object, setting all captures to gab_nil.
 *
 * @param gab The gab engine.
 *
 * @param prototype The prototype of the block.
 *
 * @return The new block object.
 */
gab_value gab_block(struct gab_eg *gab, gab_value prototype);

#define NAME specs
#define K gab_value
#define V gab_value
#define DEF_V gab_undefined
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

#define NAME helps
#define K gab_value
#define V s_char
#define DEF_V ((s_char){0})
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

/**
 * The message object, a collection of receivers and specializations, under a
 * name.
 */
struct gab_obj_message {
  struct gab_obj header;

  uint8_t version;

  gab_value name;

  uint64_t hash;

  d_specs specs;
};

/* Cast a value to a (gab_obj_message*) */
#define GAB_VAL_TO_MESSAGE(value) ((struct gab_obj_message *)gab_valtoo(value))

/**
 * Create a new message object.
 *
 * @param gab The gab engine.
 *
 * @param name The name of the message.
 *
 * @return The new message object.
 */
gab_value gab_message(struct gab_eg *gab, gab_value name);

/**
 * Find the index of a receiver's specializtion in the message.
 *
 * @param self The message object.
 *
 * @param receiver The receiver to look for.
 *
 * @return The index of the receiver's specialization, or UINT64_MAX if it
 * doesn't exist.
 */
static inline uint64_t gab_msgfind(gab_value msg, gab_value needle) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  if (!d_specs_exists(&obj->specs, needle))
    return UINT64_MAX;

  return d_specs_index_of(&obj->specs, needle);
}

/**
 * Set the receiver and specialization at the given offset in the message.
 * This is to be used internally by the vm.
 *
 * @param obj The message object.
 *
 * @param offset The offset in the message.
 *
 * @param rec The receiver.
 *
 * @param spec The specialization.
 */
static inline void gab_umsgput(gab_value msg, uint64_t offset,
                               gab_value receiver, gab_value specialization) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  d_specs_iset_key(&obj->specs, offset, receiver);
  d_specs_iset_val(&obj->specs, offset, specialization);
  obj->version++;
}

/**
 * Get the specialization at the given offset in the message.
 *
 * @param obj The message object.
 *
 * @param offset The offset in the message.
 */
static inline gab_value gab_umsgat(gab_value msg, uint64_t offset) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  return d_specs_ival(&obj->specs, offset);
}

/**
 * Insert a specialization into the message for a given receiver.
 *
 * @param obj The message object.
 *
 * @param receiver The receiver.
 *
 * @param specialization The specialization.
 */
static inline void gab_msgput(gab_value msg, gab_value receiver,
                              gab_value specialization) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  d_specs_insert(&obj->specs, receiver, specialization);
  obj->version++;
}

/**
 * Read the specialization for a given receiver.
 *
 * @param obj The message object.
 *
 * @param receiver The receiver.
 *
 * @return The specialization for the receiver, or gab_undefined if it did
 * not exist.
 */
static inline gab_value gab_msgat(gab_value msg, gab_value receiver) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return d_specs_read(&obj->specs, receiver);
}

/**
 * A shape object, used to define the layout of a record.
 */
struct gab_obj_shape {
  struct gab_obj header;

  uint64_t hash, len;

  gab_value data[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_shape*) */
#define GAB_VAL_TO_SHAPE(v) ((struct gab_obj_shape *)gab_valtoo(v))

/**
 * Create a new shape object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param stride The stride of the keys in they key array.
 *
 * @param len The length of the shape.
 *
 * @param keys The key array.
 */
gab_value gab_shape(struct gab_eg *gab, bool *internedOut, size_t stride,
                    size_t len, gab_value keys[static len]);

/**
 * Create a new shape for a tuple. The keys are monotonic increasing integers,
 * starting from 0.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param len The length of the tuple.
 */
gab_value gab_nshape(struct gab_eg *gab, uint64_t len);

/**
 * Find the offset of a key in the shape.
 *
 * @param obj The shape object.
 *
 * @param key The key to look for.
 *
 * @return The offset of the key, or UINT64_MAX if it doesn't exist.
 */
static inline uint64_t gab_shpfind(gab_value shp, gab_value key) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  for (uint64_t i = 0; i < obj->len; i++) {
    assert(i < UINT64_MAX);

    if (obj->data[i] == key)
      return i;
  }

  return UINT64_MAX;
};

/**
 * # Get the length of a shape.
 *
 * @param value The shape.
 *
 * @return The length of the shape.
 */
static inline size_t gab_shplen(gab_value shp) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  return obj->len;
};

/**
 * # Get a pointer to the keys of a shape
 *
 * @param value The shape.
 *
 * @return The keys
 */
static inline gab_value *gab_shpdata(gab_value shp) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  return obj->data;
};

/**
 * Get the offset of the next key in the shape.
 *
 * @param obj The shape object.
 *
 * @param key The key to look for.
 *
 * @return The offset of the next key, 0 if the key is gab_undefined, or
 * UINT64_MAX if it doesn't exist.
 */
static inline uint64_t gab_shpnext(gab_value shp, gab_value key) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  if (key == gab_undefined)
    return 0;

  uint64_t offset = gab_shpfind(shp, key);

  if (offset == UINT64_MAX)
    return UINT64_MAX;

  return offset + 1;
};

/**
 * The counterpart to shape, which holds the data.
 */
struct gab_obj_record {
  struct gab_obj header;

  struct gab_obj_shape *shape;

  uint64_t len;

  gab_value data[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_record*) */
#define GAB_VAL_TO_RECORD(value) ((struct gab_obj_record *)gab_valtoo(value))

/**
 * Create a new record object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param shape The shape of the record.
 *
 * @param stride The stride of the values in the values array.
 *
 * @param values The values array. The length is inferred from shape.
 *
 * @return The new record object.
 */
gab_value gab_recordof(struct gab_eg *gab, gab_value shp, size_t stride,
                       gab_value values[static GAB_VAL_TO_SHAPE(shp)->len]);

/**
 * # Create a nil-initialized (empty) record of shape shape.
 *
 * @param gab The engine.
 *
 * @param shape The shape of the record.
 */
gab_value gab_erecordof(struct gab_eg *gab, gab_value shape);

/**
 * Set a value in the record. This function is not bounds checked. It should be
 * used only internally in the vm.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param obj The record object.
 */
static inline void gab_urecput(gab_value rec, uint64_t offset,
                               gab_value value) {
  assert(gab_valknd(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  assert(offset < obj->len);

  obj->data[offset] = value;
}

/**
 * Get the value corresponding to the given key.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return The value at the key, or gab_nil.
 */
static inline gab_value gab_recat(gab_value rec, gab_value key) {
  assert(gab_valknd(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  uint64_t offset = gab_shpfind(__gab_obj(obj->shape), key);

  if (offset >= obj->len)
    return gab_nil;

  return obj->data[offset];
}

static inline gab_value gab_srecat(struct gab_eg *gab, gab_value value,
                                   const char *key) {
  return gab_recat(value, gab_string(gab, key));
}

/**
 * Put a value in the record at the given key.
 *
 * @param vm The vm.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @param value The value.
 *
 * @return true if the put was a success, false otherwise.
 */
static inline bool gab_recput(gab_value rec, gab_value key, gab_value value) {
  assert(gab_valknd(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  uint64_t prop_offset = gab_shpfind(__gab_obj(obj->shape), key);

  if (prop_offset == UINT64_MAX)
    return false;

  gab_urecput(rec, prop_offset, value);

  return true;
}

static inline bool gab_srecput(struct gab_eg *gab, gab_value value,
                               const char *key, gab_value v) {
  return gab_recput(value, gab_string(gab, key), v);
}

/**
 * Check if the record has a value at the given key.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return true if the key exists on the record, false otherwise.
 */
static inline bool gab_rechas(gab_value obj, gab_value key) {
  return gab_recat(obj, key) != gab_nil;
}

static inline bool gab_srechas(struct gab_eg *gab, gab_value value,
                               const char *key) {
  return gab_rechas(value, gab_string(gab, key));
}
/**
 * # Get the shape of a record.
 *
 * @param value The record.
 *
 * @return The shape of the record.
 */
static inline gab_value gab_recshp(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return __gab_obj(GAB_VAL_TO_RECORD(value)->shape);
};

/**
 * # Get the length of a record.
 *
 * @param value The record.
 *
 * @return The length of the record.
 */
static inline size_t gab_reclen(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->len;
};

/**
 * # Get a pointer to the members of a record.
 *
 * @param value The record.
 *
 * @return The members of the record.
 *
 */
static inline gab_value *gab_recdata(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->data;
};

/**
 * A container object, which holds arbitrary data.
 *
 * There are two callbacks:
 *  - one to do cleanup when the object is destroyed
 *  - one to visit children values when doing garbage collection.
 */
struct gab_obj_box {
  struct gab_obj header;

  gab_boxdestroy_f do_destroy;

  gab_boxvisit_f do_visit;

  gab_value type;

  void *data;
};

#define GAB_VAL_TO_BOX(value) ((struct gab_obj_box *)gab_valtoo(value))

/**
 * A suspense object prototye, which holds the information about a suspense that
 * we know at compile time
 */
struct gab_obj_suspense_proto {
  struct gab_obj header;

  uint8_t want;

  uint64_t offset;
};

#define GAB_VAL_TO_SUSPENSE_PROTO(value)                                       \
  ((struct gab_obj_suspense_proto *)gab_valtoo(value))

/**
 * Create a new suspense object prototype.
 *
 * @param gab The gab engine.
 *
 * @param offset The offset in the block.
 *
 * @param want The number of values the block wants.
 */
gab_value gab_susproto(struct gab_eg *gab, uint64_t offset, uint8_t want);

/**
 * A suspense object, which holds the state of a suspended coroutine.
 */
struct gab_obj_suspense {
  struct gab_obj header;

  uint16_t len;

  struct gab_obj_suspense_proto *p;

  struct gab_obj_block *b;

  gab_value frame[FLEXIBLE_ARRAY];
};

#define GAB_VAL_TO_SUSPENSE(value)                                             \
  ((struct gab_obj_suspense *)gab_valtoo(value))

/**
 * Create a new suspense object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param c The paused block.
 *
 * @param len The length of the frame.
 *
 * @param frame The frame.
 */
gab_value gab_suspense(struct gab_eg *gab, uint16_t len,
                       struct gab_obj_block *block,
                       struct gab_obj_suspense_proto *proto,
                       gab_value frame[static len]);

/**
 * # Create a builtin wrapper to a c function with a gab_string name.
 *
 * @param gab The engine.
 *
 * @param name The name of the builtin.
 *
 * @param f The c function.
 *
 * @return The value.
 */
gab_value gab_builtin(struct gab_eg *gab, gab_value name, gab_builtin_f f);

/**
 * # Create a builtin wrapper to a c function with a c-string name.
 *
 * @param gab The engine.
 *
 * @param name The name of the builtin.
 *
 * @param f The c function.
 *
 * @return The value.
 */
gab_value gab_sbuiltin(struct gab_eg *gab, const char *name, gab_builtin_f f);

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
gab_value gab_box(struct gab_eg *gab, struct gab_box_argt args);

/**
 * # Get the user data from a boxed value.
 *
 * @param value The value.
 *
 * @return The user data.
 */
static inline void *gab_boxdata(gab_value value) {
  assert(gab_valknd(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->data;
}

struct gab_compile_argt {
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
gab_value gab_compile(struct gab_eg *gab, struct gab_compile_argt args);

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
gab_value gab_record(struct gab_eg *gab, size_t len, gab_value keys[static len],
                     gab_value values[static len]);

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
gab_value gab_srecord(struct gab_eg *gab, size_t len,
                      const char *keys[static len],
                      gab_value values[static len]);

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
gab_value gab_tuple(struct gab_eg *gab, size_t len,
                    gab_value values[static len]);

/**
 * # Bundle a list of values into a tuple.
 *
 * @param gab The engine
 *
 * @param vm The vm. This can be NULL.
 *
 * @param len The length of values array.
 *
 * @return The new record.
 */
gab_value gab_etuple(struct gab_eg *gab, size_t len);

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
gab_value gab_spec(struct gab_eg *gab, struct gab_spec_argt args);

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
gab_value gab_valcpy(struct gab_eg *gab, struct gab_vm *vm, gab_value value);

/**
 * # Get the runtime value that corresponds to the given kind.
 *
 * @param gab The engine
 *
 * @param kind The type to retrieve the value for.
 *
 * @return The runtime value corresponding to that type.
 */
gab_value gab_typ(struct gab_eg *gab, gab_kind kind);

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
static inline gab_value gab_valtyp(struct gab_eg *gab, gab_value value) {
  gab_kind k = gab_valknd(value);
  switch (k) {
  case kGAB_NIL:
  case kGAB_UNDEFINED:
    return value;

  case kGAB_RECORD: {
    struct gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return __gab_obj(obj->shape);
  }

  case kGAB_BOX: {
    struct gab_obj_box *con = GAB_VAL_TO_BOX(value);
    return con->type;
  }
  default:
    return gab_typ(gab, k);
  }
}

/**
 * # Coerce a value *into* a boolean.
 *
 * @param self The value to check.
 *
 * @return False if the value is false or nil. Otherwise true.
 */
static inline bool gab_valintob(gab_value self) {
  return !(self == gab_false || self == gab_nil);
}

/**
 * # Coerce the given value to a gab string.
 *
 * @param gab The engine
 *
 * @param self The value to convert
 *
 * @return The string representation of the value.
 */
static inline gab_value gab_valintos(struct gab_eg *gab, gab_value self) {
  switch (gab_valknd(self)) {
  case kGAB_STRING:
    return self;
  case kGAB_BLOCK:
    return gab_string(gab, "<block>");
  case kGAB_RECORD:
    return gab_string(gab, "<record>");
  case kGAB_SHAPE:
    return gab_string(gab, "<shape>");
  case kGAB_MESSAGE:
    return gab_string(gab, "<message>");
  case kGAB_BLOCK_PROTO:
    return gab_string(gab, "<prototype>");
  case kGAB_BUILTIN:
    return gab_string(gab, "<builtin>");
  case kGAB_BOX:
    return gab_string(gab, "<container>");
  case kGAB_SUSPENSE:
    return gab_string(gab, "<suspense>");
  case kGAB_TRUE:
    return gab_string(gab, "true");
  case kGAB_FALSE:
    return gab_string(gab, "false");
  case kGAB_NIL:
    return gab_string(gab, "nil");
  case kGAB_UNDEFINED:
    return gab_string(gab, "undefined");
  case kGAB_PRIMITIVE:
    return gab_string(gab, gab_opcode_names[gab_valtop(self)]);
  case kGAB_NUMBER: {
    char str[24];
    snprintf(str, 24, "%g", gab_valton(self));
    return gab_string(gab, str);
  }
  default:
    assert(false && "Unhandled type in gab_valtos");
    return gab_undefined;
  }
}

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
static inline s_char gab_valintocs(struct gab_eg *gab, gab_value value) {
  gab_value str = gab_valintos(gab, value);

  struct gab_obj_string *obj = GAB_VAL_TO_STRING(str);

  return (s_char){.len = obj->len, .data = obj->data};
}

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
int32_t gab_fdump(FILE *stream, gab_value self);

/**
 * # Disassemble a module.
 *
 * This is useful for debugging bytecode.
 *
 * @param mod The module.
 */
void gab_fdis(FILE *stream, struct gab_mod *mod);

/**
 * # Pry into the frame at the given depth in the callstack.
 *
 * @param gab The engine.
 *
 * @param vm The vm.
 *
 * @param depth The depth.
 */
void gab_fpry(FILE *stream, struct gab_vm *vm, uint64_t depth);
#endif
