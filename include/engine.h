#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "core.h"
#include "gab.h"
#include <threads.h>

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

void gab_egqpush(struct gab_eg *eg, struct gab_fb* fiber);

a_gab_value* gab_vmexec(struct gab_triple gab, struct gab_fb* fiber);

void gab_gccreate(struct gab_gc *gc);

void gab_gcdestroy(struct gab_gc *gc);

/*
 * Check if collection is necessary, and unblock the collection
 * thread if necessary
 */
void gab_gctrigger(struct gab_eg *gc);

/*
 * Begin the next epoch for the given pid
 */
void gab_gcepochnext(struct gab_triple gab, size_t pid);

void gab_gcdocollect(struct gab_triple gab);

typedef void (*gab_gc_visitor)(struct gab_triple gab, struct gab_obj *obj);

enum variable_flag {
  fLOCAL_LOCAL = 1 << 0,
  fLOCAL_CAPTURED = 1 << 1,
  fLOCAL_INITIALIZED = 1 << 2,
  fLOCAL_REST = 1 << 3,
};

static inline void *gab_egalloc(struct gab_triple gab, struct gab_obj *obj,
                                uint64_t size) {
  return gab.eg->os_objalloc(gab, obj, size);
}

struct gab_obj_string *gab_egstrfind(struct gab_eg *gab, s_char str,
                                     uint64_t hash);

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
