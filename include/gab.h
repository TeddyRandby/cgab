/**
 * @file
 * @brief A small, fast, and portable implementation of the gab programming
 * language
 */

#ifndef GAB_H
#define GAB_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "core.h"

#if defined(_WIN32) || defined(_WIN64) || defined(Wint32_t)
#define OS_UNIX 0
#else
#define OS_UNIX 1
#endif

#if cGAB_LIKELY
#define __gab_likely(x) (__builtin_expect(!!(x), 1))
#define __gab_unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define __gab_likely(x) (x)
#define __gab_unlikely(x) (x)
#endif

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

  Pointer bit set       Pointer data
  |                     |
 [1][....NaN....1][--][------------------------------------------------]

 Immediate values store a tag in the lowest three bits.

                   kGAB_STRING, kGAB_SIGIL, kGAB_UNDEFINED, kGAB_PRIMITIVE
                   |
 [0][....NaN....1][--][------------------------------------------------]


 'Primitives' are message specializtions which are implemented as an opcode in
 the vm. This covers things like '+' on numbers and strings, etc.

 The opcode is stored in the lowest byte

                   kPRIMITIVE                                   Opcode
                   |                                            |
 [0][....NaN....1][--]----------------------------------------[--------]

 Gab also employs a short string optimization. Lots of strings in a gab program
 are incredibly small, and incredibly common. values like '.some', '.none',
 '.ok', and even messages like '+' store a small string.

 We need to store the string's length, a null-terminator (for c-compatibility),
 and the string's data.

 Instead of storing the length of the string, we store the amount of bytes *not*
 used. Since there are a total of 5 bytes availble for storing string data, the
 remaining length is computed as 5 - strlen(str).

 We do this for a special case - when the string has length 5, the remaining
 length is 0. In this case, the byte which stores the remaining length *also*
 serves as the null-terminator for the string.

 This layout sneakily gives us an extra byte of storage in our small strings.

                  kSTRING Remaining Length                              <- Data
                     |    |                                                |
 [0][....NaN....1][---][--------][----------------------------------------]
                         [...0....][...e.......p.......a........h......s....]
                         [...3....][-------------------------...k......o....]

*/

#define T uint64_t
#define NAME gab_value
#include "array.h"

#define T uint64_t
#define NAME gab_value
#include "vector.h"

typedef uint64_t gab_value;

enum gab_kind {
  kGAB_STRING = 0, // MUST_STAY_ZERO
  kGAB_SIGIL = 1,
  kGAB_UNDEFINED = 2,
  kGAB_PRIMITIVE = 3,
  kGAB_PROTOTYPE,
  kGAB_MESSAGE,
  kGAB_NUMBER,
  kGAB_NATIVE,
  kGAB_RECORD,
  kGAB_SHAPE,
  kGAB_BLOCK,
  kGAB_BOX,
  kGAB_NKINDS,
};

#define __GAB_QNAN ((uint64_t)0x7ffc000000000000)

#define __GAB_SIGN_BIT ((uint64_t)1 << 63)

#define __GAB_TAGMASK (3)

#define __GAB_TAGOFFSET (48)

#define __GAB_TAGBITS ((uint64_t)__GAB_TAGMASK << __GAB_TAGOFFSET)

#define __GAB_VAL_TAG(val)                                                     \
  ((enum gab_kind)((__gab_valisn(val)                                          \
                        ? kGAB_NUMBER                                          \
                        : ((val) >> __GAB_TAGOFFSET) & __GAB_TAGMASK)))

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

#define __gab_valisb(val)                                                      \
  (gab_valeq(val, gab_true) || gab_valeq(val, gab_false))

#define __gab_obj(val)                                                         \
  (gab_value)(__GAB_SIGN_BIT | __GAB_QNAN | (uint64_t)(uintptr_t)(val))

#define gab_valiso(val)                                                        \
  (((val) & (__GAB_SIGN_BIT | __GAB_QNAN)) == (__GAB_SIGN_BIT | __GAB_QNAN))

#define gab_valisnew(val) (gab_valiso(val) && GAB_OBJ_IS_NEW(gab_valtoo(val)))

/*
 * The gab values true and false are implemented with sigils.
 *
 * These are simple gab strings, but serve as their own type, allowing
 * specialization on messages for sigils like true, false, ok, some, none, etc.
 */

#define gab_nil                                                                \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)2 << 40 | (uint64_t)'n' | (uint64_t)'i' << 8 |        \
               (uint64_t)'l' << 16))

#define gab_false                                                              \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)0 << 40 | (uint64_t)'f' | (uint64_t)'a' << 8 |        \
               (uint64_t)'l' << 16 | (uint64_t)'s' << 24 |                     \
               (uint64_t)'e' << 32))

#define gab_true                                                               \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)1 << 40 | (uint64_t)'t' | (uint64_t)'r' << 8 |        \
               (uint64_t)'u' << 16 | (uint64_t)'e' << 24))

#define gab_ok                                                                 \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)3 << 40 | (uint64_t)'o' | (uint64_t)'k' << 8))

#define gab_none                                                               \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)1 << 40 | (uint64_t)'n' | (uint64_t)'o' << 8 |        \
               (uint64_t)'n' << 16 | (uint64_t)'e' << 24))

#define gab_err                                                                \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET |          \
               (uint64_t)2 << 40 | (uint64_t)'e' | (uint64_t)'r' << 8 |        \
               (uint64_t)'r' << 16)

/* The gab value 'undefined'*/
#define gab_undefined                                                          \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_UNDEFINED << __GAB_TAGOFFSET))

/* Convert a c boolean into the corresponding gab value */
#define gab_bool(val) ((val) ? gab_true : gab_false)

/* Convert a c double into a gab value */
#define gab_number(val) (__gab_dtoval(val))

/* Create the gab value for a primitive operation */
#define gab_primitive(op)                                                      \
  ((gab_value)(__GAB_QNAN | ((uint64_t)kGAB_PRIMITIVE << __GAB_TAGOFFSET) |    \
               (op)))

/* Cast a gab value to a number */
#define gab_valton(val) (__gab_valtod(val))

