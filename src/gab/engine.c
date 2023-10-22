#include "include/engine.h"
#include "include/builtins.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/import.h"
#include "include/lexer.h"
#include "include/module.h"
#include "include/types.h"
#include "include/vm.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct primitive {
  const char *name;
  enum gab_kind type;
  gab_value primitive;
};

struct primitive primitives[] = {
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
        .name = mGAB_SET,
        .type = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_STORE),
    },
    {
        .name = mGAB_GET,
        .type = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LOAD),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_BUILTIN,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_BUILTIN),
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

  struct gab_gc *gc = NEW(struct gab_gc);
  gab_gccreate(gc);

  struct gab_triple gab = {.eg = eg, .gc = gc};

  eg->hash_seed = time(NULL);

  eg->types[kGAB_UNDEFINED] = gab_undefined;
  eg->types[kGAB_NIL] = gab_nil;
  eg->types[kGAB_NUMBER] = gab_string(gab, "Number");
  eg->types[kGAB_TRUE] = gab_string(gab, "Boolean");
  eg->types[kGAB_FALSE] = gab_string(gab, "Boolean");
  eg->types[kGAB_STRING] = gab_string(gab, "String");
  eg->types[kGAB_MESSAGE] = gab_string(gab, "Message");
  eg->types[kGAB_BLOCK_PROTO] = gab_string(gab, "Prototype");
  eg->types[kGAB_BUILTIN] = gab_string(gab, "Builtin");
  eg->types[kGAB_BLOCK] = gab_string(gab, "Block");
  eg->types[kGAB_RECORD] = gab_string(gab, "Record");
  eg->types[kGAB_SHAPE] = gab_string(gab, "Shape");
  eg->types[kGAB_BOX] = gab_string(gab, "Box");
  eg->types[kGAB_SUSPENSE] = gab_string(gab, "Suspsense");
  eg->types[kGAB_PRIMITIVE] = gab_string(gab, "Primitive");

  gab_ngciref(gab, 1, kGAB_NKINDS, eg->types);

  gab_negkeep(gab.eg, kGAB_NKINDS, eg->types);

  gab_setup_builtins(gab);

  for (int i = 0; i < LEN_CARRAY(primitives); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = primitives[i].name,
                      .receiver = gab_typ(eg, primitives[i].type),
                      .specialization = primitives[i].primitive,
                  });
  }

  return gab;
}

void gab_destroy(struct gab_triple gab) {
  gab_ngcdref(gab, 1, gab.eg->scratch.len, gab.eg->scratch.data);

  while (gab.eg->sources) {
    struct gab_src *s = gab.eg->sources;
    gab.eg->sources = s->next;
    gab_srcdestroy(s);
  }

  while (gab.eg->modules) {
    struct gab_mod *m = gab.eg->modules;
    gab.eg->modules = m->next;
    gab_moddestroy(gab, m);
  }

  gab_gcrun(gab);

  gab_gcdestroy(gab.gc);

  for (uint64_t i = 0; i < gab.eg->imports.cap; i++) {
    if (d_gab_imp_iexists(&gab.eg->imports, i)) {
      gab_impdestroy(gab.eg, gab.gc, d_gab_imp_ival(&gab.eg->imports, i));
    }
  }

  d_strings_destroy(&gab.eg->interned_strings);
  d_shapes_destroy(&gab.eg->interned_shapes);
  d_messages_destroy(&gab.eg->interned_messages);
  d_gab_imp_destroy(&gab.eg->imports);

  v_gab_value_destroy(&gab.eg->scratch);
  free(gab.gc);
  free(gab.eg);
}

gab_value gab_cmpl(struct gab_triple gab, struct gab_cmpl_argt args) {
  gab_value argv_names[args.len];

  for (uint64_t i = 0; i < args.len; i++)
    argv_names[i] = gab_string(gab, args.argv[i]);

  gab_value res =
      gab_bccomp(gab, gab_string(gab, args.name),
                 s_char_create(args.source, strlen(args.source) + 1),
                 args.flags, args.len, argv_names);

  return res;
}

