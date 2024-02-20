#define STATUS_NAMES
#define TOKEN_NAMES
#include "engine.h"

#include "colors.h"
#include "core.h"
#include "gab.h"
#include "lexer.h"
#include "natives.h"
#include "os.h"
#include "types.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct primitive {
  const char *name;
  enum gab_kind type;
  gab_value primitive;
};

struct primitive all_primitives[] = {
    {
        .name = mGAB_AND,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_AND),
    },
    {
        .name = mGAB_OR,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_OR),
    },
};

/*
 * It is important that all primitives be FINAL -
 * as in, it is not possible for a gab program to define some
 * specialization which would take precedence OVER the primitive.
 * This is becayse of how dynamic message sends handle primitives
 */
struct primitive specific_primitive[] = {
    {
        .name = mGAB_BOR,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BOR),
    },
    {
        .name = mGAB_BND,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BND),
    },
    {
        .name = mGAB_LSH,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LSH),
    },
    {
        .name = mGAB_RSH,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_RSH),
    },
    {
        .name = mGAB_ADD,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_ADD),
    },
    {
        .name = mGAB_SUB,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_SUB),
    },
    {
        .name = mGAB_MUL,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_MUL),
    },
    {
        .name = mGAB_DIV,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_DIV),
    },
    {
        .name = mGAB_MOD,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_MOD),
    },
    {
        .name = mGAB_LT,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LT),
    },
    {
        .name = mGAB_LTE,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LTE),
    },
    {
        .name = mGAB_GT,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_GT),
    },
    {
        .name = mGAB_GTE,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_GTE),
    },
    {
        .name = mGAB_ADD,
        .type = kGAB_STRING,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_UNDEFINED,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_SPLAT,
        .type = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_SPLAT),
    },
    {
        .name = mGAB_GET,
        .type = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_GET),
    },
    {
        .name = mGAB_SET,
        .type = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_SET),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_NATIVE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_NATIVE),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_BLOCK,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_BLOCK),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_SUSPENSE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_SUSPENSE),
    },
};

struct gab_triple gab_create() {
  struct gab_eg *eg = NEW(struct gab_eg);
  memset(eg, 0, sizeof(struct gab_eg));

  d_gab_src_create(&eg->sources, 8);

  struct gab_gc *gc = NEW(struct gab_gc);
  gab_gccreate(gc);

  struct gab_triple gab = {.eg = eg, .gc = gc};

  eg->hash_seed = time(NULL);

  eg->types[kGAB_UNDEFINED] = gab_undefined;
  eg->types[kGAB_NIL] = gab_nil;
  eg->types[kGAB_TRUE] = gab_true;
  eg->types[kGAB_FALSE] = gab_false;

  eg->types[kGAB_NUMBER] = gab_string(gab, "gab.number");
  gab_iref(gab, eg->types[kGAB_NUMBER]);

  eg->types[kGAB_STRING] = gab_string(gab, "gab.string");
  gab_iref(gab, eg->types[kGAB_STRING]);

  eg->types[kGAB_MESSAGE] = gab_string(gab, "gab.message");
  gab_iref(gab, eg->types[kGAB_MESSAGE]);

  eg->types[kGAB_SPROTOTYPE] = gab_string(gab, "gab.prototype");
  gab_iref(gab, eg->types[kGAB_SPROTOTYPE]);

  eg->types[kGAB_BPROTOTYPE] = gab_string(gab, "gab.prototype");
  gab_iref(gab, eg->types[kGAB_BPROTOTYPE]);

  eg->types[kGAB_NATIVE] = gab_string(gab, "gab.native");
  gab_iref(gab, eg->types[kGAB_NATIVE]);

  eg->types[kGAB_BLOCK] = gab_string(gab, "gab.block");
  gab_iref(gab, eg->types[kGAB_BLOCK]);

  eg->types[kGAB_RECORD] = gab_string(gab, "gab.record");
  gab_iref(gab, eg->types[kGAB_RECORD]);

  eg->types[kGAB_SHAPE] = gab_string(gab, "gab.shape");
  gab_iref(gab, eg->types[kGAB_SHAPE]);

  eg->types[kGAB_BOX] = gab_string(gab, "gab.box");
  gab_iref(gab, eg->types[kGAB_BOX]);

  eg->types[kGAB_SUSPENSE] = gab_string(gab, "gab.suspense");
  gab_iref(gab, eg->types[kGAB_SUSPENSE]);

  eg->types[kGAB_PRIMITIVE] = gab_string(gab, "gab.primitive");
  gab_iref(gab, eg->types[kGAB_PRIMITIVE]);

