/**
 * @file
 * @brief A small, fast, and portable implementation of the gab programming
 * language
 */

#ifndef GAB_H
#define GAB_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "core.h"

#if defined(_WIN32) || defined(_WIN64) || defined(Wint32_t)
#define GAB_OS_UNIX 0
#else
#define GAB_OS_UNIX 1
#endif

#if cGAB_LIKELY
#define __gab_likely(x) (__builtin_expect(!!(x), 1))
#define __gab_unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define __gab_likely(x) (x)
#define __gab_unlikely(x) (x)
#endif

/**
 *===============================*
 |                               |
 |     Value Representation      |
 |                               |
 *===============================*

 Gab values are nan-boxed.

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
 Even 64-bit machines only actually use 48 bits for addresses.

  Pointer bit set       Pointer data
  |                     |
 [1][....NaN....1][---------------------------------------------------]

 Immediate values *don't* have the pointer bit set.
 They also store a tag in the 3 bits just below the NaN.

                   kGAB_STRING, kGAB_SIGIL, kGAB_MESSAGE, kGAB_UNDEFINED,
 kGAB_PRIMITIVE
                   |
 [0][....NaN....1][---][------------------------------------------------]

 'Primitives' are message specializtions which are implemented as an opcode in
 the vm. This covers things like '+' on numbers and strings, etc.

 The opcode is stored in the lowest byte

                   kPRIMITIVE                                   Opcode
                   |                                            |
 [0][....NaN....1][---]----------------------------------------[--------]

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

                kSTRING Remaining Length                             <- Data
                   |    |                                               |
 [0][....NaN....1][---][--------][----------------------------------------]
                       [...0....][...e.......p.......a........h......s....]
                       [...3....][-------------------------...k......o....]

*/

typedef uint64_t gab_value;

#define T gab_value
#include "array.h"

#define T gab_value
#include "vector.h"

enum gab_kind {
  kGAB_STRING = 0, // MUST_STAY_ZERO
  kGAB_SIGIL = 1,
  kGAB_MESSAGE = 2,
  kGAB_PRIMITIVE = 3,
  kGAB_UNDEFINED = 4,
  kGAB_NUMBER,
  kGAB_NATIVE,
  kGAB_PROTOTYPE,
  kGAB_BLOCK,
  kGAB_BOX,
  kGAB_RECORD,
  kGAB_RECORDNODE,
  kGAB_SHAPE,
  kGAB_FIBER,
  kGAB_CHANNEL,
  kGAB_CHANNELCLOSED,
  kGAB_CHANNELBUFFERED,
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

#define gab_undefined                                                          \
  ((gab_value)(__GAB_QNAN | (uint64_t)kGAB_UNDEFINED << __GAB_TAGOFFSET))

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

#if cGAB_LOG_GC
/* Cast a gab value to the generic object pointer */
#define gab_valtoo(val)                                                        \
  ({                                                                           \
    struct gab_obj *__o =                                                      \
        (struct gab_obj *)(uintptr_t)((val) & ~(__GAB_SIGN_BIT | __GAB_QNAN |  \
                                                __GAB_TAGBITS));               \
    if (GAB_OBJ_IS_FREED(__o)) {                                               \
      printf("UAF\t%p\t%s:%i", __o, __FUNCTION__, __LINE__);                   \
      exit(1);                                                                 \
    }                                                                          \
    __o;                                                                       \
  })
#else
/* Cast a gab value to the generic object pointer */
#define gab_valtoo(val)                                                        \
  ((struct gab_obj *)(uintptr_t)((val) & ~(__GAB_SIGN_BIT | __GAB_QNAN |       \
                                           __GAB_TAGBITS)))
#endif

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
#define fGAB_OBJ_BUFFERED (1 << 6)
#define fGAB_OBJ_NEW (1 << 7)
#define fGAB_OBJ_FREED (1 << 8) // Used for debug purposes

#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & fGAB_OBJ_BUFFERED)
#define GAB_OBJ_IS_NEW(obj) ((obj)->flags & fGAB_OBJ_NEW)
#define GAB_OBJ_IS_FREED(obj) ((obj)->flags & fGAB_OBJ_FREED)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags |= fGAB_OBJ_BUFFERED)
#define GAB_OBJ_NEW(obj) ((obj)->flags |= fGAB_OBJ_NEW)
#define GAB_OBJ_FREED(obj) ((obj)->flags |= fGAB_OBJ_FREED)

#define __KEEP_FLAGS (fGAB_OBJ_BUFFERED | fGAB_OBJ_NEW | fGAB_OBJ_FREED)

#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~fGAB_OBJ_BUFFERED)
#define GAB_OBJ_NOT_NEW(obj) ((obj)->flags &= ~fGAB_OBJ_NEW)

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
  int32_t flags;
  int32_t wkid;
};