/* Cast a gab value to the generic object pointer */
#define gab_valtoo(val)                                                        \
  ((struct gab_obj *)(uintptr_t)((val) & ~(__GAB_SIGN_BIT | __GAB_QNAN |       \
                                           __GAB_TAGBITS)))

/* Cast a gab value to a primitive operation */
#define gab_valtop(val) ((uint8_t)((val) & 0xff))

/* Convenience macro for getting arguments in builtins */
#define gab_arg(i) (i < argc ? argv[i] : gab_nil)

/* Convenience macro for comparing values */
#define gab_valeq(a, b) ((a) == (b))

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

#define NAME specs
#define K gab_value
#define V gab_value
#define DEF_V gab_undefined
#define HASH(a) (a)
#define EQUAL(a, b) (gab_valeq(a, b))
#include "dict.h"

#define NAME helps
#define K gab_value
#define V s_char
#define DEF_V ((s_char){0})
#define HASH(a) (a)
#define EQUAL(a, b) (gab_valeq(a, b))
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
struct gab_src;

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

typedef void (*gab_gcvisit_f)(struct gab_triple, struct gab_obj *obj);

typedef a_gab_value *(*gab_native_f)(struct gab_triple, size_t argc,
                                     gab_value argv[argc]);

typedef void (*gab_boxdestroy_f)(size_t len, char data[static len]);

typedef void (*gab_boxvisit_f)(struct gab_triple gab, gab_gcvisit_f visitor,
                               size_t len, char data[static len]);

/**
 * @class gab_obj
 * @brief This struct is the first member of all heap-allocated objects.
 *
 * All gab objects inherit (as far as c can do inheritance) from this struct.
 */
struct gab_obj {
  /**
   * @brief The number of live references to this object.
   *
   * If this number is overflowed, gab keeps track  of this object's references
   * in a separate, slower map<gab_obj, uint64_t>. When the reference count
   * drops back under 255, rc returns to the fast path.
   */
  int8_t references;
  /**
   * @brief Flags used by garbage collection and for debug information.
   */
#if cGAB_LOG_GC
  uint16_t flags;
#else
  uint8_t flags;
#endif
  /**
   * @brief a flag denoting the kind of object referenced by this pointer -
   * defines how to interpret the remaining bytes of this allocation.
   */
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
 * @brief Format the given string to the given stream.
 *
 * This format function does *not* respect the %-style formatters like printf.
 * The only supported formatter is $, which will use the next gab_value in the
 * var args.
 *
 * eg:
 * `gab_fprintf(stdout, "foo $", gab_string(gab, "bar"));`c
 *
 * @param stream The stream to print to
 * @param fmt The format string
 * @return the number of bytes written to the stream.
 */
int gab_fprintf(FILE *stream, const char *fmt, ...);

/**
 * @brief Format the given string to the given stream.
 * @see gab_fprintf
 *
 * @param stream The stream to print to
 * @param fmt The format string
 * @param varargs The arguments
 * @return the number of bytes written to the stream.
 */
int gab_vfprintf(FILE *stream, const char *fmt, va_list varargs);

/**
 * @brief Give the engine ownership of the values.
 *
 * When in c-code, it can be useful to create gab_objects which should be global
 * (ie, always kept alive).
 * Since gab_objects are allocated with one reference, this function does not
 * increment the rc of the value. It instead gives the engine 'ownership',
 * implying that when the engine is freed it should *decrement* the rc of the
 * value. The object will then be freed in the final collection, if its rc is <=
 * 0.
 *
 * @param eg The engine.
 * @param value The value to keep.
 * @return The number of values kept.
 */
size_t gab_egkeep(struct gab_eg *eg, gab_value value);

/**
 * @brief Give the engine ownership of multiple values.
 * @see gab_egkeep.
 *
 * @param gab The engine.
 * @param len The number of values to keep.
 * @param values The values.
 * @return The total number of values kept by the engine.
 */
size_t gab_negkeep(struct gab_eg *eg, size_t len, gab_value argv[static len]);

/**
 * @enum The possible result states of a call to gab_egimpl.
 * @see gab_egimpl_rest
 * @see gab_egimpl
 */
enum gab_egimpl_resk {
  kGAB_IMPL_NONE = 0,
  kGAB_IMPL_TYPE,
  kGAB_IMPL_KIND,
  kGAB_IMPL_GENERAL,
  kGAB_IMPL_PROPERTY,
};

/**
 * @class gab_egimpl_rest
 * @brief The result of gab_egimpl
 * @see gab_egimpl
 */
struct gab_egimpl_rest {
  /**
   * @brief The type of the relevant type of the receiver.
   *
   * gab_value have multiple types - a gab_record has its shape, as well as
   * the gab.record type, as well as the general gab_undefined type.
   *
   * These are checked in a specific order, and the first match is used.
   */
  gab_value type;

  /**
   * @brief The offset within the message of the implementation.
   *
   * For properties (see sGAB_IMPL_PROPERTY), this will return the offset within
   * the record.
   */
  size_t offset;