  gab_negkeep(gab.eg, kGAB_NKINDS, eg->types);

  gab_setup_natives(gab);

  for (int i = 0; i < LEN_CARRAY(specific_primitive); i++) {
    gab_egkeep(
        gab.eg,
        gab_iref(
            gab,
            gab_spec(gab,
                     (struct gab_spec_argt){
                         .name = specific_primitive[i].name,
                         .receiver = gab_type(eg, specific_primitive[i].type),
                         .specialization = specific_primitive[i].primitive,
                     })));
  }

  for (int i = 0; i < LEN_CARRAY(all_primitives); i++) {
    for (int t = 1; t < kGAB_NKINDS; t++) {
      gab_egkeep(
          gab.eg,
          gab_iref(gab, gab_spec(gab, (struct gab_spec_argt){
                                          .name = all_primitives[i].name,
                                          .receiver = gab_type(eg, t),
                                          .specialization =
                                              all_primitives[i].primitive,
                                      })));
    }
  }

  return gab;
}

void gab_destroy(struct gab_triple gab) {
  gab_ndref(gab, 1, gab.eg->scratch.len, gab.eg->scratch.data);

  for (uint64_t i = 0; i < gab.eg->modules.cap; i++) {
    if (d_gab_modules_iexists(&gab.eg->modules, i)) {
      a_gab_value *module = d_gab_modules_ival(&gab.eg->modules, i);
      free(module);
    }
  }

  gab_collect(gab);

  gab_gcdestroy(gab.gc);

  for (uint64_t i = 0; i < gab.eg->sources.cap; i++) {
    if (d_gab_src_iexists(&gab.eg->sources, i)) {
      gab_srcdestroy(d_gab_src_ival(&gab.eg->sources, i));
    }
  }

  d_strings_destroy(&gab.eg->strings);
  d_shapes_destroy(&gab.eg->shapes);
  d_messages_destroy(&gab.eg->messages);
  d_gab_modules_destroy(&gab.eg->modules);
  d_gab_src_destroy(&gab.eg->sources);

  v_gab_value_destroy(&gab.eg->scratch);
  free(gab.gc);
  free(gab.eg);
}

void gab_repl(struct gab_triple gab, struct gab_repl_argt args) {
  a_gab_value *prev = NULL;

  size_t iterations = 0;

  for (;;) {
    printf("%s", args.prompt_prefix);
    a_char *src = gab_fosreadl(stdin);

    if (src->data[0] == EOF) {
      a_char_destroy(src);
      return;
    }

    if (src->data[1] == '\0') {
      a_char_destroy(src);
      continue;
    }

    size_t prev_len = prev == NULL ? 0 : prev->len;

    // Append the iterations number to the end of the given name
    char unique_name[strlen(args.name) + 16];
    snprintf(unique_name, sizeof(unique_name), "%s:%lu", args.name, iterations);

    iterations++;

    /*
     * Build a buffer holding the argument names.
     * Arguments from the previous iteration should be empty strings.
     */
    const char *sargv[args.len + prev_len];

    for (int i = 0; i < prev_len; i++) {
      sargv[i] = "";
    }

    memcpy(sargv + prev_len, args.sargv, args.len * sizeof(char *));

    gab_value argv[args.len + prev_len];

    for (int i = 0; i < prev_len; i++) {
      argv[i] = prev ? prev->data[i] : gab_nil;
    }

    memcpy(argv + prev_len, args.argv, args.len * sizeof(gab_value));

    a_gab_value *result = gab_exec(gab, (struct gab_exec_argt){
                                            .name = unique_name,
                                            .source = (char *)src->data,
                                            .flags = args.flags,
                                            .len = args.len + prev_len,
                                            .sargv = sargv,
                                            .argv = argv,
                                        });

    if (result == NULL)
      continue;

    gab_niref(gab, 1, result->len, result->data);
    gab_negkeep(gab.eg, result->len, result->data);

    printf("%s", args.result_prefix);
    for (int32_t i = 0; i < result->len; i++) {
      gab_value arg = result->data[i];

      if (i == result->len - 1) {
        gab_fvalinspect(stdout, arg, -1);
      } else {
        gab_fvalinspect(stdout, arg, -1);
        printf(", ");
      }
    }

    printf("\n");

    if (prev)
      a_gab_value_destroy(prev);

    prev = result;
  }
}

