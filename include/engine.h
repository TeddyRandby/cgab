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

struct gab_gc {
  v_gab_obj decrements, increments, roots;
  d_gab_obj overflow_rc;
  int locked;
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
  gab_value m;

  struct gab_obj_block *b;

  uint8_t *ip;

  gab_value *slots;
};

/*
 * The gab virtual machine. This has all the state needed for executing
 * bytecode.
 */
struct gab_vm {
  struct gab_vm_frame *fp;

  gab_value *sp;

  gab_value sb[cGAB_STACK_MAX];

  struct gab_vm_frame fb[cGAB_FRAMES_MAX];
};

void *gab_egalloc(struct gab_triple gab, struct gab_obj *obj, uint64_t size);

struct gab_obj_string *gab_egstrfind(struct gab_eg *gab, s_char str,
                                     uint64_t hash);

struct gab_obj_message *gab_egmsgfind(struct gab_eg *gab, gab_value name,
                                      uint64_t hash);

struct gab_obj_shape *gab_egshpfind(struct gab_eg *gab, uint64_t size,
                                    uint64_t stride, uint64_t hash,
                                    gab_value keys[size]);

struct gab_err_argt {
  enum gab_status status;
  const char *note_fmt;
  struct gab_src *src;
  gab_value message;
  size_t tok;
};

void gab_fvpanic(struct gab_triple gab, FILE *stream, va_list vastruct,
                 struct gab_err_argt args);
#endif