  /**
   * @brief The status of this implementation resolution.
   * @see gab_egimpl_resk
   */
  enum gab_egimpl_resk status;
};

/**
 * @brief Find the implementation of a message for a given receiver.
 *
 * @param eg The engine to search for an implementation.
 * @param message The message to find.
 * @param receiver The receiver to find the implementation for.
 * @return The result of the implementation search.
 */
static inline struct gab_egimpl_rest
gab_egimpl(struct gab_eg *eg, gab_value message, gab_value receiver);

/**
 * @brief Push a value onto the vm's internal stack.
 *
 * This is how c-natives return values to the runtime.
 *
 * @param vm The vm to push the values onto.
 * @param value the value to push.
 * @return The number of values pushed (always one), 0 on err.
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
void gab_fvminspect(FILE *stream, struct gab_vm *vm, int depth);

/**
 * @brief Inspect the vm's callstack at the given depth, returning a value with
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
 * @returns The module if it was added, nullptr otherwise.
 */
a_gab_value *gab_segmodput(struct gab_eg *eg, const char *name, gab_value mod,
                           size_t len, gab_value values[len]);

/**
 * @brief Check if an engine has a module by name.
 *
 * @param eg The engine.
 * @param name The name of the import.
 * @returns The module if it exists, nullptr otherwise.
 */
a_gab_value *gab_segmodat(struct gab_eg *eg, const char *name);

/**
 * @class gab_cmpl_argt
 * @brief Arguments and options for compiling a string of source code.
 * @see gab_cmpl.
 * @see enum gab_flags.
 */
struct gab_build_argt {
  /**
   * The name of the module, defaults to "__main__"
   */
  const char *name;
  /**
   * The source code to compile.
   */
  const char *source;
  /**
   * Optional flags for compilation.
   */
  int flags;
  /**
   * The number of arguments expected by the main block.
   */
  size_t len;
  /**
   * The names of the arguments expected by the main block.
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
gab_value gab_build(struct gab_triple gab, struct gab_build_argt args);

/**
 * @class gab_run_argt
 * @brief Arguments and options for running a block.
 * @see gab_run
 */
struct gab_run_argt {
  /**
   * The main block to run.
   */
  gab_value main;
  /**
   * Optional flags for the vm.
   */
  int flags;
  /**
   * The number of arguments passed to the main block.
   */
  size_t len;
  /**
   * The arguments passed to the main block.
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
   * The name of the module - defaults to "__main__".
   */
  const char *name;
  /**
   * The source code to execute.
   */
  const char *source;
  /**
   * Optional flags for compilation AND execution.
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
 * @brief Arguments and options for an interactive REPL.
 */
struct gab_repl_argt {
  /**
   * The prompt to display before each input of REPL.
   */
  const char *prompt_prefix;
  /**
   * The prefix to display before each result of REPL.
   */
  const char *result_prefix;
  /**
   * The name of the module - defaults to "__main__".
   */
  const char *name;
  /**
   * Optional flags for compilation AND execution.
   */
  int flags;
  /**
   * The number of arguments to the main block.
   */
  size_t len;
  /**
   * The names of the arguments to the main block.
   */
  const char **sargv;
  /**
   * The values of the arguments to the main block.
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
 * @brief Arguments for creating a specialization
 */
struct gab_spec_argt {
  /**
   * The name of the message to specialize on.
   */
  const char *name;
  /**
   * The reciever and value of the specialization.
   */
  gab_value receiver, specialization;
};
/**
 * @brief Create a specialization on the given message for the given receiver
 * @see struct gab_spec_argt
 *
 * @param gab The triple.
 * @param len The number of specializations to set.
 * @param args The specializations.
 * @return -1 on a success. Otherwise, returns the index in args of the first
 * specialization that failed.
 */
int gab_nspec(struct gab_triple gab, size_t len,
              struct gab_spec_argt args[static len]);

/**
 * @brief Create a specialization on the given message for the given receiver
 * @see struct gab_spec_argt
 *
 * @param gab The triple.
 * @param args The arguments.
 * @return The message that was updated, or gab_undefined if specialization
 * failed.
 */
gab_value gab_spec(struct gab_triple gab, struct gab_spec_argt args);

/**
 * @brief Deep copy a value.
 *
 * @param gab The engine.
 * @param value The value to copy.
 * @return The copy.
 */
gab_value gab_valcpy(struct gab_triple gab, gab_value value);

/**
 * @brief Get the runtime value that corresponds to the given kind.
 *
 * @param gab The engine
 * @param kind The type to retrieve the value for.
 * @return The runtime value corresponding to that type.
 */
static inline gab_value gab_egtype(struct gab_eg *eg, enum gab_kind kind);

/**
 * @brief If fGAB_SILENT is not set, print an error message to stderr.
 * If fGAB_EXIT_ON_PANIC is set, then exit the program.
 *
 * @param gab The triple.
 * @param fmt The format string.
 * @returns An array of gab_values.
 */
a_gab_value *gab_panic(struct gab_triple gab, const char *fmt, ...);

/**
 * @brief Panic the triple with an 'unexpected type' message.
 * @see gab_panic
 *
 * @param gab The triple.
 * @param found The value with the mismatched type.
 * @param texpected The expected type.
 * @returns An array of gab_values.
 */
a_gab_value *gab_ptypemismatch(struct gab_triple gab, gab_value found,
                               gab_value texpected);

/**
 * @brief Panic the triple with an 'unexpected type' message.
 * @see gab_panic
 *
 * @param gab The triple.
 * @param found The value with the mismatched type.
 * @param texpected The expected type.
 * @returns false.
 */
static inline a_gab_value *gab_pktypemismatch(struct gab_triple gab,
                                              gab_value found,
                                              enum gab_kind texpected) {
  return gab_ptypemismatch(gab, found, gab_egtype(gab.eg, texpected));
};

#if cGAB_LOG_GC

#define gab_iref(gab, val) (__gab_iref(gab, val, __FUNCTION__, __LINE__))

gab_value __gab_iref(struct gab_triple gab, gab_value val, const char *file,
                     int line);

#define gab_dref(gab, val) (__gab_dref(gab, val, __FUNCTION__, __LINE__))

gab_value __gab_dref(struct gab_triple gab, gab_value val, const char *file,
                     int line);

/**
 * # Increment the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void __gab_niref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len], const char *file, int line);
#define gab_niref(gab, stride, len, values)                                    \
  (__gab_niref(gab, stride, len, values, __FUNCTION__, __LINE__))

/**
 * # Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void __gab_ndref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len], const char *file, int line);
#define gab_ndref(gab, stride, len, values)                                    \
  (__gab_ndref(gab, stride, len, values, __FUNCTION__, __LINE__))

#else

/**
 * @brief Increment the reference count of the value(s)
 *
 * @param gab The gab triple.
 * @param value The value to increment.
 * @return the value incremented.
 */
gab_value gab_iref(struct gab_triple gab, gab_value value);

/**
 * @brief Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param len The number of values.
 * @param values The values.
 * @return the value decremented.
 */
gab_value gab_dref(struct gab_triple gab, gab_value value);

/**
 * @brief Increment the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void gab_niref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]);

/**
 * @brief Decrement the reference count of the value(s)
 *
 * @param vm The vm.
 * @param value The value.
 */
void gab_ndref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]);

#endif

/**
 * @brief Run the garbage collector.
 *
 * @param gab The engine.
 * @param gc The garbage collector.
 * @param vm The vm.
 */
void gab_collect(struct gab_triple gab);

/**
 * @brief Lock the garbage collector to prevent collection until gab_gcunlock is
 * called.
 * @see gab_gcunlock
 *
 * @param gc The gc to lock
 */
void gab_gclock(struct gab_gc *gc);

/**
 * @brief Unlock the given collector.
 *
 * @param gc The gc to unlock
 */
void gab_gcunlock(struct gab_gc *gc);

/**
 * @brief Get the name of a source file - aka the fully qualified path.
 *
 * @param src The source
 * @return The name
 */
gab_value gab_srcname(struct gab_src *src);

/**
 * @brief Get the line in the source code corresponding to an offset in the
 * bytecode.
 *
 * @param src The source
 * @param offset The offset
 * @return The line in the source code
 */
size_t gab_srcline(struct gab_src *src, size_t offset);

/**
 * @brief Get the kind of a value.
 * @see enum gab_kind
 *
 * This is not the **runtime type** of the value. For that, use
 * `gab_valtype`.
 *
 * @param value The value.
 * @return The kind of the value.
 */
static inline enum gab_kind gab_valkind(gab_value value) {
  if (gab_valiso(value))
    return gab_valtoo(value)->kind + __GAB_VAL_TAG(value);

  return __GAB_VAL_TAG(value);
}

/**
 * @brief Check if a value supports property-like message sends.
 *
 * @param value The value to check.
 * @return true if the value has properties.
 */
static inline bool gab_valhasp(gab_value value) {
  enum gab_kind k = gab_valkind(value);
  return k == kGAB_RECORD;
}

/**
 * @brief Check if a value has a type *other than its kind* to check for message
 * sends.
 *
 * @param value The value to check.
 * @return true if the value supports types other than its kind.
 */
static inline bool gab_valhast(gab_value value) {
  enum gab_kind k = gab_valkind(value);
  switch (k) {
  case kGAB_SIGIL:
  case kGAB_RECORD:
  case kGAB_BOX:
    return true;
  default:
    return false;
  }
}

/**
 * @brief An immutable sequence of bytes.
 */
struct gab_obj_string {
  struct gab_obj header;

