#include <threads.h>
#define GAB_STATUS_NAMES_IMPL
#define GAB_TOKEN_NAMES_IMPL
#include "engine.h"

#define GAB_COLORS_IMPL
#include "colors.h"

#include "core.h"
#include "gab.h"
#include "lexer.h"
#include "os.h"
#include "types.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

struct primitive {
  const char *name;
  union {
    enum gab_kind kind;
    gab_value type;
  };
  gab_value primitive;
};

struct primitive all_primitives[] = {
    {
        .name = mGAB_TYPE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_TYPE),
    },
};

struct primitive type_primitives[] = {
    {
        .name = mGAB_BND,
        .type = gab_false,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LND),
    },
    {
        .name = mGAB_BOR,
        .type = gab_false,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LOR),
    },
    {
        .name = mGAB_LIN,
        .type = gab_false,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LIN),
    },
    {
        .name = mGAB_BND,
        .type = gab_true,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LND),
    },
    {
        .name = mGAB_BOR,
        .type = gab_true,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LOR),
    },
    {
        .name = mGAB_LIN,
        .type = gab_true,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LIN),
    },
};

struct primitive kind_primitives[] = {
    {
        .name = mGAB_BIN,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BIN),
    },
    {
        .name = mGAB_BIN,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BIN),
    },
    {
        .name = mGAB_BOR,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BOR),
    },
    {
        .name = mGAB_BND,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_BND),
    },
    {
        .name = mGAB_LSH,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LSH),
    },
    {
        .name = mGAB_RSH,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_RSH),
    },
    {
        .name = mGAB_ADD,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_ADD),
    },
    {
        .name = mGAB_SUB,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_SUB),
    },
    {
        .name = mGAB_MUL,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_MUL),
    },
    {
        .name = mGAB_DIV,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_DIV),
    },
    {
        .name = mGAB_MOD,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_MOD),
    },
    {
        .name = mGAB_LT,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LT),
    },
    {
        .name = mGAB_LTE,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LTE),
    },
    {
        .name = mGAB_GT,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_GT),
    },
    {
        .name = mGAB_GTE,
        .kind = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_GTE),
    },
    {
        .name = mGAB_ADD,
        .kind = kGAB_STRING,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = mGAB_ADD,
        .kind = kGAB_SIGIL,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = mGAB_EQ,
        .kind = kGAB_UNDEFINED,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_SPLAT,
        .kind = kGAB_RECORD,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_SPLAT),
    },
    {
        .name = mGAB_USE,
        .kind = kGAB_STRING,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_USE),
    },
    {
        .name = mGAB_CALL,
        .kind = kGAB_NATIVE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_NATIVE),
    },
    {
        .name = mGAB_CALL,
        .kind = kGAB_BLOCK,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_BLOCK),
    },
    {
        .name = mGAB_CALL,
        .kind = kGAB_MESSAGE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CALL_MESSAGE),
    },
};

struct gab_fb *gab_egqpop(struct gab_eg *eg) {
  mtx_lock(&eg->queue_mtx);

  if (eg->queue.len == 0) {
    mtx_unlock(&eg->queue_mtx);
    return nullptr;
  }

  struct gab_fb *fiber = v_gab_fb_pop(&eg->queue);

  mtx_unlock(&eg->queue_mtx);
  return fiber;
}

void gab_egqpush(struct gab_eg *eg, struct gab_fb *fiber) {
  mtx_lock(&eg->queue_mtx);
  v_gab_fb_push(&eg->queue, fiber);
  mtx_unlock(&eg->queue_mtx);
}

int32_t wkid(struct gab_eg *eg) {
  for (size_t i = 0; i < GAB_NWORKERS; i++) {
    struct gab_wk *wk = &eg->workers[i];
    if (wk->td == thrd_current())
      return i;
  }

  return -1;
}

a_gab_value *gab_fibawait(struct gab_fb *fiber) {
  while (fiber->status != kGAB_FIBER_DONE)
    thrd_yield();

  return fiber->res;
}