a_gab_value *gab_exec(struct gab_triple gab, struct gab_exec_argt args) {
  gab_value main = gab_cmpl(gab, (struct gab_cmpl_argt){
                                     .name = args.name,
                                     .source = args.source,
                                     .flags = args.flags,
                                     .len = args.len,
                                     .argv = args.sargv,
                                 });

  if (main == gab_undefined) {
    return NULL;
  }

  return gab_run(gab, (struct gab_run_argt){
                          .main = main,
                          .flags = args.flags,
                          .len = args.len,
                          .argv = args.argv,
                      });
}

int gab_nspec(struct gab_triple gab, size_t len,
              struct gab_spec_argt args[static len]) {
  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    if (gab_spec(gab, args[i]) == gab_undefined) {
      gab_gcunlock(gab.gc);
      return i;
    }
  }

  gab_gcunlock(gab.gc);
  return -1;
}

gab_value gab_spec(struct gab_triple gab, struct gab_spec_argt args) {
  gab_gclock(gab.gc);

  gab_value n = gab_string(gab, args.name);
  gab_value m = gab_message(gab, n);
  gab_msgput(gab, m, args.receiver, args.specialization);

  gab_gcunlock(gab.gc);

  return m;
}

struct gab_obj_message *gab_egmsgfind(struct gab_eg *self, gab_value name,
                                      uint64_t hash) {
  if (self->messages.len == 0)
    return NULL;

  uint64_t index = hash & (self->messages.cap - 1);

  for (;;) {
    d_status status = d_messages_istatus(&self->messages, index);
    struct gab_obj_message *key = d_messages_ikey(&self->messages, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return NULL;
    case D_FULL:
      if (key->hash == hash && key->name == name)
        return key;
    }

    index = (index + 1) & (self->messages.cap - 1);
  }
}

struct gab_obj_string *gab_egstrfind(struct gab_eg *self, s_char str,
                                     uint64_t hash) {
  if (self->strings.len == 0)
    return NULL;

  uint64_t index = hash & (self->strings.cap - 1);

  for (;;) {
    d_status status = d_strings_istatus(&self->strings, index);
    struct gab_obj_string *key = d_strings_ikey(&self->strings, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return NULL;
    case D_FULL:
      if (key->hash == hash && s_char_match(str, (s_char){
                                                     .data = key->data,
                                                     .len = key->len,
                                                 }))
        return key;
    }

    index = (index + 1) & (self->strings.cap - 1);
  }
}

static inline bool shape_matches_keys(struct gab_obj_shape *self,
                                      gab_value values[], uint64_t len,
                                      uint64_t stride) {

  if (self->len != len)
    return false;

  for (uint64_t i = 0; i < len; i++) {
    gab_value key = values[i * stride];
    if (self->data[i] != key)
      return false;
  }

  return true;
}

struct gab_obj_shape *gab_egshpfind(struct gab_eg *self, uint64_t size,
                                    uint64_t stride, uint64_t hash,
                                    gab_value keys[size]) {
  if (self->shapes.len == 0)
    return NULL;

  uint64_t index = hash & (self->shapes.cap - 1);

  for (;;) {
    d_status status = d_shapes_istatus(&self->shapes, index);
    struct gab_obj_shape *key = d_shapes_ikey(&self->shapes, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return NULL;
    case D_FULL:
      if (key->hash == hash && shape_matches_keys(key, keys, size, stride))
        return key;
    }

    index = (index + 1) & (self->shapes.cap - 1);
  }
}

a_gab_value *gab_segmodat(struct gab_eg *eg, const char *name) {
  size_t hash = s_char_hash(s_char_cstr(name), eg->hash_seed);

  return d_gab_modules_read(&eg->modules, hash);
}

a_gab_value *gab_segmodput(struct gab_eg *eg, const char *name, gab_value mod,
                           size_t len, gab_value values[len]) {
  size_t hash = s_char_hash(s_char_cstr(name), eg->hash_seed);

  if (d_gab_modules_exists(&eg->modules, hash))
    return NULL;

  a_gab_value *module = a_gab_value_empty(len + 1);
  module->data[0] = mod;
  memcpy(module->data + 1, values, len * sizeof(gab_value));

  d_gab_modules_insert(&eg->modules, hash, module);
  return module;
}

size_t gab_egkeep(struct gab_eg *gab, gab_value v) {
  if (gab_valiso(v))
    v_gab_value_push(&gab->scratch, v);

  return gab->scratch.len;
}

size_t gab_negkeep(struct gab_eg *gab, size_t len,
                   gab_value values[static len]) {
  for (uint64_t i = 0; i < len; i++)
    v_gab_value_push(&gab->scratch, values[i]);

  return gab->scratch.len;
}