  /**
   * A pre-computed hash of the bytes in 'data'.
   */
  uint64_t hash;

  /**
   * The number of bytes in 'data'
   */
  size_t len;

  /**
   * The data.
   */
  char data[];
};

/* Cast a value to a (gab_obj_string*) */
#define GAB_VAL_TO_STRING(value) ((struct gab_obj_string *)gab_valtoo(value))

/**
 * @brief Create a gab_value from a c-string
 *
 * @param gab The engine.
 * @param data The data.
 * @return The value.
 */
gab_value gab_string(struct gab_triple gab, const char data[static 1]);

/**
 * @brief Create a gab_value from a bounded array of chars.
 *
 * @param gab The engine.
 * @param len The length of the string.
 * @param data The data.
 * @return The value.
 */
gab_value gab_nstring(struct gab_triple gab, size_t len,
                      const char data[static len]);

/**
 * @brief Concatenate two gab strings
 *
 * @param gab The engine.
 * @param a The first string.
 * @param b The second string.
 * @return The value.
 */
gab_value gab_strcat(struct gab_triple gab, gab_value a, gab_value b);

/**
 * @brief Get the length of a string. This is constant-time.
 *
 * @param str The string.
 * @return The length of the string.
 */
static inline size_t gab_strlen(gab_value str) {
  assert(gab_valkind(str) == kGAB_STRING || gab_valkind(str) == kGAB_SIGIL);

  if (gab_valiso(str))
    return GAB_VAL_TO_STRING(str)->len;

  return 5 - ((str >> 40) & 0xFF);
};

/**
 * @brief Get a pointer to the start of the string.
 *
 * This accepts a pointer because of the short string optimization, where
 * the gab_value itself embeds the string data.
 *
 * @param str The string
 * @return A pointer to the start of the string
 */
static inline const char *gab_strdata(gab_value *str) {
  assert(gab_valkind(*str) == kGAB_STRING || gab_valkind(*str) == kGAB_SIGIL);

  if (gab_valiso(*str))
    return GAB_VAL_TO_STRING(*str)->data;

  return ((const char *)str);
}

/**
 * @brief Get a string's hash. This a constant-time.
 *
 * @param str The string
 * @return The hash
 */
static inline size_t gab_strhash(gab_value str) {
  assert(gab_valkind(str) == kGAB_STRING);

  if (gab_valiso(str))
    return GAB_VAL_TO_STRING(str)->hash;

  return str;
}

/**
 * @brief Convert a string into it's corresponding sigil. This is constant-time.
 *
 * @param str The string
 * @return The sigil
 */
static inline gab_value gab_strtosig(gab_value str) {
  assert(gab_valkind(str) == kGAB_STRING);

  return str | (uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET;
}

/**
 * @brief Create a sigil value from a cstring.
 *
 * @param data The cstring
 * @return The sigil
 */
static inline gab_value gab_sigil(struct gab_triple gab,
                                  const char data[static 1]) {
  return gab_strtosig(gab_string(gab, data));
}

/**
 * @brief Create a sigil value from a cstring.
 *
 * @param len The number of bytes in data
 * @param data The cstring
 * @return The sigil
 */
static inline gab_value gab_nsigil(struct gab_triple gab, size_t len,
                                   const char data[static len]) {
  return gab_strtosig(gab_nstring(gab, len, data));
}

/**
 * @brief Convert a sigil into it's corresponding string. This is constant-time.
 *
 * @param str The string
 * @return The sigil
 */
static inline gab_value gab_sigtostr(gab_value str) {
  assert(gab_valkind(str) == kGAB_SIGIL);

  return str & ~((uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET);
}

/**
 * @brief A wrapper for a native c function.
 */
struct gab_obj_native {
  struct gab_obj header;

  /**
   * The underlying native function.
   */
  gab_native_f function;

  /**
   * A name, not often useful.
   */
  gab_value name;
};

/* Cast a value to a (gab_obj_native*) */
#define GAB_VAL_TO_NATIVE(value) ((struct gab_obj_native *)gab_valtoo(value))

/**
 * @brief A block - aka a prototype and it's captures.
 */
struct gab_obj_block {
  struct gab_obj header;

  /**
   * The number of captured upvalues.
   */
  uint8_t nupvalues;

  /**
   * The prototype of the block.
   */
  gab_value p;

  /**
   * The captured values.
   */
  gab_value upvalues[];
};

/* Cast a value to a (gab_obj_block*) */
#define GAB_VAL_TO_BLOCK(value) ((struct gab_obj_block *)gab_valtoo(value))

/**
 * @brief Create a new block object, setting all captures to gab_nil.
 *
 * @param gab The gab engine.
 * @param prototype The prototype of the block.
 * @return The new block object.
 */
gab_value gab_block(struct gab_triple gab, gab_value prototype);

/**
 * @brief A shape object, used to define the layout of a record.
 */
struct gab_obj_shape {
  struct gab_obj header;

  /**
   * The pre-computed hash, and length of data.
   */
  uint64_t hash, len;

  /**
   * The ordered list of keys in the shape.
   */
  gab_value data[];
};

/* Cast a value to a (gab_obj_shape*) */
#define GAB_VAL_TO_SHAPE(v) ((struct gab_obj_shape *)gab_valtoo(v))

/**
 * @brief Create a new shape object.
 *
 * @param gab The gab triple.
 * @param stride The stride of the keys in they key array.
 * @param len The length of the shape.
 * @param keys The key array.
 * @returns The new shape.
 */
gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[static len]);

/**
 * @brief Create a new shape object.
 *
 * @param gab The gab triple.
 * @param stride The stride of the keys in they key array.
 * @param len The length of the shape.
 * @param keys The key array.
 */
gab_value gab_sshape(struct gab_triple gab, size_t stride, size_t len,
                     const char *keys[static len]);

/**
 * @brief Append a key to a shape, returning the new shape.
 *
 * @param gab The gab triple.
 * @param shape The shape to extend.
 * @param key The key to append.
 * @returns The new shape.
 */
gab_value gab_shapewith(struct gab_triple gab, gab_value shape, gab_value key);

/**
 * @brief Create a new shape for a tuple. The keys are increasing integers,
 * starting from 0.
 *
 * @param gab The gab engine.
 * @param vm The vm.
 * @param len The length of the tuple.
 * @return The new shape.
 */
gab_value gab_nshape(struct gab_triple gab, uint64_t len);

#define GAB_PROPERTY_NOT_FOUND (UINT64_MAX)

/**
 * @brief Find the offset of a key in the shape.
 *
 * @param obj The shape object.
 * @param key The key to look for.
 * @return The offset of the key, or GAB_PROPERTY_NOT_FOUND if it doesn't exist.
 */
static inline size_t gab_shpfind(gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  size_t res = GAB_PROPERTY_NOT_FOUND;

  // Linear search for the key in the shape
  // Instead of early return, prefer
  // This sort of result-storing strategy.
  // It is trivially vectorized by the compiler.
  for (size_t i = 0; i < obj->len; i++)
    if (obj->data[i] == key)
      res = i;

  return res;
};

/**
 * @brief Get the length of a shape. This is constant-time.
 *
 * @param value The shape.
 * @return The length of the shape.
 */
static inline size_t gab_shplen(gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  return obj->len;
};

/**
 * @brief Get a pointer to the keys of a shape
 *
 * @param value The shape.
 * @return The keys
 */
static inline gab_value *gab_shpdata(gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  return obj->data;
};

/**
 * @brief Get the offset of the next key in the shape.
 *
 * @param obj The shape object.
 * @param key The key to look for.
 * @return The offset of the next key, 0 if the key is gab_undefined, or
 * GAB_PROPERTY_NOT_FOUND if it doesn't exist.
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
 * @brief Get the value within the shape at the given offset.
 * This function is not bounds checked.
 *
 * @param shp The shape.
 * @param offset the offset.
 * @retunrs the value at the offset.
 */
static inline gab_value gab_ushpat(gab_value shp, uint64_t offset) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shp);