a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args) {
  return gab_vmrun(gab, args.main, args.flags, args.len, args.argv);
};

a_gab_value *gab_exec(struct gab_triple gab, struct gab_exec_argt args) {
  gab_value main = gab_cmpl(gab, (struct gab_cmpl_argt){
                                     .name = args.name,
                                     .source = args.source,
                                     .flags = args.flags,
                                     .len = args.len,
                                     .argv = args.sargv,
                                 });

  if (main == gab_undefined)
    return NULL;

  return gab_run(gab, (struct gab_run_argt){
                          .main = main,
                          .flags = args.flags,
                          .len = args.len,
                          .argv = args.argv,
                      });
}

gab_value gab_panic(struct gab_triple gab, const char *msg) {
  gab_value vm_container = gab_vm_panic(gab, msg);
  return vm_container;
}

void gab_nspec(struct gab_triple gab, size_t len,
               struct gab_spec_argt args[static len]) {
  gab_gcreserve(gab, len * 3);

  for (size_t i = 0; i < len; i++) {
    gab_spec(gab, args[i]);
  }
}

gab_value gab_spec(struct gab_triple gab, struct gab_spec_argt args) {
  gab_value n = gab_string(gab, args.name);
  gab_value m = gab_message(gab, n);

  if (gab_msgfind(m, args.receiver) != UINT64_MAX)
    return gab_nil;

  struct gab_obj_message *msg = GAB_VAL_TO_MESSAGE(m);

  bool new = GAB_OBJ_IS_NEW(gab_valtoo(m));

  // if (!new)
  //   gab_gcdref(gab, msg->specs);

  msg->specs =
      gab_recordwith(gab, msg->specs, args.receiver, args.specialization);
  msg->version++;

  // if (!new)
  //   gab_gciref(gab, msg->specs);
  // else
  //   gab_egkeep(gab.eg, m), gab_gciref(gab, m);

  return m;
}

a_gab_value *send_msg(struct gab_triple gab, gab_value msg, gab_value receiver,
                      size_t argc, gab_value argv[argc]) {
  if (msg == gab_undefined)
    return a_gab_value_one(gab_undefined);

  gab_value main =
      gab_bccompsend(gab, msg, receiver, fGAB_DUMP_ERROR, argc, argv);

  if (main == gab_undefined)
    return a_gab_value_one(gab_undefined);

  return gab_vmrun(gab, main, fGAB_DUMP_ERROR, 0, NULL);
}

a_gab_value *gab_send(struct gab_triple gab, struct gab_send_argt args) {
  if (args.smessage) {
    gab_value msg = gab_message(gab, gab_string(gab, args.smessage));
    return send_msg(gab, msg, args.receiver, args.len, args.argv);
  }

  switch (gab_valknd(args.vmessage)) {
  case kGAB_STRING: {
    gab_value main = gab_message(gab, args.vmessage);
    return send_msg(gab, main, args.receiver, args.len, args.argv);
  }
  case kGAB_MESSAGE:
    return send_msg(gab, args.vmessage, args.receiver, args.len, args.argv);
  default:
    return NULL;
  }
}

struct gab_obj_message *gab_eg_find_message(struct gab_eg *self, gab_value name,
                                            uint64_t hash) {
  if (self->interned_messages.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_messages.cap - 1);

  for (;;) {
    d_status status = d_messages_istatus(&self->interned_messages, index);
    struct gab_obj_message *key =
        d_messages_ikey(&self->interned_messages, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return NULL;
    case D_FULL:
      if (key->hash == hash && key->name == name)
        return key;
    }

    index = (index + 1) & (self->interned_messages.cap - 1);
  }
}