struct gab_obj;

struct gab_obj_string;
struct gab_obj_prototype;
struct gab_obj_native;
struct gab_obj_block;
struct gab_obj_message;
struct gab_obj_box;
struct gab_obj_fiber;

typedef void (*gab_gcvisit_f)(struct gab_triple, struct gab_obj *obj);

typedef a_gab_value *(*gab_native_f)(struct gab_triple, size_t argc,
                                     gab_value argv[argc]);

typedef void (*gab_boxdestroy_f)(size_t len, char data[static len]);

typedef void (*gab_boxcopy_f)(size_t len, char data[static len]);

typedef void (*gab_boxvisit_f)(struct gab_triple gab, gab_gcvisit_f visitor,
                               size_t len, char data[static len]);

/**
 * @class gab_obj
 * @brief This struct is the first member of all heap-allocated objects.
 *
 * All gab objects inherit (as far as c can do inheritance) from this struct.
 */
struct gab_obj {
  void *chunk;
  /**
   * @brief The number of live references to this object.
   *
   * If this number is overflowed, gab keeps track  of this object's references
   * in a separate, slower rec<gab_obj, uint64_t>. When the reference count
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
 * @brief Return the size of the object's allocation, in bytes.
 *
 * @param obj The object.
 */
size_t gab_obj_size(struct gab_obj *obj);

/**
 * @class gab_create_argt
 */
struct gab_create_argt {
  size_t flags, jobs;
  /**
   * @brief A hook for loading dynamic libraries.
   * This is used to load native-c modules.
   */
  void *(*os_dynopen)(const char *path);
  /**
   * @brief A hook for pulling symbols out of dynamically loaded libraries.
   */
  void *(*os_dynsymbol)(void *os_dynhandle, const char *path);
  /**
   * @brief A hook for allocating gab objects. A default is used if this is not
   * provided.
   */
  struct gab_obj *(*os_objalloc)(struct gab_triple gab, struct gab_obj *,
                                 size_t new_size);
};

/**
 * @brief Allocate data necessary for a runtime. This struct is lightweight and
 * should be passed by value.
 */
struct gab_triple gab_create(struct gab_create_argt args);

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
 * @brief Format the given string to the given stream.
 * @see gab_fprintf
 *
 * @param stream The stream to print to
 * @param fmt The format string
 * @param argc The number of arguments
 * @param argv The arguments
 * @return the number of bytes written to the stream.
 */
int gab_afprintf(FILE *stream, const char *fmt, size_t argc,
                 gab_value argv[argc]);

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
  gab_value spec;

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
 *
 */
static inline gab_value gab_egmsgput(struct gab_triple gab, gab_value message,
                                     gab_value type, gab_value specialization);

/**
 *
 */
static inline gab_value gab_egmsgat(struct gab_eg *eg, gab_value message,
                                    gab_value receiver);

/**
 *
 */
static inline gab_value gab_egmsgrec(struct gab_eg *eg, gab_value message);

/**
 * @brief Push any number of value onto the vm's internal stack.
 *
 * This is how c-natives return values to the runtime.
 *
 * @param vm The vm to push the values onto.
 * @param value the value to push.
 * @return The number of values pushed (always one), 0 on err.
 */
#define gab_vmpush(vm, ...)                                                    \
  ({                                                                           \
    gab_value _args[] = {__VA_ARGS__};                                         \
    gab_nvmpush(vm, sizeof(_args) / sizeof(gab_value), _args);                 \
  })

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

/*
 * @brief Attempt to load the module named 'name', with the default semantics of
 * \use
 *
 * @param gab The triple
 * @param name The name of the module
 * @return The values returned by the module, or nullptr if the module was not
 * found.
 */
a_gab_value *gab_suse(struct gab_triple gab, const char *name);

/*
 * @param gab The triple
 * @param name The name of the module, as a gab_value
 * @return The values returned by the module, or nullptr if the module was not
 * found.
 */
a_gab_value *gab_use(struct gab_triple gab, gab_value name);

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
 * @brief Call a block. Under the hood, this creates and queues a fiber, then
 * block the caller until that fiber is completed.
 * @see struct gab_run_argt
 *
 * @param  gab The triple.
 * @param args The arguments.
 * @return A heap-allocated slice of values returned by the fiber.
 */
a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args);

