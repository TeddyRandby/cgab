#ifndef GAB_ENGINE_H
#define GAB_ENGINE_H

#include "gab.h"

#include "alloc.h"
#include "gc.h"
#include <stdarg.h>

static const char *gab_status_names[] = {
#define STATUS(name, message) message,
#include "include/status_code.h"
#undef STATUS
};

static const char *gab_token_names[] = {
#define TOKEN(message) #message,
#include "include/token.h"
#undef TOKEN
};

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

struct gab_eg {
  size_t hash_seed;

  struct gab_src *sources;

  struct gab_allocator allocator;

  d_gab_imp imports;

  d_strings interned_strings;

  d_shapes interned_shapes;

  d_messages interned_messages;

  gab_value types[kGAB_NKINDS];

  v_gab_value scratch;
};

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
  struct gab_src* src;
  gab_value context;
  size_t flags;
  size_t tok;
};

void gab_verr(struct gab_err_argt args, va_list va);
#endif