struct gab_obj_string *gab_eg_find_string(struct gab_eg *self, s_char str,
                                          uint64_t hash) {
  if (self->interned_strings.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_strings.cap - 1);

  for (;;) {
    d_status status = d_strings_istatus(&self->interned_strings, index);
    struct gab_obj_string *key = d_strings_ikey(&self->interned_strings, index);

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

    index = (index + 1) & (self->interned_strings.cap - 1);
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

/*
 *
 * TODO: All these find_x functions need to change.
 *
 * Now that we can remove interned values, this chaining dict doesn't work as
 * well.
 *
 * Imagine a hash-collision chain like this
 *
 * Looking for z, which hashes to 5
 *
 * [x] [y] [z]
 *  5   6   7
 *
 * In this scenario, we hash to 5 and follow the chain to z. Works as planned.
 *
 * Now we remove y from the dict, as it is collected
 *
 * [x] [ ] [z]
 *  5   6   7
 *
 * Next time we go looking for z, we hash to 5 and find the open spot at 6.
 *
 * This is where the tombstone values come into play I believe.
 *
 * We can reactive the tombstone slot somehow
 */

struct gab_obj_shape *gab_eg_find_shape(struct gab_eg *self, uint64_t size,
                                        uint64_t stride, uint64_t hash,
                                        gab_value keys[size]) {
  if (self->interned_shapes.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_shapes.cap - 1);

  for (;;) {
    d_status status = d_shapes_istatus(&self->interned_shapes, index);
    struct gab_obj_shape *key = d_shapes_ikey(&self->interned_shapes, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return NULL;
    case D_FULL:
      if (key->hash == hash && shape_matches_keys(key, keys, size, stride))
        return key;
    }

    index = (index + 1) & (self->interned_shapes.cap - 1);
  }
}

gab_value gab_typ(struct gab_eg *gab, enum gab_kind k) { return gab->types[k]; }

int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args) {
  const gab_value value = *(const gab_value *const)args[0];
  return gab_fdump(stream, value);
}
int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes) {
  if (n > 0) {
    argtypes[0] = PA_INT | PA_FLAG_LONG;
    sizes[0] = sizeof(gab_value);
  }

  return 1;
}

size_t gab_egkeep(struct gab_eg *gab, gab_value v) {
  v_gab_value_push(&gab->scratch, v);
  return gab->scratch.len;
}

size_t gab_negkeep(struct gab_eg *gab, size_t len, gab_value *values) {
  for (uint64_t i = 0; i < len; i++)
    v_gab_value_push(&gab->scratch, values[i]);

  return gab->scratch.len;
}

size_t gab_vmpush(struct gab_vm *vm, gab_value value) {
  return gab_vm_push(vm, 1, &value);
}

size_t gab_nvmpush(struct gab_vm *vm, size_t argc, gab_value argv[argc]) {
  return gab_vm_push(vm, argc, argv);
}