/**
 * @brief Asynchronously call a block. This will create and queue a fiber,
 * returning a handle to the fiber.
 * @see struct gab_run_argt
 *
 * @param  gab The triple.
 * @param args The arguments.
 * @return The fiber that was queued.
 */
gab_value gab_arun(struct gab_triple gab, struct gab_run_argt args);

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
 * @brief Compile a source string to a block and run it asynchronously.
 * This is equivalent to calling @link gab_cmpl and then @link gab_arun on the
 * result.
 *
 * @see struct gab_exec_argt
 *
 * @param gab The triple.
 * @param args The arguments.
 * @return A heap-allocated slice of values returned by the block.
 */
struct gab_fb *gab_aexec(struct gab_triple gab, struct gab_exec_argt args);

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
void gab_gclock(struct gab_triple gab);

/**
 * @brief Unlock the given collector.
 *
 * @param gc The gc to unlock
 */
void gab_gcunlock(struct gab_triple gab);

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
  case kGAB_BOX:
  case kGAB_RECORD:
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
  assert(gab_valkind(*str) == kGAB_STRING || gab_valkind(*str) == kGAB_SIGIL ||
         gab_valkind(*str) == kGAB_MESSAGE);

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
static inline gab_value gab_sigtostr(gab_value sigil) {
  assert(gab_valkind(sigil) == kGAB_SIGIL);
  return sigil & ~((uint64_t)kGAB_SIGIL << __GAB_TAGOFFSET);
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

/*
 * @brief A lightweight green-thread / coroutine / fiber.
 *
 * Execution can be paused and resumed.
 */
struct gab_obj_fiber {
  struct gab_obj header;

  enum {
    kGAB_FIBER_WAITING,
    kGAB_FIBER_RUNNING,
    kGAB_FIBER_DONE,
  } status;

  struct gab_vm {
    uint8_t *ip;

    gab_value *sp, *fp;

    gab_value sb[cGAB_STACK_MAX];
  } vm;

  a_gab_value *res;

  size_t len;

  gab_value data[];
};

gab_value gab_fiber(struct gab_triple gab, gab_value main, size_t argc,
                    gab_value argv[argc]);

/**
 * @brief Block the caller until this fiber is completed.
 *
 * @param fiber The fiber
 */
a_gab_value *gab_fibawait(gab_value fiber);

/* Cast a value to a (gab_obj_native*) */
#define GAB_VAL_TO_FIBER(value) ((struct gab_obj_fiber *)gab_valtoo(value))

struct gab_obj_channel {
  struct gab_obj header;

  mtx_t mtx;
  cnd_t t_cnd;
  cnd_t p_cnd;

  size_t cap;
  _Atomic size_t len;
  gab_value data[];
};

gab_value gab_channel(struct gab_triple gab, size_t cap);

void gab_chnput(struct gab_triple gab, gab_value channel, gab_value value);

gab_value gab_chntake(struct gab_triple gab, gab_value channel);

void gab_chnclose(gab_value channel);

bool gab_chnisclosed(gab_value channel);

/* Cast a value to a (gab_obj_channel*) */
#define GAB_VAL_TO_CHANNEL(value) ((struct gab_obj_channel *)gab_valtoo(value))

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

struct gab_obj_shape {
  struct gab_obj header;

  size_t len;

  v_gab_value transitions;

  gab_value keys[];
};

#define GAB_VAL_TO_SHAPE(value) ((struct gab_obj_shape *)gab_valtoo(value))
gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[static len]);

