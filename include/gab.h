/**
 * @file
 * @brief A small, fast, and portable implementation of the gab programming
 * language
 */

#ifndef GAB_H
#define GAB_H

#include <stdint.h>
#include <stdio.h>

#include "core.h"
#include "types.h"

#ifdef cGAB_LIKELY
#define __gab_likely(x) (__builtin_expect(!!(x), 1))
#define __gab_unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define __gab_likely(x) (x)
#define __gab_unlikely(x) (x)
#endif

/**
 * \file
 * \brief The API for the gab programming language.
 */

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

enum gab_kind {
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
  kGAB_BPROTOTYPE,
  kGAB_SPROTOTYPE,
  kGAB_NATIVE,
  kGAB_BLOCK,
  kGAB_BOX,
  kGAB_RECORD,
  kGAB_SHAPE,
  kGAB_NKINDS,
};

#define __GAB_QNAN ((uint64_t)0x7ffc000000000000)

#define __GAB_SIGN_BIT ((uint64_t)1 << 63)

#define __GAB_TAGMASK (7)

#define __GAB_VAL_TAG(val) ((enum gab_kind)((val) & __GAB_TAGMASK))

// Sneakily use a union to get around the type system
static inline double __gab_valtod(gab_value value) {
  union {
    uint64_t bits;
    double num;
  } data;
  data.bits = value;
  return data.num;
}

static inline gab_value __gab_dtoval(double value) {
  union {
    uint64_t bits;
    double num;
  } data;
  data.num = value;
  return data.bits;
}

#define __gab_valisn(val) (((val) & __GAB_QNAN) != __GAB_QNAN)

#define __gab_obj(val)                                                         \
  (gab_value)(__GAB_SIGN_BIT | __GAB_QNAN | (uint64_t)(uintptr_t)(val))

#define gab_valiso(val)                                                        \
  (((val) & (__GAB_QNAN | __GAB_SIGN_BIT)) == (__GAB_QNAN | __GAB_SIGN_BIT))

#define gab_valisnew(val) (gab_valiso(val) && GAB_OBJ_IS_NEW(gab_valtoo(val)))

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

/* Convenience macro for getting arguments in builtins */
#define gab_arg(i) (i < argc ? argv[i] : gab_nil)

/*
 * Gab uses a purely RC garbage collection approach.
 *
 * The algorithm is described in this paper:
 * https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon03Pure.pdf
 */
#define fGAB_OBJ_BLACK (1 << 1)
#define fGAB_OBJ_GRAY (1 << 2)
#define fGAB_OBJ_WHITE (1 << 3)
#define fGAB_OBJ_PURPLE (1 << 4)
#define fGAB_OBJ_GREEN (1 << 5)
#define fGAB_OBJ_BUFFERED (1 << 6)
#define fGAB_OBJ_NEW (1 << 7)
#define fGAB_OBJ_FREED (1 << 8) // Used for debug purposes

#define GAB_OBJ_IS_BLACK(obj) ((obj)->flags & fGAB_OBJ_BLACK)
#define GAB_OBJ_IS_GRAY(obj) ((obj)->flags & fGAB_OBJ_GRAY)
#define GAB_OBJ_IS_WHITE(obj) ((obj)->flags & fGAB_OBJ_WHITE)
#define GAB_OBJ_IS_PURPLE(obj) ((obj)->flags & fGAB_OBJ_PURPLE)
#define GAB_OBJ_IS_GREEN(obj) ((obj)->flags & fGAB_OBJ_GREEN)
#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & fGAB_OBJ_BUFFERED)
#define GAB_OBJ_IS_NEW(obj) ((obj)->flags & fGAB_OBJ_NEW)
#define GAB_OBJ_IS_FREED(obj) ((obj)->flags & fGAB_OBJ_FREED)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags |= fGAB_OBJ_BUFFERED)

#define GAB_OBJ_NEW(obj) ((obj)->flags |= fGAB_OBJ_NEW)

#define GAB_OBJ_FREED(obj) ((obj)->flags |= fGAB_OBJ_FREED)

#define __KEEP_FLAGS (fGAB_OBJ_BUFFERED | fGAB_OBJ_NEW | fGAB_OBJ_FREED)

#define GAB_OBJ_GREEN(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_GREEN)

#define GAB_OBJ_BLACK(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_BLACK)

#define GAB_OBJ_GRAY(obj)                                                      \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_GRAY)

#define GAB_OBJ_PURPLE(obj)                                                    \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_PURPLE)

#define GAB_OBJ_WHITE(obj)                                                     \
  ((obj)->flags = ((obj)->flags & __KEEP_FLAGS) | fGAB_OBJ_WHITE)

#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~fGAB_OBJ_BUFFERED)
#define GAB_OBJ_NOT_NEW(obj) ((obj)->flags &= ~fGAB_OBJ_NEW)