int32_t gc_thread(void *data) {
  struct gab_eg *eg = data;
  struct gab_triple gab = {.eg = eg, .wkid = GAB_NWORKERS};

  while (eg->alive) {
    if (eg->gc.schedule == gab.wkid)
      gab_gcdocollect(gab);
  }

  return 0;
}

int32_t worker_thread(void *data) {
  struct gab_eg *eg = data;
  int32_t id = wkid(eg);

  struct gab_triple gab = {.eg = eg, .wkid = id};

  while (gab.eg->alive) {
    struct gab_fb *fiber = gab_egqpop(eg);

    if (gab.eg->gc.schedule == gab.wkid)
      gab_gcepochnext(gab);

    if (!fiber) {
      thrd_yield();
      continue;
    }

    gab.eg->workers[id].fiber = fiber;

    a_gab_value *res = gab_vmexec(gab, fiber);
    fiber->res = res;
    fiber->status = kGAB_FIBER_DONE;
    assert(res);

    gab.eg->workers[id].fiber = nullptr;

    if (!res)
      goto fin;
  }

fin:
  gab.eg->workers[id].fiber = nullptr;
  gab.eg->nworkers--;
  return 0;
}

struct gab_obj *eg_defaultalloc(struct gab_triple gab, struct gab_obj *obj,
                                uint64_t size) {
  if (size == 0) {
    assert(obj);

    free(obj);

    return nullptr;
  }

  assert(!obj);

  return malloc(size);
}

struct gab_triple gab_create(struct gab_create_argt args) {
  struct gab_eg *eg = NEW(struct gab_eg);
  memset(eg, 0, sizeof(struct gab_eg));

  assert(args.os_dynsymbol);
  assert(args.os_dynopen);

  eg->os_dynsymbol = args.os_dynsymbol;
  eg->os_dynopen = args.os_dynopen;

  eg->alive = true;
  eg->hash_seed = time(nullptr);

  if (args.os_objalloc)
    eg->os_objalloc = args.os_objalloc;
  else
    eg->os_objalloc = eg_defaultalloc;

  d_gab_src_create(&eg->sources, 8);

  gab_gccreate(&eg->gc);

  struct gab_triple gab = {.eg = eg, .flags = args.flags, .wkid = GAB_NWORKERS};

  struct gab_wk *gc_wk = &gab.eg->workers[GAB_NWORKERS];
  gc_wk->epoch = 0;
  v_gab_obj_create(&gc_wk->lock_keep, 8);
  gc_wk->locked = 0;
  size_t res = thrd_create(&gc_wk->td, gc_thread, gab.eg);

  if (res != thrd_success) {
    printf("UHOH\n");
    exit(1);
  }

  eg->shapes = __gab_shape(gab, 0);
  eg->messages = gab_record(gab, 0, 0, &eg->shapes, &eg->shapes);

  eg->types[kGAB_UNDEFINED] = gab_undefined;

  eg->types[kGAB_NUMBER] = gab_string(gab, "gab.number");
  gab_iref(gab, eg->types[kGAB_NUMBER]);

  eg->types[kGAB_STRING] = gab_string(gab, "gab.string");
  gab_iref(gab, eg->types[kGAB_STRING]);

  eg->types[kGAB_SIGIL] = gab_string(gab, "gab.sigil");
  gab_iref(gab, eg->types[kGAB_SIGIL]);

  eg->types[kGAB_MESSAGE] = gab_string(gab, "gab.message");
  gab_iref(gab, eg->types[kGAB_MESSAGE]);

  eg->types[kGAB_PROTOTYPE] = gab_string(gab, "gab.prototype");
  gab_iref(gab, eg->types[kGAB_PROTOTYPE]);

  eg->types[kGAB_NATIVE] = gab_string(gab, "gab.native");
  gab_iref(gab, eg->types[kGAB_NATIVE]);