gab_value gab_valcpy(struct gab_triple gab, gab_value value) {
  switch (gab_valkind(value)) {

  default:
    return value;

  case kGAB_BOX: {
    struct gab_obj_box *self = GAB_VAL_TO_BOX(value);
    gab_value copy = gab_box(gab, (struct gab_box_argt){
                                      .type = gab_valcpy(gab, self->type),
                                      .data = self->data,
                                      .size = self->len,
                                      .visitor = self->do_visit,
                                      .destructor = self->do_destroy,
                                  });
    return copy;
  }

  case kGAB_MESSAGE: {
    struct gab_obj_message *self = GAB_VAL_TO_MESSAGE(value);
    gab_value copy = gab_message(gab, gab_valcpy(gab, self->name));

    return copy;
  }

  case kGAB_STRING: {
    struct gab_obj_string *self = GAB_VAL_TO_STRING(value);
    return gab_nstring(gab, self->len, self->data);
  }

  case kGAB_NATIVE: {
    struct gab_obj_native *self = GAB_VAL_TO_NATIVE(value);
    return gab_native(gab, gab_valcpy(gab, self->name), self->function);
  }

  case kGAB_BPROTOTYPE: {
    struct gab_obj_prototype *self = GAB_VAL_TO_PROTOTYPE(value);

    gab_value copy = gab_bprototype(gab, gab_srccpy(gab, self->src),
                                    gab_valcpy(gab, self->name), self->offset,
                                    self->as.block.len,
                                    (struct gab_blkproto_argt){
                                        .nupvalues = self->as.block.nupvalues,
                                        .nslots = self->as.block.nslots,
                                        .narguments = self->as.block.narguments,
                                        .nlocals = self->as.block.nlocals,
                                        .data = self->data,
                                    });

    return copy;
  }

  case kGAB_BLOCK: {
    struct gab_obj_block *self = GAB_VAL_TO_BLOCK(value);

    gab_value p_copy = gab_valcpy(gab, self->p);

    gab_value copy = gab_block(gab, p_copy);

    for (uint8_t i = 0; i < GAB_VAL_TO_PROTOTYPE(p_copy)->as.block.nupvalues;
         i++) {
      GAB_VAL_TO_BLOCK(copy)->upvalues[i] = gab_valcpy(gab, self->upvalues[i]);
    }

    return copy;
  }

  case kGAB_SHAPE: {
    struct gab_obj_shape *self = GAB_VAL_TO_SHAPE(value);

    gab_value keys[self->len];

    for (uint64_t i = 0; i < self->len; i++) {
      keys[i] = gab_valcpy(gab, self->data[i]);
    }

    gab_value copy = gab_shape(gab, 1, self->len, keys);

    return copy;
  }

  case kGAB_RECORD: {
    struct gab_obj_record *self = GAB_VAL_TO_RECORD(value);

    gab_value s_copy = gab_valcpy(gab, self->shape);

    gab_value values[self->len];

    for (uint64_t i = 0; i < self->len; i++)
      values[i] = gab_valcpy(gab, self->data[i]);

    return gab_recordof(gab, s_copy, 1, values);
  }

  case kGAB_SPROTOTYPE: {
    struct gab_obj_prototype *self = GAB_VAL_TO_PROTOTYPE(value);

    return gab_sprototype(gab, gab_srccpy(gab, self->src),
                          gab_valcpy(gab, self->name), self->offset);
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *self = GAB_VAL_TO_SUSPENSE(value);

    gab_value frame_copy[self->nslots];

    for (size_t i = 0; i < self->nslots; i++) {
      frame_copy[i] = gab_valcpy(gab, self->slots[i]);
    }

    gab_value p_copy = gab_valcpy(gab, self->p);
    gab_value b_copy = gab_valcpy(gab, self->b);

    return gab_suspense(gab, b_copy, p_copy, self->nslots, frame_copy);
  }
  }
}

gab_value gab_string(struct gab_triple gab, const char data[static 1]) {
  return gab_nstring(gab, strlen(data), data);
}

gab_value gab_record(struct gab_triple gab, uint64_t size, gab_value keys[size],
                     gab_value values[size]) {
  gab_value bundle_shape = gab_shape(gab, 1, size, keys);
  return gab_recordof(gab, bundle_shape, 1, values);
}