gab_value __gab_shape(struct gab_triple gab, size_t len);

static inline size_t gab_shplen(gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);
  return s->len;
}

static inline gab_value gab_ushpat(gab_value shp, size_t idx) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);
  return s->keys[idx];
}

static inline size_t gab_shpfind(gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);

  for (size_t i = 0; i < s->len; i++) {
    if (gab_valeq(key, s->keys[i]))
      return i;
  }

  return -1;
}

static inline size_t gab_shptfind(gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);

  for (size_t i = 0; i < s->transitions.len / 2; i++) {
    if (gab_valeq(key, v_gab_value_val_at(&s->transitions, i * 2)))
      return i;
  }

  return -1;
};

gab_value gab_shpwith(struct gab_triple gab, gab_value shp, gab_value key);

gab_value gab_shpwithout(struct gab_triple gab, gab_value shp, gab_value key);

struct gab_obj_recnode {
  struct gab_obj header;

  /*
   * Masks for describing 'data'
   */
  size_t mask, vmask;

  /*
   * Dense array of children. Each child has 2 slots in the array.
   * This optimizes for the common case of a node being a 'leaf'.
   * Instead of separately allocating the leaves as nodes,
   * they are stored directly in this array as [K | V].
   * The pairs which are leaves are denoted in the mask 'vmask'.
   */
  gab_value data[];
};

/*
 * The gab_obj_rec serves as the root of a HAMT. All of the children are
 * recnodes, and may be leaves or branches.
 */
struct gab_obj_rec {
  struct gab_obj header;

  size_t mask, vmask;

  gab_value shape;

  gab_value data[];
};

#define GAB_VAL_TO_REC(value) ((struct gab_obj_rec *)gab_valtoo(value))
#define GAB_VAL_TO_RECNODE(value) ((struct gab_obj_recnode *)gab_valtoo(value))

gab_value gab_record(struct gab_triple gab, size_t stride, size_t len,
                     gab_value keys[static len], gab_value vals[static len]);

gab_value __gab_record(struct gab_triple gab, size_t len, size_t space,
                       gab_value *data);

gab_value __gab_recordnode(struct gab_triple gab, size_t len, size_t space,
                           gab_value *data);

bool gab_rechas(gab_value rec, gab_value key);
bool gab_urechas(gab_value rec, size_t i);

gab_value gab_recat(gab_value rec, gab_value key);
gab_value gab_srecat(struct gab_triple, gab_value rec, const char *key);

gab_value gab_ukrecat(gab_value rec, size_t i);
gab_value gab_uvrecat(gab_value rec, size_t i);

gab_value gab_recput(struct gab_triple gab, gab_value rec, gab_value key,
                     gab_value val);

gab_value gab_recdel(struct gab_triple gab, gab_value rec, gab_value key);

static inline gab_value gab_recshp(gab_value rec) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  return GAB_VAL_TO_REC(rec)->shape;
};

size_t gab_reclen(gab_value rec);

static inline size_t gab_recfind(gab_value rec, gab_value key) {
  return gab_shpfind(gab_recshp(rec), key);
}

/**
 * @brief Convert a string into it's corresponding message. This is
 * constant-time.
 *
 * @param str The string
 * @return The message
 */
static inline gab_value gab_strtomsg(gab_value str) {
  assert(gab_valkind(str) == kGAB_STRING);

  return str | (uint64_t)kGAB_MESSAGE << __GAB_TAGOFFSET;
}