#define T gab_value
#include "array.h"

#define T gab_value
#include "vector.h"

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

struct gab_gc;
struct gab_vm;
struct gab_eg;

struct gab_triple {
  struct gab_eg *eg;
  struct gab_gc *gc;
  struct gab_vm *vm;
  int flags;
};

struct gab_obj;

struct gab_obj_string;
struct gab_obj_prototype;
struct gab_obj_native;
struct gab_obj_block;
struct gab_obj_message;
struct gab_obj_shape;
struct gab_obj_record;
struct gab_obj_box;
struct gab_obj_suspense;

typedef void (*gab_gcvisit_f)(struct gab_triple, struct gab_obj *obj);

typedef a_gab_value *(*gab_native_f)(struct gab_triple, size_t argc,
                                     gab_value argv[argc]);

typedef void (*gab_boxdestroy_f)(size_t len, unsigned char data[static len]);

typedef void (*gab_boxvisit_f)(struct gab_triple gab, gab_gcvisit_f visitor,
                               size_t len, unsigned char data[static len]);

/**
 * @class gab_obj
 * @brief This struct is the first member of all heap-allocated objects.
 */
struct gab_obj {
  uint8_t references;
  uint8_t flags;
  enum gab_kind kind;
};

/**
 * @brief INTERNAL: Free memory held by an object.
 *
 * @param eg The engine responsible for the object.
 * @param obj The object.
 */
void gab_obj_destroy(struct gab_eg *eg, struct gab_obj *obj);

/**
 * @brief Compute the size of an object, in bytes.
 *
 * @param obj The object.
 * @return The number of bytes allocated for the object.
 */
size_t gab_obj_size(struct gab_obj *obj);

/**
 * @brief Allocate data necessary for a runtime. This struct is lightweight and
 * should be passed by value.
 */
struct gab_triple gab_create();

/**
 * @brief Free the memory owned by this triple.
 *
 * @param gab The triple to free.
 */
void gab_destroy(struct gab_triple gab);

/**
 * @brief Print the bytecode to the stream - useful for debugging.
 *
 * @param stream The stream to print to
 * @param proto The prototype to inspect
 * @return non-zero if an error occured.
 */
int gab_fmodinspect(FILE *stream, struct gab_obj_prototype *proto);

/**
 * @brief Print a gab_value to the given stream. Will prent nested values as
 * deep as depth.
 *
 * @param stream The stream to print to
 * @param value The value to inspect
 * @param depth The depth to recurse to
 * @return the number of bytes written to the stream.
 */
int gab_fvalinspect(FILE *stream, gab_value value, int depth);

/**
 * @brief Give the engine ownership of the values. When in c-code,
 * it can be useful to have gab_values outlive the function
 * they are created in, but if they aren't referenced by another value they will
 * be collected. Thus, the scratch buffer is useful to hold a reference to the
 * value until the engine is destroyed.
 *
 *  @param eg The engine.
 *  @param value The value to keep.
 *  @return The value.
 */
size_t gab_egkeep(struct gab_eg *eg, gab_value value);

/**
 * @brief Give the engine ownership of multiple values.
 * @see gab_egkeep.
 *
 *  @param gab The engine.
 *  @param len The number of values to keep.
 *  @param values The values.
 *  @return The total number of values kept by the engine.
 */
size_t gab_negkeep(struct gab_eg *eg, size_t len, gab_value argv[static len]);

/**
 * @class gab_egimpl_rest
 * @brief The result of gab_egimpl
 * @see gab_egimpl.
 *
 */
struct gab_egimpl_rest {
  gab_value type;
  size_t offset;
  int status;
};

enum {
  sGAB_IMPL_NONE = 0,
  sGAB_IMPL_TYPE,
  sGAB_IMPL_KIND,
  sGAB_IMPL_GENERAL,
  sGAB_IMPL_PROPERTY,
};

/**
 * @brief Find the implementation of a message for a given receiver.
 *
 * @param eg The engine to search for an implementation.
 * @param message The message to find.
 * @param receiver The receiver to find the implementation for.
 */
static inline struct gab_egimpl_rest
gab_egimpl(struct gab_eg *eg, gab_value message, gab_value receiver);

/**
 * @brief Push a value onto the vm's internal stack. This is how
 * c-natives can return values to the runtime.
 *
 * @param vm The vm to push the values onto.
 * @param value the value to push.
 * @return The number of values pushed (always one).
 */
size_t gab_vmpush(struct gab_vm *vm, gab_value value);

/**
 * @brief Push multiple values onto the vm's internal stack.
 * @see gab_vmpush.
 *
 * @param vm The vm that will receive the values.
 * @param len The number of values.
 * @param argv The array of values.
 */