  eg->types[kGAB_BLOCK] = gab_string(gab, "gab.block");
  gab_iref(gab, eg->types[kGAB_BLOCK]);

  eg->types[kGAB_SHAPE] = gab_string(gab, "gab.shape");
  gab_iref(gab, eg->types[kGAB_SHAPE]);

  eg->types[kGAB_RECORD] = gab_string(gab, "gab.record");
  gab_iref(gab, eg->types[kGAB_RECORD]);

  eg->types[kGAB_BOX] = gab_string(gab, "gab.box");
  gab_iref(gab, eg->types[kGAB_BOX]);

  eg->types[kGAB_PRIMITIVE] = gab_string(gab, "gab.primitive");
  gab_iref(gab, eg->types[kGAB_PRIMITIVE]);

  gab_negkeep(gab.eg, kGAB_NKINDS, eg->types);

  for (int i = 0; i < LEN_CARRAY(kind_primitives); i++) {
    gab_egkeep(gab.eg,
               gab_iref(gab, gab_spec(gab, (struct gab_spec_argt){
                                               .name = kind_primitives[i].name,
                                               .receiver = gab_egtype(
                                                   eg, kind_primitives[i].kind),
                                               .specialization =
                                                   kind_primitives[i].primitive,
                                           })));
  }

  for (int i = 0; i < LEN_CARRAY(type_primitives); i++) {
    gab_egkeep(
        gab.eg,
        gab_iref(gab, gab_spec(gab, (struct gab_spec_argt){
                                        .name = type_primitives[i].name,
                                        .receiver = type_primitives[i].type,
                                        .specialization =
                                            type_primitives[i].primitive,
                                    })));
  }

  for (int i = 0; i < LEN_CARRAY(all_primitives); i++) {
    for (int t = 1; t < kGAB_NKINDS; t++) {
      gab_egkeep(
          gab.eg,
          gab_iref(gab, gab_spec(gab, (struct gab_spec_argt){
                                          .name = all_primitives[i].name,
                                          .receiver = gab_egtype(eg, t),
                                          .specialization =
                                              all_primitives[i].primitive,
                                      })));
    }
  }

  v_gab_fb_create(&gab.eg->queue, 32);
  mtx_init(&gab.eg->queue_mtx, mtx_plain);

  for (int i = 0; i < GAB_NWORKERS; i++) {
    struct gab_wk *wk = &gab.eg->workers[i];
    v_gab_obj_create(&wk->lock_keep, 8);
    wk->epoch = 0;
    size_t res = thrd_create(&wk->td, worker_thread, gab.eg);
    if (res != thrd_success) {
      printf("UHOH\n");
      exit(1);
    }
    gab.eg->nworkers++;
  }

  if (!(gab.flags & fGAB_NO_CORE))
    gab_suse(gab, "core");

  return gab;
}

void gab_destroy(struct gab_triple gab) {
  gab_ndref(gab, 1, gab.eg->scratch.len, gab.eg->scratch.data);
  gab.eg->messages = gab_undefined;
  gab.eg->shapes = gab_undefined;

  for (uint64_t i = 0; i < gab.eg->modules.cap; i++) {
    if (d_gab_modules_iexists(&gab.eg->modules, i)) {
      a_gab_value *module = d_gab_modules_ival(&gab.eg->modules, i);
      free(module);
    }
  }

  gab_collect(gab);
  while (gab.eg->gc.schedule >= 0)
    ;

  gab.eg->alive = false;
  while (gab.eg->nworkers > 0)
    continue;

  thrd_join(gab.eg->workers[GAB_NWORKERS].td, nullptr);

  gab_gcdestroy(&gab.eg->gc);

  for (uint64_t i = 0; i < gab.eg->sources.cap; i++) {
    if (d_gab_src_iexists(&gab.eg->sources, i)) {
      struct gab_src *s = d_gab_src_ival(&gab.eg->sources, i);
      gab_srcdestroy(s);
    }
  }

  d_strings_destroy(&gab.eg->strings);
  d_gab_modules_destroy(&gab.eg->modules);
  d_gab_src_destroy(&gab.eg->sources);

  v_gab_value_destroy(&gab.eg->scratch);
  v_gab_fb_destroy(&gab.eg->queue);
  mtx_destroy(&gab.eg->queue_mtx);
  free(gab.eg);
}