  assert(offset < obj->len);
  return obj->data[offset];
}

/**
 * @brief The counterpart to shape, which holds the data.
 */
struct gab_obj_record {
  struct gab_obj header;

  /**
   * The shape of the record.
   */
  gab_value shape;

  /**
   * The number of values in the record (duplicate from shape, but necessary)
   */
  uint64_t len;

  /**
   * The data.
   */
  gab_value data[];
};

/* Cast a value to a (gab_obj_record*) */
#define GAB_VAL_TO_RECORD(value) ((struct gab_obj_record *)gab_valtoo(value))

/**
 * @brief Create a new record object.
 *
 * @param gab The gab triple.
 * @param shape The shape of the record.
 * @param stride The stride of the values in the values array.
 * @param values The values array. The length is inferred from shape.
 *
 * @return The new record object.
 */
gab_value gab_recordof(struct gab_triple gab, gab_value shape, size_t stride,
                       gab_value values[static GAB_VAL_TO_SHAPE(shape)->len]);

/**
 * @brief extend a record with a key/value pair.
 *
 * @param gab The triple.
 * @param rec The record.
 * @param key The key.
 * @param value The value.
 * @return a new record, extended with key: value
 */
gab_value gab_recordwith(struct gab_triple gab, gab_value rec, gab_value key,
                         gab_value value);

/**
 * @brief Create a nil-initialized record of shape shape.
 *
 * @param gab The triple.
 * @param shape The shape of the record.
 * @return the new record.
 */
gab_value gab_erecordof(struct gab_triple gab, gab_value shape);

/**
 * @brief Set a value within the record.
 * WARNING: This function is not bounds checked. For safe alternatives,
 * try @link gab_recput or @link gab_srecput
 *
 * @see gab_recput
 * @see gab_srecput
 *
 * @param gab The triple.
 * @param rec The record.
 * @param offset The offset within the record.
 * @param value The new value to put into the record.
 * @returns the new value
 */
static inline gab_value gab_urecput(struct gab_triple gab, gab_value rec,
                                    uint64_t offset, gab_value value) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  assert(offset < obj->len);

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_dref(gab, obj->data[offset]);

