#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "core.h"
#include "gab.h"

#ifdef GAB_STATUS_NAMES_IMPL
static const char *gab_status_names[] = {
#define STATUS(name, message) message,
#include "status_code.h"
#undef STATUS
};
#undef STATUS_NAMES
#endif

#ifdef GAB_TOKEN_NAMES_IMPL
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
  int locked;
  d_gab_obj overflow_rc;
  v_gab_obj dead;
  size_t dlen, ilen;
  struct gab_obj *decrements[cGAB_GC_DEC_BUFF_MAX];
  struct gab_obj *increments[cGAB_GC_MOD_BUFF_MAX];
};

void gab_gccreate(struct gab_gc *gc);
void gab_gcdestroy(struct gab_gc *gc);

typedef void (*gab_gc_visitor)(struct gab_triple gab, struct gab_obj *obj);

enum variable_flag {
  fLOCAL_LOCAL = 1 << 0,
  fLOCAL_CAPTURED = 1 << 1,
  fLOCAL_INITIALIZED = 1 << 2,
  fLOCAL_REST = 1 << 3,
};

/*
 * The gab virtual machine. This has all the state needed for executing
 * bytecode.
 */
struct gab_vm {
  uint8_t *ip;

  gab_value *sp, *fp;

  gab_value sb[cGAB_STACK_MAX];
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

void gab_vfpanic(struct gab_triple gab, FILE *stream, va_list vastruct,
                 struct gab_err_argt args);
#endif