void gab_repl(struct gab_triple gab, struct gab_repl_argt args) {
  a_gab_value *prev = nullptr;

  size_t iterations = 0;

  for (;;) {
    printf("%s", args.prompt_prefix);
    a_char *src = gab_fosreadl(stdin);

    if (src->data[0] == EOF) {
      a_char_destroy(src);

      if (prev)
        a_gab_value_destroy(prev);

      return;
    }

    if (src->data[1] == '\0') {
      a_char_destroy(src);
      continue;
    }

    size_t prev_len = prev == nullptr ? 0 : prev->len;

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

    if (result == nullptr)
      continue;

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

    a_char_destroy(src);
    prev = result;
  }
}

a_gab_value *gab_exec(struct gab_triple gab, struct gab_exec_argt args) {
  gab_value main = gab_build(gab, (struct gab_build_argt){
                                      .name = args.name,
                                      .source = args.source,
                                      .flags = args.flags,
                                      .len = args.len,
                                      .argv = args.sargv,
                                  });

  if (main == gab_undefined || args.flags & fGAB_BUILD_CHECK) {
    return nullptr;
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
  gab_gclock(gab);

  for (size_t i = 0; i < len; i++) {
    if (gab_spec(gab, args[i]) == gab_undefined) {
      gab_gcunlock(gab);
      return i;
    }
  }

  gab_gcunlock(gab);
  return -1;
}

gab_value gab_spec(struct gab_triple gab, struct gab_spec_argt args) {
  gab_gclock(gab);

  gab_value m = gab_message(gab, args.name);
  m = gab_egmsgput(gab, m, args.receiver, args.specialization);

  gab_gcunlock(gab);

  return m;
}

struct gab_obj_string *gab_egstrfind(struct gab_eg *self, s_char str,
                                     uint64_t hash) {
  if (self->strings.len == 0)
    return nullptr;

  uint64_t index = hash & (self->strings.cap - 1);

  for (;;) {
    d_status status = d_strings_istatus(&self->strings, index);
    struct gab_obj_string *key = d_strings_ikey(&self->strings, index);

    switch (status) {
    case D_TOMBSTONE:
      break;
    case D_EMPTY:
      return nullptr;
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

a_gab_value *gab_segmodat(struct gab_eg *eg, const char *name) {
  size_t hash = s_char_hash(s_char_cstr(name), eg->hash_seed);

  return d_gab_modules_read(&eg->modules, hash);
}

a_gab_value *gab_segmodput(struct gab_eg *eg, const char *name, gab_value mod,
                           size_t len, gab_value values[len]) {
  size_t hash = s_char_hash(s_char_cstr(name), eg->hash_seed);

  if (d_gab_modules_exists(&eg->modules, hash))
    return nullptr;

  a_gab_value *module = a_gab_value_empty(len + 1);
  module->data[0] = mod;
  memcpy(module->data + 1, values, len * sizeof(gab_value));

  d_gab_modules_insert(&eg->modules, hash, module);
  return module;
}

size_t gab_egkeep(struct gab_eg *gab, gab_value v) {
  return gab_negkeep(gab, 1, &v);
}

size_t gab_negkeep(struct gab_eg *gab, size_t len,
                   gab_value values[static len]) {
  for (uint64_t i = 0; i < len; i++)
    if (gab_valiso(values[i]))
      v_gab_value_push(&gab->scratch, values[i]);

  return len;
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
                                      .copier = self->do_copy,
                                  });

    if (self->do_copy)
      self->do_copy(gab_boxlen(copy), gab_boxdata(copy));

    return copy;
  }

  case kGAB_MESSAGE: {
    return gab_strtomsg(
        gab_nstring(gab, gab_strlen(value), gab_strdata(&value)));
  }

  case kGAB_SIGIL: {
    return gab_strtosig(
        gab_nstring(gab, gab_strlen(value), gab_strdata(&value)));
  }

  case kGAB_STRING: {
    return gab_nstring(gab, gab_strlen(value), gab_strdata(&value));
  }

  case kGAB_NATIVE: {
    struct gab_obj_native *self = GAB_VAL_TO_NATIVE(value);
    return gab_native(gab, gab_valcpy(gab, self->name), self->function);
  }

  case kGAB_PROTOTYPE: {
    struct gab_obj_prototype *self = GAB_VAL_TO_PROTOTYPE(value);

    gab_value copy =
        gab_prototype(gab, gab_srccpy(gab, self->src), self->offset, self->len,
                      (struct gab_prototype_argt){
                          .nupvalues = self->nupvalues,
                          .nslots = self->nslots,
                          .narguments = self->narguments,
                          .nlocals = self->nlocals,
                          .data = self->data,
                      });

    return copy;
  }

  case kGAB_BLOCK: {
    struct gab_obj_block *self = GAB_VAL_TO_BLOCK(value);

    gab_value p_copy = gab_valcpy(gab, self->p);

    gab_value copy = gab_block(gab, p_copy);

    for (uint8_t i = 0; i < GAB_VAL_TO_PROTOTYPE(p_copy)->nupvalues; i++) {
      GAB_VAL_TO_BLOCK(copy)->upvalues[i] = gab_valcpy(gab, self->upvalues[i]);
    }

    return copy;
  }
  }
}