  obj->data[offset] = value;

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_iref(gab, obj->data[offset]);

  return value;
}

/**
 * @brief Get the value within the record at the given offset.
 * WARNING: This function is not bounds checked. For safe alternatives,
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
 * @brief Get the value corresponding to the given key.
 *
 * @param obj The record object.
 * @param key The key.
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
 * @brief Get the offset of the given key in the record.
 *
 * @param rec The record object.
 * @param key The key.
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
 *
 * @param gab The triple.
 * @param rec The record object.
 * @param key The key, as a value.
 * @param value The value.
 * @return the new value if a success, gab_undefined otherwise.
 */
static inline gab_value gab_recput(struct gab_triple gab, gab_value rec,
                                   gab_value key, gab_value value) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  size_t offset = gab_shpfind(obj->shape, key);

  if (offset == GAB_PROPERTY_NOT_FOUND)
    return gab_undefined;

  return gab_urecput(gab, rec, offset, value);
}

/**
 * @brief Put a value in the record at the given key.
 *
 * @param gab The triple.
 * @param rec The record object.
 * @param key The key, as a c string literal.
 * @param value The value.
 * @return the new value if a success, gab_undefined otherwise.
 */
static inline gab_value gab_srecput(struct gab_triple gab, gab_value rec,
                                    const char *key, gab_value value) {
  return gab_recput(gab, rec, gab_string(gab, key), value);
}

/**
 * @brief Check if the record has a value at the given key.
 *
 * @param rec The record object.
 * @param key The key.
 * @return true if the key exists on the record, false otherwise.
 */
static inline bool gab_rechas(gab_value rec, gab_value key) {
  return gab_recat(rec, key) != gab_undefined;
}

/**
 * @brief Check if the record has a value at the given key.
 *
 * @param rec The record object.
 * @param key The key, as a c string literal.
 * @return true if the key exists on the record, false otherwise.
 */
static inline bool gab_srechas(struct gab_triple gab, gab_value rec,
                               const char *key) {
  return gab_rechas(rec, gab_string(gab, key));
}
/**
 * @brief Get the shape of a record.
 *
 * @param rec The record.
 * @return The shape of the record.
 */
static inline gab_value gab_recshp(gab_value rec) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(rec)->shape;
};

/**
 * @brief Get the length of a record.
 *
 * @param rec The record.
 * @return The length of the record.
 */
static inline size_t gab_reclen(gab_value rec) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(rec)->len;
};

/**
 * @brief Get a pointer to the members of a record.
 *
 * @param rec The record.
 * @return The members of the record.
 *
 */
static inline gab_value *gab_recdata(gab_value rec) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(rec)->data;
};

/**
 * @brief The message object, a collection of receivers and specializations,
 * under a name.
 */
struct gab_obj_message {
  struct gab_obj header;

  /**
   * A precomputed hash.
   */
  uint64_t hash;

  /**
   * The name of the message, and its specializations.
   */
  gab_value name, specs;
};

/* Cast a value to a (gab_obj_message*) */
#define GAB_VAL_TO_MESSAGE(value) ((struct gab_obj_message *)gab_valtoo(value))

/**
 * @brief Create a new message object.
 *
 * @param gab The gab engine.
 * @param name The name of the message.
 * @return The new message object.
 */
gab_value gab_message(struct gab_triple gab, gab_value name);

/**
 * @brief Get the shape of the message's spec record.
 *
 * @param msg The message.
 * @return the shape.
 */
static inline gab_value gab_msgshp(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return gab_recshp(GAB_VAL_TO_MESSAGE(msg)->specs);
}

/**
 * @brief Get a message's internal spec record.
 *
 * @param msg The message.
 * @return the record.
 */
static inline gab_value gab_msgrec(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return GAB_VAL_TO_MESSAGE(msg)->specs;
}

/**
 * @brief get the name of the message.
 *
 * @param msg The message.
 * @return the name.
 */
static inline gab_value gab_msgname(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return GAB_VAL_TO_MESSAGE(msg)->name;
}

/**
 * @brief Find the index of a receiver's specializtion in the message.
 *
 * @param msg The message object.
 * @param receiver The receiver to look for.
 * @return The index of the receiver's specialization, or GAB_PROPERTY_NOT_FOUND
 * if it doesn't exist.
 */
static inline size_t gab_msgfind(gab_value msg, gab_value receiver) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_recfind(obj->specs, receiver);
}

/**
 * @brief Get the specialization at the given offset in the message.
 * WARNING: This function is not bounds checked. For a safe alternative, try
 * gab_msgat.
 *
 * @see gab_msgat
 *
 * @param obj The message object.
 * @param offset The offset in the message.
 * @return The specialization at the offset.
 */
static inline gab_value gab_umsgat(gab_value msg, size_t offset) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_urecat(obj->specs, offset);
}

/**
 * @brief Get the specialization for a given receiver.
 *
 * @param msg The message object.
 * @param receiver The receiver.
 * @return The specialization for the receiver, or gab_undefined if it did
 * not exist.
 */
static inline gab_value gab_msgat(gab_value msg, gab_value receiver) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);
  return gab_recat(obj->specs, receiver);
}

/**
 * @brief Add a specialization to a message.
 *
 * @param gab The gab triple.
 * @param msg The message.
 * @param receiver The receiver.
 * @param spec The specialization.
 * @return The message, or gab_undefined if it already had the spec.
 */
static inline gab_value gab_msgput(struct gab_triple gab, gab_value msg,
                                   gab_value receiver, gab_value spec) {
  gab_gclock(gab.gc);

  assert(gab_valkind(msg) == kGAB_MESSAGE);

  struct gab_obj_message *obj = GAB_VAL_TO_MESSAGE(msg);

  if (gab_rechas(obj->specs, receiver)) {
    gab_gcunlock(gab.gc);
    return gab_undefined;
  }

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_dref(gab, obj->specs);

  obj->specs = gab_recordwith(gab, obj->specs, receiver, spec);

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)obj))
    gab_iref(gab, obj->specs);

  gab_gcunlock(gab.gc);

  return msg;
}