gab_value gab_srecord(struct gab_triple gab, uint64_t size,
                      const char *keys[size], gab_value values[size]) {
  gab_value value_keys[size];

  for (uint64_t i = 0; i < size; i++)
    value_keys[i] = gab_string(gab, keys[i]);

  gab_value bundle_shape = gab_shape(gab, 1, size, value_keys);
  return gab_recordof(gab, bundle_shape, 1, values);
}

gab_value gab_etuple(struct gab_triple gab, size_t len) {
  gab_gclock(gab.gc);
  gab_value bundle_shape = gab_nshape(gab, len);
  gab_value v = gab_erecordof(gab, bundle_shape);
  gab_gcunlock(gab.gc);
  return v;
}

gab_value gab_tuple(struct gab_triple gab, uint64_t size,
                    gab_value values[size]) {
  gab_gclock(gab.gc);

  gab_value bundle_shape = gab_nshape(gab, size);
  gab_value v = gab_recordof(gab, bundle_shape, 1, values);
  gab_gcunlock(gab.gc);
  return v;
}

void gab_fvpanic(struct gab_triple gab, FILE *stream, va_list varargs,
                 struct gab_err_argt args) {
  if (!(gab.flags & fGAB_DUMP_ERROR))
    goto fin;

  const char *tok_name =
      args.src
          ? gab_token_names[v_gab_token_val_at(&args.src->tokens, args.tok)]
          : "C";

  fputc('\n', stream);

  if (args.src) {
    fprintf(stream, "[" ANSI_COLOR_GREEN);
    gab_fvalinspect(stream, args.src->name, 0);
    fprintf(stream, ANSI_COLOR_RESET "]");
  }

  if (args.block != gab_nil) {
    fprintf(stream, " " ANSI_COLOR_CYAN);
    gab_fvalinspect(stream, args.block, 0);
    fprintf(stream, ANSI_COLOR_RESET);
  }

  fprintf(stream,
          ANSI_COLOR_RESET " panicked near " ANSI_COLOR_MAGENTA
                           "%s" ANSI_COLOR_RESET,
          tok_name);

  if (args.status != GAB_NONE) {
    fprintf(stream, ":" ANSI_COLOR_YELLOW " %s." ANSI_COLOR_RESET,
            gab_status_names[args.status]);
  }

  if (args.src) {
    s_char tok_src = v_s_char_val_at(&args.src->token_srcs, args.tok);
    const char *tok_start = tok_src.data;
    const char *tok_end = tok_src.data + tok_src.len;

    uint64_t line = v_uint64_t_val_at(&args.src->token_lines, args.tok);

    s_char line_src = v_s_char_val_at(&args.src->lines, line - 1);

    while (*line_src.data == ' ' || *line_src.data == '\t') {
      line_src.data++;
      line_src.len--;
      if (line_src.len == 0)
        break;
    }

    a_char *line_under = a_char_empty(line_src.len);

    for (uint8_t i = 0; i < line_under->len; i++) {
      if (line_src.data + i >= tok_start && line_src.data + i < tok_end)
        line_under->data[i] = '^';
      else
        line_under->data[i] = ' ';
    }

    fprintf(stream,
            "\n\n" ANSI_COLOR_RED "%.4lu" ANSI_COLOR_RESET "| %.*s"
            "\n      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET "",
            line, (int)line_src.len, line_src.data, (int)line_under->len,
            line_under->data);

    a_char_destroy(line_under);
  }

  if (args.note_fmt && strlen(args.note_fmt) > 0) {
    fprintf(stream, "\n\n");
    const char *c = args.note_fmt;

    while (*c != '\0') {
      switch (*c) {
      case '$':
        fprintf(stream, ANSI_COLOR_GREEN);
        gab_fvalinspect(stream, va_arg(varargs, gab_value), 1);
        fprintf(stream, ANSI_COLOR_RESET);
        break;
      default:
        fputc(*c, stream);
      }
      c++;
    }
  }

  fprintf(stream,
          "\n________________________________________________________\n");

fin:
  if (gab.flags & fGAB_EXIT_ON_PANIC) {
    exit(1);
  }
}

void *gab_egalloc(struct gab_triple gab, struct gab_obj *obj, uint64_t size) {
  if (size == 0) {
    assert(obj);

    free(obj);

    return NULL;
  }

  assert(!obj);

  return malloc(size);
}

int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args) {
  const gab_value value = *(const gab_value *const)args[0];
  return gab_fvalinspect(stream, value, -1);
}

int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes) {
  if (n > 0) {
    argtypes[0] = PA_INT | PA_FLAG_LONG;
    sizes[0] = sizeof(gab_value);
  }

  return 1;
}