gab_value gab_string(struct gab_triple gab, const char data[static 1]) {
  return gab_nstring(gab, strlen(data), data);
}

gab_value gab_tuple(struct gab_triple gab, uint64_t size,
                    gab_value values[size]) {
  gab_gclock(gab);

  gab_value keys[size];
  for (size_t i = 0; i < size; i++) {
    keys[i] = gab_number(i);
  }

  gab_value v = gab_record(gab, 1, size, keys, values);
  gab_gcunlock(gab);
  return v;
}

int gab_fprintf(FILE *stream, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);

  int res = gab_vfprintf(stream, fmt, va);

  va_end(va);

  return res;
}

// TODO: Bounds check this
int gab_afprintf(FILE *stream, const char *fmt, size_t argc,
                 gab_value argv[argc]) {
  const char *c = fmt;
  int bytes = 0;
  size_t i = 0;

  while (*c != '\0') {
    switch (*c) {
    case '$': {
      gab_value arg = argv[i++];
      int idx = gab_valkind(arg) % GAB_COLORS_LEN;
      const char *color = ANSI_COLORS[idx];
      bytes += fprintf(stream, "%s", color);
      bytes += gab_fvalinspect(stream, arg, 1);
      bytes += fprintf(stream, GAB_RESET);
      break;
    }
    default:
      bytes += fputc(*c, stream);
    }
    c++;
  }

  return bytes;
}
int gab_vfprintf(FILE *stream, const char *fmt, va_list varargs) {
  const char *c = fmt;
  int bytes = 0;

  while (*c != '\0') {
    switch (*c) {
    case '$': {
      gab_value arg = va_arg(varargs, gab_value);
      int idx = gab_valkind(arg) % GAB_COLORS_LEN;
      const char *color = ANSI_COLORS[idx];
      bytes += fprintf(stream, "%s", color);
      bytes += gab_fvalinspect(stream, arg, 1);
      bytes += fprintf(stream, GAB_RESET);
      break;
    }
    default:
      bytes += fputc(*c, stream);
    }
    c++;
  }

  return bytes;
}