/**
 * @brief A container object, which holds arbitrary data.
 *
 * There are two callbacks:
 *  - one to do cleanup when the object is destroyed
 *  - one to visit children values when doing garbage collection.
 */
struct gab_obj_box {
  struct gab_obj header;

  /**
   * A callback called when the object is collected by the gc.
   *
   * This function should release all non-gab resources owned by the box.
   */
  gab_boxdestroy_f do_destroy;

  /**
   * A callback called when the object is visited during gc.
   *
   * This function should be used to visit all gab_values that are referenced by
   * the box.
   */
  gab_boxvisit_f do_visit;

  /**
   * The type of the box.
   */
  gab_value type;

  /**
   * The number of bytes in 'data'.
   */
  size_t len;

  /**
   * The data.
   */
  char data[];
};

#define GAB_VAL_TO_BOX(value) ((struct gab_obj_box *)gab_valtoo(value))

/**
 * @brief The prototype of a block. Encapsulates everything known about a block
 * at compile time.
 */
struct gab_obj_prototype {
  struct gab_obj header;

  /**
   * The source file this prototype is from.
   */
  struct gab_src *src;

  /**
   * The offset in the source's bytecode, and the length.
   */
  size_t offset, len;

  /**
   * The number of arguments, captures, slots (stack space) and locals.
   */
  unsigned char narguments, nupvalues, nslots, nlocals;

  /**
   * Flags providing additional metadata about the prototype.
   */
  char data[];
};

/* Cast a value to a (gab_obj_bprototype*) */
#define GAB_VAL_TO_PROTOTYPE(value)                                            \
  ((struct gab_obj_prototype *)gab_valtoo(value))

/**
 * @brief Arguments for creating a prototype.
 */
struct gab_prototype_argt {
  char narguments, nslots, nlocals, nupvalues, *flags, *indexes, *data;
};

/**
 * @brief Create a new prototype object.
 *
 * @param gab The gab engine.
 * @param src The source file.
 * @param begin The start of the bytecode.
 * @param end The end of the bytecode.
 * @param args The arguments.
 * @see struct gab_blkproto_argt
 * @return The new block prototype object.
 */
gab_value gab_prototype(struct gab_triple gab, struct gab_src *src,
                        size_t begin, size_t end,
                        struct gab_prototype_argt args);

/**
 * @brief Create a native wrapper to a c function with a gab_string name.
 *
 * @param gab The triple.
 * @param name The name of the native.
 * @param f The c function.
 *
 * @return The value.
 */
gab_value gab_native(struct gab_triple gab, gab_value name, gab_native_f f);

/**
 * @brief Create a native wrapper to a c function with a c-string name.
 *
 * @param gab The triple.
 * @param name The name of the native.
 * @param f The c function.
 * @return The value.
 */
gab_value gab_snative(struct gab_triple gab, const char *name, gab_native_f f);

/**
 * The arguments for creating a box.
 */
struct gab_box_argt {
  /**
   * The number of bytes in 'data'.
   */
  size_t size;
  /**
   * The user data.
   */
  void *data;
  /**
   * The type of the box.
   */
  gab_value type;
  /**
   * A callback called when the object is collected by the gc.
   */
  gab_boxdestroy_f destructor;
  /**
   * A callback called when the object is visited during gc.
   */
  gab_boxvisit_f visitor;
};

/**
 * @brief Create a gab value which wraps some user data.
 * @see struct gab_box_argt
 *
 * @param gab The engine.
 * @param args The arguments.
 *
 * @return The value.
 */
gab_value gab_box(struct gab_triple gab, struct gab_box_argt args);

/**
 * @brief Get the length of the user data from a boxed value.
 *
 * @param box The box.
 * @return The user data.
 */
static inline size_t gab_boxlen(gab_value box) {
  assert(gab_valkind(box) == kGAB_BOX);
  return GAB_VAL_TO_BOX(box)->len;
}

/**
 * @brief Get the user data from a boxed value.
 *
 * @param box The box.
 * @return The user data.
 */
static inline void *gab_boxdata(gab_value box) {
  assert(gab_valkind(box) == kGAB_BOX);
  return GAB_VAL_TO_BOX(box)->data;
}

/**
 * @brief Get the user type from a boxed value.
 *
 * @param box The box.
 * @return The user type.
 */