gab_value gab_valcpy(struct gab_triple gab, gab_value value) {
  switch (gab_valknd(value)) {

  default:
    return value;

  case kGAB_BOX: {
    struct gab_obj_box *self = GAB_VAL_TO_BOX(value);
    gab_value copy = gab_box(gab, (struct gab_box_argt){
                                      .type = gab_valcpy(gab, self->type),
                                      .data = self->data,
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

  case kGAB_BUILTIN: {
    struct gab_obj_builtin *self = GAB_VAL_TO_BUILTIN(value);
    return gab_builtin(gab, gab_valcpy(gab, self->name), self->function);
  }

  case kGAB_BLOCK_PROTO: {
    struct gab_obj_block_proto *self = GAB_VAL_TO_BLOCK_PROTO(value);
    gab_modcpy(gab, self->mod);

    gab_value copy = gab_blkproto(gab, (struct gab_blkproto_argt){
                                           .nupvalues = self->nupvalues,
                                           .nslots = self->nslots,
                                           .narguments = self->narguments,
                                           .nlocals = self->nlocals,
                                           .mod = gab.eg->modules,
                                       });

    memcpy(GAB_VAL_TO_BLOCK_PROTO(copy)->upv_desc, self->upv_desc,
           self->nupvalues * 2);

    gab_egkeep(gab.eg, copy); // No module to own this prototype

    return copy;
  }

  case kGAB_BLOCK: {
    struct gab_obj_block *self = GAB_VAL_TO_BLOCK(value);

    gab_value p_copy = gab_valcpy(gab, self->p);

    gab_value copy = gab_block(gab, p_copy);

    for (uint8_t i = 0; i < GAB_VAL_TO_BLOCK_PROTO(p_copy)->nupvalues; i++) {
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

  case kGAB_SUSPENSE_PROTO: {
    struct gab_obj_suspense_proto *self = GAB_VAL_TO_SUSPENSE_PROTO(value);

    return gab_susproto(gab, self->offset, self->want);
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *self = GAB_VAL_TO_SUSPENSE(value);

    gab_value frame[self->len];

    gab_value p_copy = gab_valcpy(gab, self->p);

    for (uint64_t i = 0; i < self->len; i++)
      frame[i] = gab_valcpy(gab, self->frame[i]);

    gab_value b_copy = gab_valcpy(gab, self->b);

    return gab_suspense(gab, self->len, b_copy, p_copy, frame);
  }
  }
}

gab_value gab_string(struct gab_triple gab, const char *data) {
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
  gab_value bundle_shape = gab_nshape(gab, len);
  return gab_erecordof(gab, bundle_shape);
}

gab_value gab_tuple(struct gab_triple gab, uint64_t size,
                    gab_value values[size]) {
  gab_value bundle_shape = gab_nshape(gab, size);
  return gab_recordof(gab, bundle_shape, 1, values);
}

void gab_verr(struct gab_err_argt args, va_list varargs) {
  if (!(args.flags & fGAB_DUMP_ERROR))
    return;

  struct gab_src *src = args.mod->source;

  if (!src) {
    fprintf(stderr,
            "\n[" ANSI_COLOR_GREEN "%V" ANSI_COLOR_RESET "]" ANSI_COLOR_YELLOW
            " %s. " ANSI_COLOR_RESET " " ANSI_COLOR_GREEN,
            args.context, gab_status_names[args.status]);

    vfprintf(stderr, args.note_fmt, varargs);

    fprintf(stderr, ANSI_COLOR_RESET "\n");

    return;
  }

  uint64_t line = v_uint64_t_val_at(&src->token_lines, args.tok);

  s_char tok_src = v_s_char_val_at(&src->token_srcs, args.tok);

  s_char line_src = v_s_char_val_at(&src->lines, line - 1);

  while (is_whitespace(*line_src.data)) {
    line_src.data++;
    line_src.len--;
    if (line_src.len == 0)
      break;
  }

  a_char *line_under = a_char_empty(line_src.len);

  const char *tok_start, *tok_end;

  tok_start = tok_src.data;
  tok_end = tok_src.data + tok_src.len;

  const char *tok_name =
      gab_token_names[v_gab_token_val_at(&src->tokens, args.tok)];

  for (uint8_t i = 0; i < line_under->len; i++) {
    if (line_src.data + i >= tok_start && line_src.data + i < tok_end)
      line_under->data[i] = '^';
    else
      line_under->data[i] = ' ';
  }

  const char *curr_color = ANSI_COLOR_RED;
  const char *curr_box = "\u256d";

  fprintf(stderr,
          "\n[" ANSI_COLOR_GREEN "%V" ANSI_COLOR_RESET
          "] Error near " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET
          ":\n\t%s%s %.4lu " ANSI_COLOR_RESET "%.*s"
          "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
          "\n\t\u2570\u2500> ",
          args.context, tok_name, curr_box, curr_color, line, (int)line_src.len,
          line_src.data, (int)line_under->len, line_under->data);

  a_char_destroy(line_under);

  fprintf(stderr,
          ANSI_COLOR_YELLOW "%s. \n\n" ANSI_COLOR_RESET "\t" ANSI_COLOR_GREEN,
          gab_status_names[args.status]);

  vfprintf(stderr, args.note_fmt, varargs);

  fprintf(stderr, "\n" ANSI_COLOR_RESET);
}