size_t gab_nvmpush(struct gab_vm *vm, size_t len, gab_value argv[static len]);

/**
 * @brief Inspect the vm at depth N in the callstack.
 *
 * @param stream The stream to print to.
 * @param vm The vm.
 * @param depth The depth of the callframe. '0' would be the topmost callframe.
 */
void gab_fvminspect(FILE *stream, struct gab_vm *vm, uint64_t depth);

/**
 * @brief Inspect the vm's callstack at the given depth, and return a value with
 * the relevant data.
 *
 * @param gab The triple to inspect.
 * @param depth The depth of the callframe. '0' would be the topmost callframe.
 * @return The record.
 */
gab_value gab_vmframe(struct gab_triple gab, uint64_t depth);

/**
 * @brief Put a module into the engine's import table.
 *
 * @param eg The engine.
 * @param name The name of the import. This is used to lookup the import.
 * @param mod The module to store. This is usually a block, but can be anything
 * @param len The number of values returned by the module.
 * @param values The values returned by the module.
 * @returns The module if it was added, NULL otherwise.
 */
a_gab_value *gab_segmodput(struct gab_eg *eg, const char *name, gab_value mod,
                           size_t len, gab_value values[len]);

/**
 * @brief Check if an engine has an module by name.
 *
 * @param eg The engine.
 * @param name The name of the import.
 * @returns The module if it exists, NULL otherwise.
 */
a_gab_value *gab_segmodat(struct gab_eg *eg, const char *name);

/**
 * @class gab_cmpl_argt
 * @brief Arguments and options for compiling a string of source code.
 * @see gab_cmpl.
 * @see enum gab_flags.
 */
struct gab_cmpl_argt {
  /**
   * @brief The name of the module.
   */
  const char *name;
  /**
   * @brief The source code to compile.
   */
  const char *source;
  /**
   * @brief Optional flags for compilation.
   */
  int flags;
  /**
   * @brief The number of arguments expected by the main block.
   */
  size_t len;
  /**
   * @brief The names of the arguments expected by the main block.
   */
  const char **argv;
};

/**
 * @brief Compile a source string into a block.
 * Flag options are defined in @link enum gab_flags.
 *
 * @see struct gab_cmpl_argt.
 * @see enum gab_flags.
 *
 * @param gab The engine.
 * @param args The arguments.
 * @returns A gab_value containing the compiled block, which can be called.
 */
gab_value gab_cmpl(struct gab_triple gab, struct gab_cmpl_argt args);

/**
 * @class gab_run_argt
 * @brief Arguments and options for running a block.
 * @see gab_run
 */
struct gab_run_argt {
  /**
   * @brief The main block to run.
   */
  gab_value main;
  /**
   * @brief Optional flags for the vm.
   */
  int flags;
  /**
   * @brief The number of arguments passed to the main block.
   */
  size_t len;
  /**
   * @brief The arguments passed to the main block.
   */
  gab_value *argv;
};

/**
 * @brief Call a block
 * @see struct gab_run_argt
 *
 * @param  gab The triple.
 * @param args The arguments.
 * @return A heap-allocated slice of values returned by the block.
 */
a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args);

/**
 * @class gab_exec_argt
 * @brief Arguments and options for executing a source string.
 */
struct gab_exec_argt {
  /**
   * @brief The name of the module - defaults to "__main__".
   */
  const char *name;
  /**
   * @brief The source code to execute.
   */
  const char *source;
  /**
   * @brief Optional flags for compilation AND execution.
   */
  int flags;
  /**
   * @brief The number of arguments to the main block.
   */
  size_t len;
  /**
   * @brief The names of the arguments to the main block.
   */
  const char **sargv;
  /**
   * @brief The values of the arguments to the main block.
   */
  gab_value *argv;
};

/**
 * @brief Compile a source string to a block and run it.
 * This is equivalent to calling @link gab_cmpl and then @link gab_run on the
 * result.
 *
 * @see struct gab_exec_argt
 *
 * @param gab The triple.
 * @param args The arguments.
 * @return A heap-allocated slice of values returned by the block.
 */
a_gab_value *gab_exec(struct gab_triple gab, struct gab_exec_argt args);

/**
 * @class gab_repl_argt
 * @brief Arguments and options for an interactive REPL.
 */
struct gab_repl_argt {
  /**
   * @brief The prompt to display before each input of REPL.
   */
  const char *prompt_prefix;
  /**
   * @brief The prefix to display before each result of REPL.
   */
  const char *result_prefix;
  /**
   * @brief The name of the module - defaults to "__main__".
   */
  const char *name;
  /**
   * @brief Optional flags for compilation AND execution.
   */
  int flags;
  /**
   * @brief The number of arguments to the main block.
   */
  size_t len;
  /**
   * @brief The names of the arguments to the main block.
   */
  const char **sargv;
  /**
   * @brief The values of the arguments to the main block.
   */
  gab_value *argv;
};