void dump_pretty_err(struct gab_triple gab, FILE *stream, va_list varargs,
                     struct gab_err_argt args) {
  gab_value tok_name = gab_string(
      gab,
      args.src
          ? gab_token_names[v_gab_token_val_at(&args.src->tokens, args.tok)]
          : "C");

  gab_value src_name = args.src ? args.src->name : gab_string(gab, "C");

  gab_fprintf(stream, "\n[$] $ panicked near $", src_name, args.message,
              tok_name);

  if (args.status != GAB_NONE) {
    gab_value status_name = gab_string(gab, gab_status_names[args.status]);
    gab_fprintf(stream, ": $.\n", status_name);
  }

  if (args.src) {
    s_char tok_src = v_s_char_val_at(&args.src->token_srcs, args.tok);
    const char *tok_start = tok_src.data;
    const char *tok_end = tok_src.data + tok_src.len;

    size_t line = v_uint64_t_val_at(&args.src->token_lines, args.tok);

    s_char line_src = v_s_char_val_at(&args.src->lines, line - 1);

    while (*line_src.data == ' ' || *line_src.data == '\t') {
      line_src.data++;
      line_src.len--;
      if (line_src.len == 0)
        break;
    }

    a_char *under_src = a_char_empty(line_src.len);

    for (uint8_t i = 0; i < under_src->len; i++) {
      if (line_src.data + i >= tok_start && line_src.data + i < tok_end)
        under_src->data[i] = '^';
      else
        under_src->data[i] = ' ';
    }

    fprintf(stream,
            "\n\n" GAB_RED "%.4lu" GAB_RESET "| %.*s"
            "\n      " GAB_YELLOW "%.*s" GAB_RESET "",
            line, (int)line_src.len, line_src.data, (int)under_src->len,
            under_src->data);

    a_char_destroy(under_src);
  }

  if (args.note_fmt && strlen(args.note_fmt) > 0) {
    fprintf(stream, "\n\n");
    gab_vfprintf(stream, args.note_fmt, varargs);
  }

  fprintf(stream, "\n\n");
};

void dump_structured_err(struct gab_triple gab, FILE *stream, va_list varargs,
                         struct gab_err_argt args) {
  const char *tok_name =
      args.src
          ? gab_token_names[v_gab_token_val_at(&args.src->tokens, args.tok)]
          : "C";

  const char *src_name = args.src ? gab_valintocs(gab, args.src->name) : "C";

  const char *msg_name = gab_valintocs(gab, args.message);

  const char *status_name = gab_status_names[args.status];

  fprintf(stream, "%s:%s:%s:%s", status_name, src_name, tok_name, msg_name);

  if (args.src) {
    size_t line = v_uint64_t_val_at(&args.src->token_lines, args.tok);

    s_char line_src = v_s_char_val_at(&args.src->lines, line - 1);
    s_char tok_src = v_s_char_val_at(&args.src->token_srcs, args.tok);
    size_t line_relative_start = tok_src.data - line_src.data;
    size_t line_relative_end = tok_src.data + tok_src.len - line_src.data;

    size_t src_relative_start = tok_src.data - args.src->source->data;
    size_t src_relative_end =
        tok_src.data + tok_src.len - args.src->source->data;

    fprintf(stream, ":%lu:%lu:%lu:%lu:%lu", line, line_relative_start,
            line_relative_end, src_relative_start, src_relative_end);
  }

  fputc('\n', stream);
}