static inline gab_value gab_boxtype(gab_value value) {
  assert(gab_valkind(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->type;
}

/**
 * @brief Bundle a list of keys and values into a record.
 *
 * @param gab The triple.
 * @param len The length of keys and values arrays
 * @param keys The keys of the record to bundle.
 * @param values The values of the record to bundle.
 * @return The new record.
 */
gab_value gab_record(struct gab_triple gab, size_t len,
                     gab_value keys[static len], gab_value values[static len]);

/**
 * @brief Bundle a list of keys and values into a record.
 *
 * @param gab The triple.
 * @param len The length of keys and values arrays
 * @param keys The keys of the record to bundle, as c-strings.
 * @param values The values of the record to bundle.
 * @return The new record.
 */
gab_value gab_srecord(struct gab_triple gab, size_t len,
                      const char *keys[static len],
                      gab_value values[static len]);

/**
 * @brief Bundle a list of values into a tuple.
 *
 * @param gab The engine
 * @param len The length of values array.
 * @param values The values of the record to bundle.
 * @return The new record.
 */
gab_value gab_tuple(struct gab_triple gab, size_t len,
                    gab_value values[static len]);

/**
 * @brief Bundle a list of values into a tuple.
 *
 * @param gab The engine
 * @param len The length of values array.
 * @return The new record.
 */
gab_value gab_etuple(struct gab_triple gab, size_t len);

/**
 * @brief Get the practical runtime type of a value.
 *
 * @param gab The engine
 * @param value The value
 * @return The runtime value corresponding to the type of the given value
 */
static inline gab_value gab_valtype(struct gab_eg *gab, gab_value value) {
  enum gab_kind k = gab_valkind(value);
  switch (k) {
  /* These values have a runtime type of themselves */
  case kGAB_SIGIL:
  case kGAB_UNDEFINED:
    return value;
  /* These are special cases for the practical type */
  case kGAB_RECORD:
    return gab_recshp(value);
  case kGAB_BOX:
    return gab_boxtype(value);
  /* Otherwise, return the value for that kind */
  default:
    return gab_egtype(gab, k);
  }
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
#define DEF_V nullptr
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

#define NAME gab_src
#define K gab_value
#define V struct gab_src *
#define DEF_V nullptr
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

static inline gab_value gab_egtype(struct gab_eg *gab, enum gab_kind k) {
  return gab->types[k];
}

/**
 * @brief Check if a value's runtime type matches a given value.
 *
 * @param eg The engine
 * @param value The value
 * @param type The type
 * @return true if the value's type matches the given type.
 */
static inline bool gab_egvalisa(struct gab_eg *eg, gab_value value,
                                gab_value type) {
  return gab_valeq(gab_valtype(eg, value), type);
}

static inline struct gab_egimpl_rest
gab_egimpl(struct gab_eg *eg, gab_value message, gab_value receiver) {
  size_t offset = GAB_PROPERTY_NOT_FOUND;
  gab_value type = receiver;

  /* Check if the receiver supports properties, and if there is a matching
   * property */
  if (gab_valhasp(receiver)) {
    offset = gab_recfind(receiver, gab_msgname(message));

    if (offset != GAB_PROPERTY_NOT_FOUND)
      return (struct gab_egimpl_rest){type, offset, kGAB_IMPL_PROPERTY};
  }

  /* Check if the receiver has a supertype, and if that supertype implments the
   * message. ie <gab.shape 0 1>*/
  if (gab_valhast(receiver)) {
    type = gab_valtype(eg, receiver);
    offset = gab_msgfind(message, type);

    if (offset != GAB_PROPERTY_NOT_FOUND)
      return (struct gab_egimpl_rest){type, offset, kGAB_IMPL_TYPE};
  }

  /* Check for the kind of the receiver. ie 'gab.record' */
  type = gab_egtype(eg, gab_valkind(receiver));
  offset = gab_msgfind(message, type);

  if (offset != GAB_PROPERTY_NOT_FOUND)
    return (struct gab_egimpl_rest){type, offset, kGAB_IMPL_KIND};

  /* Lastly, check for a generic implmentation.*/
  type = gab_undefined;
  offset = gab_msgfind(message, type);

  return (struct gab_egimpl_rest){
      type, offset,
      offset == GAB_PROPERTY_NOT_FOUND ? kGAB_IMPL_NONE : kGAB_IMPL_GENERAL};
}

/**
 * @brief Coerce a value *into* a boolean.
 *
 * @param value The value to coerce.
 * @return False if the value is gab_false or gab_nil. Otherwise true.
 */
static inline bool gab_valintob(gab_value value) {
  return !(gab_valeq(value, gab_false) || gab_valeq(value, gab_nil));
}

/**
 * @brief Coerce a value *into* a c-string.
 *
 * @param gab The engine
 * @param value The value to coerce
 * @return The c-string
 */
static inline const char *gab_valintocs(struct gab_triple gab, gab_value value);

/**
 * @brief Coerce the given value to a string.
 *
 * @param gab The engine
 * @param value The value to convert
 * @return The string representation of the value.
 */
static inline gab_value gab_valintos(struct gab_triple gab, gab_value value) {
  char buffer[128];

  switch (gab_valkind(value)) {
  case kGAB_SIGIL:
  case kGAB_STRING:
    return value;
  case kGAB_UNDEFINED:
    return gab_string(gab, "undefined");
  case kGAB_PRIMITIVE:
    return gab_string(gab, gab_opcode_names[gab_valtop(value)]);
  case kGAB_NUMBER: {
    snprintf(buffer, 128, "%lf", gab_valton(value));
    return gab_string(gab, buffer);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *o = GAB_VAL_TO_BLOCK(value);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(o->p);
    snprintf(buffer, 128, "<Block %s:%lu>",
             gab_valintocs(gab, gab_srcname(p->src)),
             gab_srcline(p->src, p->offset));
    return gab_string(gab, buffer);
  }
  case kGAB_RECORD: {
    struct gab_obj_record *o = GAB_VAL_TO_RECORD(value);
    snprintf(buffer, 128, "<Record %p>", o);
    return gab_string(gab, buffer);
  }
  case kGAB_SHAPE: {
    struct gab_obj_shape *o = GAB_VAL_TO_SHAPE(value);
    snprintf(buffer, 128, "<Shape %p>", o);
    return gab_string(gab, buffer);
  }
  case kGAB_MESSAGE: {
    struct gab_obj_message *o = GAB_VAL_TO_MESSAGE(value);
    snprintf(buffer, 128, "\\%s", gab_valintocs(gab, o->name));

    return gab_string(gab, buffer);
  }
  case kGAB_PROTOTYPE: {
    struct gab_obj_prototype *o = GAB_VAL_TO_PROTOTYPE(value);
    snprintf(buffer, 128, "<Prototype %s:%lu>",
             gab_valintocs(gab, gab_srcname(o->src)),
             gab_srcline(o->src, o->offset));

    return gab_string(gab, buffer);
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *o = GAB_VAL_TO_NATIVE(value);
    snprintf(buffer, 128, "<Native %s>", gab_valintocs(gab, o->name));

    return gab_string(gab, buffer);
  }
  case kGAB_BOX: {
    struct gab_obj_box *o = GAB_VAL_TO_BOX(value);
    snprintf(buffer, 128, "<%s %p>", gab_valintocs(gab, o->type), o);

    return gab_string(gab, buffer);
  }
  default:
    assert(false && "Unhandled type in gab_valtos");
    return gab_undefined;
  }
}

static inline const char *gab_valintocs(struct gab_triple gab,
                                        gab_value value) {
  gab_value str = gab_valintos(gab, value);

  if (gab_valiso(str))
    return GAB_VAL_TO_STRING(str)->data;

  static char buffer[8]; // hacky
  memcpy(buffer, gab_strdata(&str), gab_strlen(str) + 1);

  return buffer;
}

#include <printf.h>
int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args);

int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes);

#endif