/**
 * @brief Begin an interactive REPL.
 *
 * @see struct gab_repl_argt
 *
 * @param gab The engine.
 * @param args The arguments.
 * @return A heap-allocated slice of values returned by the block.
 */
void gab_repl(struct gab_triple gab, struct gab_repl_argt args);

/**
 * @brief If fGAB_DUMP_ERROR is set, print an error message to stderr.
 * If fGAB_EXIT_ON_PANIC is set, then exit the program.
 *
 * @param gab The triple.
 * @param fmt The format string.
 * @returns false.
 */
a_gab_value *gab_panic(struct gab_triple gab, const char *fmt, ...);

/**
 * @brief If fGAB_DUMP_ERROR is set, print a type-mismatch error message to
 * stderr.
 *
 * @param gab The triple.
 * @param found The value with the mismatched type.
 * @param texpected The expected type.
 * @returns false.
 */
a_gab_value *gab_ptypemismatch(struct gab_triple gab, gab_value found,
                               gab_value texpected);

#if cGAB_LOG_GC

#define gab_gciref(gab, val) (__gab_gciref(gab, val, __FUNCTION__, __LINE__))

gab_value __gab_gciref(struct gab_triple gab, gab_value val, const char *file,
                       int line);

#define gab_gcdref(gab, val) (__gab_gcdref(gab, val, __FUNCTION__, __LINE__))

gab_value __gab_gcdref(struct gab_triple gab, gab_value val, const char *file,
                       int line);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void __gab_ngciref(struct gab_triple gab, size_t stride, size_t len,
                   gab_value values[len], const char *file, int line);
#define gab_ngciref(gab, stride, len, values)                                  \
  (__gab_ngciref(gab, stride, len, values, __FUNCTION__, __LINE__))

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void __gab_ngcdref(struct gab_triple gab, size_t stride, size_t len,
                   gab_value values[len], const char *file, int line);
#define gab_ngcdref(gab, stride, len, values)                                  \
  (__gab_ngcdref(gab, stride, len, values, __FUNCTION__, __LINE__))

#else

/**
 * # Increment the reference count of the value(s)
 *
 * @param gab The gab triple.
 * @param value The value to increment.
 */
gab_value gab_iref(struct gab_triple gab, gab_value value);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param len The number of values.
 * @param values The values.
 */
gab_value gab_dref(struct gab_triple gab, gab_value value);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void gab_niref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]);

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void gab_ndref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]);

#endif

/**
 * # Run the garbage collector.
 *
 * @param gab The engine.
 * @param gc The garbage collector.
 * @param vm The vm.
 */
void gab_collect(struct gab_triple gab);

void gab_gclock(struct gab_gc *gc);
void gab_gcunlock(struct gab_gc *gc);
/**
 * # Get the kind of a value.
 * @see enum gab_kind
 * *NOTE* This is not the **runtime type** of the value. For that, use
 * `gab_valtyp`.
 *
 * @param value The value.
 * @return The kind of the value.
 *
 */
static inline enum gab_kind gab_valkind(gab_value value) {
  if (__gab_valisn(value))
    return kGAB_NUMBER;

  if (gab_valiso(value))
    return gab_valtoo(value)->kind;

  return __GAB_VAL_TAG(value);
}

static inline bool gab_valhasp(gab_value value) {
  enum gab_kind k = gab_valkind(value);
  return k == kGAB_RECORD;
}