void gab_vfpanic(struct gab_triple gab, FILE *stream, va_list varargs,
                 struct gab_err_argt args) {
  if (gab.flags & fGAB_ERR_QUIET)
    goto fin;

  if (gab.flags & fGAB_ERR_STRUCTURED)
    dump_structured_err(gab, stream, varargs, args);
  else
    dump_pretty_err(gab, stream, varargs, args);

fin:
  if (gab.flags & fGAB_ERR_EXIT)
    exit(1);
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

#define MODULE_SYMBOL "gab_lib"

typedef a_gab_value *(*handler_f)(struct gab_triple, const char *);

typedef a_gab_value *(*module_f)(struct gab_triple);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

a_gab_value *gab_shared_object_handler(struct gab_triple gab,
                                       const char *path) {
  void *handle = gab.eg->os_dynopen(path);

  if (handle == nullptr) {
    gab_panic(gab, "Couldn't open module");
    return nullptr;
  }

  module_f symbol = gab.eg->os_dynsymbol(handle, MODULE_SYMBOL);

  if (!symbol) {
    gab_panic(gab, "Missing symbol " MODULE_SYMBOL);
    return nullptr;
  }

  gab_gclock(gab);
  a_gab_value *res = symbol(gab);
  gab_gcunlock(gab);

  if (res) {
    a_gab_value *final =
        gab_segmodput(gab.eg, path, gab_nil, res->len, res->data);

    free(res);

    return final;
  }

  return gab_segmodput(gab.eg, path, gab_nil, 0, nullptr);
}

a_gab_value *gab_source_file_handler(struct gab_triple gab, const char *path) {
  a_char *src = gab_osread(path);

  if (src == nullptr)
    return gab_panic(gab, "Failed to load module");

  gab_value pkg = gab_build(gab, (struct gab_build_argt){
                                     .name = path,
                                     .source = (const char *)src->data,
                                     .flags = gab.flags | fGAB_ERR_EXIT,
                                     .len = 0,
                                 });

  a_char_destroy(src);

  a_gab_value *res = gab_run(gab, (struct gab_run_argt){
                                      .main = pkg,
                                      .flags = gab.flags | fGAB_ERR_EXIT,
                                      .len = 0,
                                  });

  if (res == nullptr || res->data[0] != gab_ok)
    return gab_panic(gab, "Failed to load module");

  gab_negkeep(gab.eg, res->len - 1, res->data + 1);

  a_gab_value *final =
      gab_segmodput(gab.eg, path, pkg, res->len - 1, res->data + 1);

  free(res);

  return final;
}

#ifndef GAB_PREFIX
#define GAB_PREFIX "."
#endif

resource resources[] = {
    // Local resources
    {
        .prefix = "./mod/",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = "/mod/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = "/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./libcgab",
        .suffix = ".so",
        .handler = gab_shared_object_handler,
    },
    // Installed resources
    {
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = "/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = "/mod/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/libcgab",
        .suffix = ".so",
        .handler = gab_shared_object_handler,
    },
};

a_char *match_resource(resource *res, const char *name, uint64_t len) {
  const uint64_t p_len = strlen(res->prefix);
  const uint64_t s_len = strlen(res->suffix);
  const uint64_t total_len = p_len + len + s_len + 1;

  char buffer[total_len];

  memcpy(buffer, res->prefix, p_len);
  memcpy(buffer + p_len, name, len);
  memcpy(buffer + p_len + len, res->suffix, s_len + 1);

  FILE *f = fopen(buffer, "r");

  if (!f)
    return nullptr;

  fclose(f);
  return a_char_create(buffer, total_len);
}

a_gab_value *gab_suse(struct gab_triple gab, const char *name) {
  return gab_use(gab, gab_string(gab, name));
}

a_gab_value *gab_use(struct gab_triple gab, gab_value path) {
  assert(gab_valkind(path) == kGAB_STRING);

  const char *name = gab_valintocs(gab, path);

  for (int j = 0; j < sizeof(resources) / sizeof(resource); j++) {
    resource *res = resources + j;
    a_char *path = match_resource(res, name, strlen(name));

    if (path) {
      a_gab_value *cached = gab_segmodat(gab.eg, (char *)path->data);

      if (cached != nullptr) {
        /* Skip the first argument, which is the module's data */
        a_char_destroy(path);
        return cached;
      }

      a_gab_value *result = res->handler(gab, (char *)path->data);

      if (result != nullptr) {
        /* Skip the first argument, which is the module's data */
        a_char_destroy(path);
        return result;
      }
    }
  }

  return nullptr;
}