/**
 * @brief Create a new message object.
 *
 * @param gab The gab engine.
 * @param name The name of the message.
 * @return The new message object.
 */
static inline gab_value gab_message(struct gab_triple gab,
                                    const char data[static 1]) {
  return gab_strtomsg(gab_string(gab, data));
}

/**
 * @brief get the name of the message.
 *
 * @param msg The message.
 * @return the name.
 */
static inline gab_value gab_msgtostr(gab_value msg) {
  assert(gab_valkind(msg) == kGAB_MESSAGE);
  return msg & ~((uint64_t)kGAB_MESSAGE << __GAB_TAGOFFSET);
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
   * A callback called when the object is copied by `gab_valcpy`
   */
  gab_boxcopy_f do_copy;

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
   * A callback called when the object is copied by `gab_valcpy`.
   */
  gab_boxcopy_f copier;
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
 * @brief Get the practical runtime type of a value.
 *
 * @param gab The engine
 * @param value The value
 * @return The runtime value corresponding to the type of the given value
 */
static inline gab_value gab_valtype(struct gab_eg *gab, gab_value value) {
  enum gab_kind k = gab_valkind(value);
#if cGAB_LOG_GC
  assert(!gab_valiso(value) || !(gab_valtoo(value)->flags & fGAB_OBJ_FREED));
#endif
  switch (k) {
  /* These values have a runtime type of themselves */
  case kGAB_SIGIL:
    return value;
  /* These are special cases for the practical type */
  case kGAB_BOX:
    return gab_boxtype(value);
  case kGAB_RECORD:
    return gab_recshp(value);
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

#define T struct gab_obj *
#define NAME gab_obj
#include "vector.h"

#define NAME gab_obj
#define K struct gab_obj *
#define V size_t
#define HASH(a) ((intptr_t)a)
#define EQUAL(a, b) (a == b)
#define DEF_V (UINT8_MAX)
#include "dict.h"

enum {
  kGAB_BUF_STK = 0,
  kGAB_BUF_INC = 1,
  kGAB_BUF_DEC = 2,
  kGAB_NBUF = 3,
};

#define GAB_GCNEPOCHS 3
/**
 * @class The 'engine'. Stores the long-lived data
 * needed for the gab environment.
 */
struct gab_eg {
  size_t hash_seed;

  v_gab_value scratch;

  gab_value types[kGAB_NKINDS];

  _Atomic int8_t njobs;

  struct gab_gc {
    _Atomic int8_t schedule;
    d_gab_obj overflow_rc;
    v_gab_obj dead;

    struct gab_gcbuf {
      size_t len;
      struct gab_obj *data[cGAB_GC_DEC_BUFF_MAX];
    } buffers[][kGAB_NBUF][GAB_GCNEPOCHS];
  } *gc;

  _Atomic gab_value messages;
  gab_value work_channel;

  mtx_t shapes_mtx;
  gab_value shapes;

  mtx_t strings_mtx;
  d_strings strings;

  mtx_t sources_mtx;
  d_gab_src sources;

  mtx_t modules_mtx;
  d_gab_modules modules;

  void *(*os_dynopen)(const char *path);

  void *(*os_dynsymbol)(void *os_dynhandle, const char *path);

  struct gab_obj *(*os_objalloc)(struct gab_triple gab, struct gab_obj *,
                                 size_t new_size);

  size_t len;
  struct gab_jb {
    thrd_t td;

    size_t pid;

    gab_value fiber;

    _Atomic uint32_t epoch;
    _Atomic int32_t locked;
    v_gab_obj lock_keep;
  } jobs[];
};

static inline gab_value gab_egtype(struct gab_eg *gab, enum gab_kind k) {
  return gab->types[k];
}

static inline struct gab_gc *gab_gc(struct gab_triple gab) {
  return gab.eg->gc;
}

static inline gab_value gab_fb(struct gab_triple gab) {
  return gab.eg->jobs[gab.wkid].fiber;
}

static inline struct gab_vm *gab_vm(struct gab_triple gab) {
  gab_value fiber = gab.eg->jobs[gab.wkid].fiber;

  if (fiber == gab_undefined)
    return nullptr;

  return &GAB_VAL_TO_FIBER(fiber)->vm;
}

void gab_yield(struct gab_triple gab);

/**
 * @brief Check if a value's runtime id matches a given value.
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

/**
 * @brief Get a message's internal spec record.
 *
 * @param msg The message.
 * @return the record.
 */
static inline gab_value gab_egmsgrec(struct gab_eg *eg, gab_value msg) {
  gab_value specs = gab_recat(eg->messages, msg);
  return specs;
}

/**
 * @brief Get the specialization for a given receiver.
 *
 * @param msg The message object.
 * @param receiver The receiver.
 * @return The specialization for the receiver, or gab_undefined if it did
 * not exist.
 */
static inline gab_value gab_egmsgat(struct gab_eg *eg, gab_value msg,
                                    gab_value receiver) {
  gab_value specs = gab_recat(eg->messages, msg);

  if (specs == gab_undefined)
    return specs;

  return gab_recat(specs, receiver);
};

/**
 * @brief Add a specialization to a message.
 *
 * @param gab The gab triple.
 * @param msg The message.
 * @param receiver The receiver.
 * @param spec The specialization.
 * @return The message, or gab_undefined if it already had the spec.
 */
static inline gab_value gab_egmsgput(struct gab_triple gab, gab_value msg,
                                     gab_value receiver, gab_value spec) {
  gab_value specs = gab_recat(gab.eg->messages, msg);

  gab_gclock(gab);

  if (specs == gab_undefined) {
    specs = gab_record(gab, 0, 0, &specs, &specs);
  }

  gab_value newspecs = gab_recput(gab, specs, receiver, spec);
  gab.eg->messages = gab_recput(gab, gab.eg->messages, msg, newspecs);

  gab_gcunlock(gab);
  return msg;
}

static inline struct gab_egimpl_rest
gab_egimpl(struct gab_eg *eg, gab_value message, gab_value receiver) {
  gab_value spec = gab_undefined;
  gab_value type = receiver;

  if (gab_valkind(receiver) == kGAB_RECORD) {
    spec = gab_recat(receiver, (message));
    type = gab_recshp(type);
    if (spec != gab_undefined)
      return (struct gab_egimpl_rest){type, spec, kGAB_IMPL_PROPERTY};
  }

  /* Check if the receiver has a supertype, and if that supertype implments the
   * message. ie <gab.shape 0 1>*/
  if (gab_valhast(receiver)) {
    type = gab_valtype(eg, receiver);
    spec = gab_egmsgat(eg, message, type);

    if (spec != gab_undefined)
      return (struct gab_egimpl_rest){type, spec, kGAB_IMPL_TYPE};
  }

  /* Check for the kind of the receiver. ie 'gab.record' */
  type = gab_egtype(eg, gab_valkind(receiver));
  spec = gab_egmsgat(eg, message, type);

  if (spec != gab_undefined)
    return (struct gab_egimpl_rest){type, spec, kGAB_IMPL_KIND};

  /* Lastly, check for a generic implmentation.*/
  type = gab_undefined;
  spec = gab_egmsgat(eg, message, type);

  return (struct gab_egimpl_rest){
      type, spec, spec == gab_undefined ? kGAB_IMPL_NONE : kGAB_IMPL_GENERAL};
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
    return gab_sigtostr(value);
  case kGAB_MESSAGE:
    return gab_msgtostr(value);
  case kGAB_STRING:
    return value;
  case kGAB_PRIMITIVE:
    return gab_string(gab, gab_opcode_names[gab_valtop(value)]);
  case kGAB_NUMBER: {
    snprintf(buffer, 128, "%lf", gab_valton(value));
    return gab_string(gab, buffer);
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(value);
    snprintf(buffer, 128, "<gab.record %p>", m);
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