static inline bool gab_valhast(gab_value value) {
  enum gab_kind k = gab_valkind(value);
  switch (k) {
  case kGAB_RECORD:
  case kGAB_BOX:
    return true;
  default:
    return false;
  }
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
 * @param data The data.
 * @return The value.
 */
gab_value gab_string(struct gab_triple gab, const char data[static 1]);

/**
 * # Create a gab_value from a bounded c-string
 *
 * @param gab The engine.
 * @param len The length of the string.
 * @param data The data.
 * @return The value.
 */
gab_value gab_nstring(struct gab_triple gab, size_t len,
                      const char data[static len]);

/**
 * # Concatenate two gab strings
 *
 * @param gab The engine.
 * @param a The first string.
 * @param b The second string.
 * @return The value.
 */
gab_value gab_strcat(struct gab_triple gab, gab_value a, gab_value b);

/**
 * # Get the length of a string
 *
 * @param str The string.
 * @return The length of the string.
 */
static inline size_t gab_strlen(gab_value str) {
  assert(gab_valkind(str) == kGAB_STRING);
  return GAB_VAL_TO_STRING(str)->len;
};

/**
 * A wrapper for a native c function.
 */
struct gab_obj_native {
  struct gab_obj header;

  gab_native_f function;

  gab_value name;
};

/* Cast a value to a (gab_obj_native*) */
#define GAB_VAL_TO_NATIVE(value) ((struct gab_obj_native *)gab_valtoo(value))

/**
 * A block - aka a prototype and it's captures.
 */
struct gab_obj_block {
  struct gab_obj header;

  unsigned char nupvalues;

  gab_value p;

  gab_value upvalues[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_block*) */
#define GAB_VAL_TO_BLOCK(value) ((struct gab_obj_block *)gab_valtoo(value))

/**
 * # Create a new block object, setting all captures to gab_nil.
 *
 * @param gab The gab engine.
 * @param prototype The prototype of the block.
 * @return The new block object.
 */
gab_value gab_block(struct gab_triple gab, gab_value prototype);

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
 * @param gab The gab triple.
 *
 * @param stride The stride of the keys in they key array.
 *
 * @param len The length of the shape.
 *
 * @param keys The key array.
 */
gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[static len]);

gab_value gab_shapewith(struct gab_triple gab, gab_value shape, gab_value key);

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
gab_value gab_nshape(struct gab_triple gab, uint64_t len);

#define GAB_PROPERTY_NOT_FOUND ((size_t)-1)
/**
 * Find the offset of a key in the shape.
 *
 * @param obj The shape object.
 *
 * @param key The key to look for.
 *
 * @return The offset of the key, or GAB_PROPERTY_NOT_FOUND if it doesn't exist.
 */
static inline size_t gab_shpfind(gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  // Linear search for the key in the shape
  for (size_t i = 0; i < obj->len; i++)
    if (obj->data[i] == key)
      return i;

  return GAB_PROPERTY_NOT_FOUND;
};

/**
 * # Get the length of a shape.
 *
 * @param value The shape.
 *
 * @return The length of the shape.
 */
static inline size_t gab_shplen(gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
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
  assert(gab_valkind(shp) == kGAB_SHAPE);
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
static inline size_t gab_shpnext(gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  if (key == gab_undefined)
    return 0;

  size_t offset = gab_shpfind(shp, key);

  if (offset == GAB_PROPERTY_NOT_FOUND)
    return GAB_PROPERTY_NOT_FOUND;

  return offset + 1;
};

/**
 * The counterpart to shape, which holds the data.
 */
struct gab_obj_record {
  struct gab_obj header;

  gab_value shape;

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
gab_value gab_recordof(struct gab_triple gab, gab_value shp, size_t stride,
                       gab_value values[static GAB_VAL_TO_SHAPE(shp)->len]);

gab_value gab_recordwith(struct gab_triple gab, gab_value rec, gab_value key,
                         gab_value value);

/**
 * @brief Create a nil-initialized (empty) record of shape shape.
 *
 * @param gab The triple.
 * @param shape The shape of the record.
 */
gab_value gab_erecordof(struct gab_triple gab, gab_value shape);

/**
 * @brief Set a value within the record.
 * This function is not bounds checked. For save alternatives,
 * try @link gab_recput or @link gab_srecput
 *
 * @see gab_recput
 * @see gab_srecput
 *
 * @param gab The triple.
 * @param rec The record.
 * @param offset The offset within the record.
 * @param value The new value to put into the record.
 */
static inline void gab_urecput(struct gab_triple gab, gab_value rec,
                               uint64_t offset, gab_value value) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  assert(offset < obj->len);

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_dref(gab, obj->data[offset]);

  obj->data[offset] = value;

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_iref(gab, obj->data[offset]);
}

/**
 * @brief Get the value within the record at the given offset.
 * This function is not bounds checked. For safe alternatives,
 * try @link gab_recat or @link gab_srecat
 *
 * @see gab_recat
 * @see gab_srecat
 *
 * @param rec The record.
 * @param offset the offset.
 * @retunrs the value at the offset.
 */
static inline gab_value gab_urecat(gab_value rec, uint64_t offset) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  assert(offset < obj->len);
  return obj->data[offset];
}

/**
 * Get the value corresponding to the given key.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return The value at the key, or gab_undefined.
 */
static inline gab_value gab_recat(gab_value rec, gab_value key) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  uint64_t offset = gab_shpfind(obj->shape, key);

  if (offset >= obj->len)
    return gab_undefined;

  return obj->data[offset];
}

/**
 * Get the offset of the given key in the record.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return The offset of the key, or UINT64_MAX if it doesn't exist.
 */
static inline uint64_t gab_recfind(gab_value rec, gab_value key) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);
  return gab_shpfind(obj->shape, key);
}

static inline gab_value gab_srecat(struct gab_triple gab, gab_value value,
                                   const char *key) {
  return gab_recat(value, gab_string(gab, key));
}

