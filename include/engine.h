#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "gab.h"

#include <stdarg.h>

#ifdef STATUS_NAMES
static const char *gab_status_names[] = {
#define STATUS(name, message) message,
#include "status_code.h"
#undef STATUS
};
#undef STATUS_NAMES
#endif

#ifdef TOKEN_NAMES
static const char *gab_token_names[] = {
#define TOKEN(message) #message,
#include "token.h"
#undef TOKEN
};
#undef TOKEN_NAMES
#endif

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

#define NAME gab_imp
#define K uint64_t
#define V struct gab_imp *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

#define T struct gab_obj *
#define NAME gab_obj
#include "vector.h"

struct gab_gc {
  v_gab_obj decrements;
  v_gab_obj modifications;
  size_t nreserve;
  bool running;
};

void gab_gccreate(struct gab_gc *gc);
void gab_gcdestroy(struct gab_gc *gc);

typedef void (*gab_gc_visitor)(struct gab_triple gab, struct gab_obj *obj);

enum variable_flag {
  fVAR_CAPTURED = 1 << 0,
  fVAR_MUTABLE = 1 << 1,
  fVAR_LOCAL = 1 << 2,
};

/**
 * The run-time representation of a callframe.
 */
struct gab_vm_frame {
  struct gab_obj_block *b;

  /**
   *The instruction pointer.
   */
  uint8_t *ip;

  /**
   * The value on the stack where this callframe begins.
   */
  gab_value *slots;

  /**
   * Every call wants a different number of results.
   * This is set at the call site.
   */
  uint8_t want;
};

/*
 * The gab virtual machine. This has all the state needed for executing
 * bytecode.
 */
struct gab_vm {
  /*
   * The flags passed in to the vm
   */
  uint8_t flags;

  struct gab_vm_frame *fp;

  gab_value *sp;

  gab_value sb[cGAB_STACK_MAX];

  struct gab_vm_frame fb[cGAB_FRAMES_MAX];
};

struct gab_eg {
  size_t hash_seed;

  struct gab_src *sources;

  struct gab_obj_block_proto *prototypes;

  d_gab_imp imports;

  d_strings interned_strings;

  d_shapes interned_shapes;

  d_messages interned_messages;

  gab_value types[kGAB_NKINDS];

  v_gab_value scratch;
};

void *gab_egalloc(struct gab_triple gab, struct gab_obj *obj, uint64_t size);

struct gab_obj_string *gab_eg_find_string(struct gab_eg *gab, s_char str,
                                          uint64_t hash);

struct gab_obj_message *gab_eg_find_message(struct gab_eg *gab, gab_value name,
                                            uint64_t hash);

struct gab_obj_shape *gab_eg_find_shape(struct gab_eg *gab, uint64_t size,
                                        uint64_t stride, uint64_t hash,
                                        gab_value keys[size]);

struct gab_err_argt {
  enum gab_status status;
  const char *note_fmt;
  struct gab_src *src;
  gab_value context;
  size_t tok;
};

void gab_verr(struct gab_err_argt args, va_list va);
#endif