/**
 * @brief Put a value in the record at the given key.
 * @param gab The triple.
 * @param obj The record object.
 * @param key The key, as a value.
 * @param value The value.
 * @return true if the put was a success, false otherwise.
 */
static inline bool gab_recput(struct gab_triple gab, gab_value rec,
                              gab_value key, gab_value value) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  size_t offset = gab_shpfind(obj->shape, key);

  if (offset == GAB_PROPERTY_NOT_FOUND)
    return false;

  gab_urecput(gab, rec, offset, value);

  return true;
}

/**
 * @brief Put a value in the record at the given key.
 * @param gab The triple.
 * @param obj The record object.
 * @param key The key, as a c string literal.
 * @param value The value.
 * @return true if the put was a success, false otherwise.
 */
static inline bool gab_srecput(struct gab_triple gab, gab_value rec,
                               const char *key, gab_value v) {
  return gab_recput(gab, rec, gab_string(gab, key), v);
}

/**
 * @brief Check if the record has a value at the given key.
 * @param obj The record object.
 * @param key The key.
 * @return true if the key exists on the record, false otherwise.
 */
static inline bool gab_rechas(gab_value obj, gab_value key) {
  return gab_recat(obj, key) != gab_undefined;
}

static inline bool gab_srechas(struct gab_triple gab, gab_value value,
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
  assert(gab_valkind(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->shape;
};

/**
 * # Get the length of a record.
 *
 * @param value The record.
 *
 * @return The length of the record.
 */
static inline size_t gab_reclen(gab_value value) {
  assert(gab_valkind(value) == kGAB_RECORD);
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
  assert(gab_valkind(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->data;
};

/**
 * # The message object, a collection of receivers and specializations, under a
 * name.
 */
struct gab_obj_message {
  struct gab_obj header;

  uint64_t hash;

  gab_value name;

  gab_value specs;
};

/* Cast a value to a (gab_obj_message*) */
#define GAB_VAL_TO_MESSAGE(value) ((struct gab_obj_message *)gab_valtoo(value))

/**
 * # Create a new message object.
 *
 * @param gab The gab engine.
 * @param name The name of the message.
 * @return The new message object.
 */
gab_value gab_message(struct gab_triple gab, gab_value name);

static inline gab_value gab_msgshp(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return gab_recshp(GAB_VAL_TO_MESSAGE(msg)->specs);
}

static inline gab_value gab_msgrec(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return GAB_VAL_TO_MESSAGE(msg)->specs;
}

static inline gab_value gab_msgname(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return GAB_VAL_TO_MESSAGE(msg)->name;
}

/**
 * Find the index of a receiver's specializtion in the message.
 *
 * @param self The message object.
 * @param receiver The receiver to look for.
 * @return The index of the receiver's specialization, or UINT64_MAX if it
 * doesn't exist.
 */
static inline size_t gab_msgfind(gab_value msg, gab_value needle) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_recfind(obj->specs, needle);
}

/**
 * Get the specialization at the given offset in the message.
 *
 * @param obj The message object.
 *
 * @param offset The offset in the message.
 */
static inline gab_value gab_umsgat(gab_value msg, size_t offset) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_urecat(obj->specs, offset);
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
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_recat(obj->specs, receiver);
}

static inline gab_value gab_msgput(struct gab_triple gab, gab_value msg,
                                   gab_value receiver, gab_value spec) {
  gab_gclock(gab.gc);

  assert(gab_valkind(msg) == kGAB_MESSAGE);
  assert(gab_valkind(spec) == kGAB_PRIMITIVE ||
         gab_valkind(spec) == kGAB_BLOCK || gab_valkind(spec) == kGAB_NATIVE);

  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  if (gab_rechas(obj->specs, receiver))
    return gab_undefined;

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_dref(gab, obj->specs);

  obj->specs = gab_recordwith(gab, obj->specs, receiver, spec);

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_iref(gab, obj->specs);

  gab_gcunlock(gab.gc);

  return msg;
}

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

  size_t len;

  unsigned char data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_TO_BOX(value) ((struct gab_obj_box *)gab_valtoo(value))

/**
 * The prototype of a block. Encapsulates everything known about a block
 * at compile time.
 */
struct gab_obj_prototype {
  struct gab_obj header;

  gab_value name;

  /* The compiled source file */
  struct gab_src *src;

  size_t begin;

  /* The length of the variable sized data member, in bytes. */
  size_t len;

  union {
    struct {
      size_t end;
      unsigned char narguments, nupvalues, nslots, nlocals;
    } block;

    struct {
      char want;
    } suspense;
  } as;

  char data[FLEXIBLE_ARRAY];
};

/* Cast a value to a (gab_obj_bprototype*) */
#define GAB_VAL_TO_PROTOTYPE(value)                                            \
  ((struct gab_obj_prototype *)gab_valtoo(value))

struct gab_blkproto_argt {
  char narguments, nslots, nlocals, nupvalues, *flags, *indexes, *data;
};

/**
 * Create a new prototype object.
 *
 * @param gab The gab engine.
 * @param args The arguments.
 * @see struct gab_blkproto_argt
 * @return The new block prototype object.
 */
gab_value gab_bprototype(struct gab_triple gab, struct gab_src *src,
                         gab_value name, size_t begin, size_t end,
                         struct gab_blkproto_argt args);

/**
 * Create a new suspense object prototype.
 *
 * @param gab The gab engine.
 *
 * @param offset The offset in the block.
 *
 * @param want The number of values the block wants.
 */
gab_value gab_sprototype(struct gab_triple gab, struct gab_src *src,
                         gab_value name, size_t begin, uint8_t want);

/**
 * A suspense object, which holds the state of a suspended coroutine.
 */
struct gab_obj_suspense {
  struct gab_obj header;

  size_t nslots;

  gab_value p, b;

  gab_value slots[FLEXIBLE_ARRAY];
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
gab_value gab_suspense(struct gab_triple gab, gab_value proto, gab_value block,
                       size_t len, gab_value frame[static len]);

/**
 * # Create a native wrapper to a c function with a gab_string name.
 *
 * @param gab The engine.
 *
 * @param name The name of the native.
 *
 * @param f The c function.
 *
 * @return The value.
 */
gab_value gab_native(struct gab_triple gab, gab_value name, gab_native_f f);

/**
 * # Create a native wrapper to a c function with a c-string name.
 *
 * @param gab The engine.
 *
 * @param name The name of the native.
 *
 * @param f The c function.
 *
 * @return The value.
 */
gab_value gab_snative(struct gab_triple gab, const char *name, gab_native_f f);

struct gab_box_argt {
  size_t size;
  void *data;
  gab_value type;
  gab_boxdestroy_f destructor;
  gab_boxvisit_f visitor;
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
gab_value gab_box(struct gab_triple gab, struct gab_box_argt args);

/**
 * # Get the length of the user data from a boxed value.
 *
 * @param value The value.
 *
 * @return The user data.
 */
static inline size_t gab_boxlen(gab_value value) {
  assert(gab_valkind(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->len;
}

/**
 * # Get the user data from a boxed value.
 *
 * @param value The value.
 *
 * @return The user data.
 */
static inline void *gab_boxdata(gab_value value) {
  assert(gab_valkind(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->data;
}

/**
 * # Get the user type from a boxed value.
 *
 * @param value The value.
 *
 * @return The user type.
 */
static inline gab_value gab_boxtype(gab_value value) {
  assert(gab_valkind(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->type;
}

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
gab_value gab_record(struct gab_triple gab, size_t len,
                     gab_value keys[static len], gab_value values[static len]);

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
gab_value gab_srecord(struct gab_triple gab, size_t len,
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
gab_value gab_tuple(struct gab_triple gab, size_t len,
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
gab_value gab_etuple(struct gab_triple gab, size_t len);

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
int gab_nspec(struct gab_triple gab, size_t len,
              struct gab_spec_argt args[static len]);

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
gab_value gab_spec(struct gab_triple gab, struct gab_spec_argt args);

/**
 * # Deep-copy a value.
 *
 * @param gab The engine.
 *
 * @param value The value to copy.
 *
 * @return The copy.
 */
gab_value gab_valcpy(struct gab_triple gab, gab_value value);

/**
 * # Get the runtime value that corresponds to the given kind.
 *
 * @param gab The engine
 *
 * @param kind The type to retrieve the value for.
 *
 * @return The runtime value corresponding to that type.
 */
static inline gab_value gab_type(struct gab_eg *gab, enum gab_kind kind);

/**
 * # Get the practical runtime type of a value.
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
static inline gab_value gab_valtype(struct gab_eg *gab, gab_value value) {
  enum gab_kind k = gab_valkind(value);
  switch (k) {
  /* These primitives have a runtime type of themselves */
  case kGAB_NIL:
  case kGAB_UNDEFINED:
    return value;
  /* Types are sepcial cases for the practical type */
  case kGAB_RECORD:
    return gab_recshp(value);
  case kGAB_BOX:
    return gab_boxtype(value);

  default:
    return gab_type(gab, k);
  }
}

static inline bool gab_valisa(struct gab_eg *eg, gab_value a, gab_value b) {
  return gab_valtype(eg, a) == b || a == b || gab_type(eg, gab_valkind(a)) == b;
}

#define NAME strings
#define K struct gab_obj_string *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define NAME shapes
#define K struct gab_obj_shape *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define NAME messages
#define K struct gab_obj_message *
#define HASH(a) (a->hash)
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

#define NAME gab_modules
#define K size_t
#define V a_gab_value *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

#define NAME gab_src
#define K gab_value
#define V struct gab_src *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

struct gab_eg {
  size_t hash_seed;

  d_gab_src sources;

  d_gab_modules modules;

  d_strings strings;

  d_shapes shapes;

  d_messages messages;

  gab_value types[kGAB_NKINDS];

  v_gab_value scratch;
};

static inline gab_value gab_type(struct gab_eg *gab, enum gab_kind k) {
  return gab->types[k];
}

static inline struct gab_egimpl_rest
gab_egimpl(struct gab_eg *eg, gab_value message, gab_value receiver) {
  size_t offset = GAB_PROPERTY_NOT_FOUND;
  gab_value type = gab_valtype(eg, receiver);

  if (gab_valhasp(receiver)) {
    offset = gab_recfind(receiver, gab_msgname(message));

    if (offset != GAB_PROPERTY_NOT_FOUND)
      return (struct gab_egimpl_rest){type, offset, sGAB_IMPL_PROPERTY};
  }

  if (gab_valhast(receiver)) {
    offset = gab_msgfind(message, type);

    if (offset != GAB_PROPERTY_NOT_FOUND)
      return (struct gab_egimpl_rest){type, offset, sGAB_IMPL_TYPE};
  }

  type = gab_type(eg, gab_valkind(receiver));
  offset = gab_msgfind(message, type);

  if (offset != GAB_PROPERTY_NOT_FOUND)
    return (struct gab_egimpl_rest){type, offset, sGAB_IMPL_KIND};

  type = gab_undefined;
  offset = gab_msgfind(message, type);

  return (struct gab_egimpl_rest){
      type, offset,
      offset == GAB_PROPERTY_NOT_FOUND ? sGAB_IMPL_NONE : sGAB_IMPL_GENERAL};
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
static inline gab_value gab_valintos(struct gab_triple gab, gab_value self) {
  char buffer[128];

  switch (gab_valkind(self)) {
  case kGAB_STRING:
    return self;
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
    snprintf(buffer, 128, "%lf", gab_valton(self));
    return gab_string(gab, buffer);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *o = GAB_VAL_TO_BLOCK(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(o->p);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(p->name);
    snprintf(buffer, 128, "<Block %s %p>", s->data, o);
    return gab_string(gab, buffer);
  }
  case kGAB_RECORD: {
    struct gab_obj_record *o = GAB_VAL_TO_RECORD(self);
    snprintf(buffer, 128, "<Record %p>", o);
    return gab_string(gab, buffer);
  }
  case kGAB_SHAPE: {
    struct gab_obj_shape *o = GAB_VAL_TO_SHAPE(self);
    snprintf(buffer, 128, "<Shape %p>", o);
    return gab_string(gab, buffer);
  }
  case kGAB_MESSAGE: {
    struct gab_obj_message *o = GAB_VAL_TO_MESSAGE(self);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(o->name);
    snprintf(buffer, 128, "&:%s", s->data);

    return gab_string(gab, buffer);
  }
  case kGAB_SPROTOTYPE:
  case kGAB_BPROTOTYPE: {
    struct gab_obj_prototype *o = GAB_VAL_TO_PROTOTYPE(self);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(o->name);
    snprintf(buffer, 128, "<Prototype %s>", s->data);

    return gab_string(gab, buffer);
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *o = GAB_VAL_TO_NATIVE(self);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(o->name);
    snprintf(buffer, 128, "<Native %s>", s->data);

    return gab_string(gab, buffer);
  }
  case kGAB_BOX: {
    struct gab_obj_box *o = GAB_VAL_TO_BOX(self);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(gab_valintos(gab, o->type));
    snprintf(buffer, 128, "<%s %p>", s->data, o);

    return gab_string(gab, buffer);
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *o = GAB_VAL_TO_SUSPENSE(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(o->p);
    struct gab_obj_string *s = GAB_VAL_TO_STRING(p->name);
    snprintf(buffer, 128, "<Suspense %s %p>", s->data, o);

    return gab_string(gab, buffer);
  }
  default:
    assert(false && "Unhandled type in gab_valtos");
    return gab_undefined;
  }
}

/**
 * # Coerce the given value to a string, and return a view into it.
 *
 *  @param gab The engine
 *
 *  @param value The value to convert.
 *
 *  @return A view into the string
 */
static inline const char *gab_valintocs(struct gab_triple gab,
                                        gab_value value) {
  gab_value str = gab_valintos(gab, value);

  struct gab_obj_string *obj = GAB_VAL_TO_STRING(str);

  return obj->data;
}

#include <printf.h>
int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args);

int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes);

#endif
