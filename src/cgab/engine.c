#include <errno.h>
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

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

struct primitive {
  const char *name;
  union {
    enum gab_kind kind;
    const char *sigil;
  };
  gab_value primitive;
};

struct primitive all_primitives[] = {
    {
        .name = mGAB_TYPE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_TYPE),
    },
};

struct primitive sigil_primitives[] = {
    {
        .name = mGAB_CALL,
        .sigil = "gab.list",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LIST),
    },
    {
        .name = mGAB_CALL,
        .sigil = "gab.fiber",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_FIBER),
    },
    {
        .name = mGAB_CALL,
        .sigil = "gab.record",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_RECORD),
    },
    {
        .name = mGAB_CALL,
        .sigil = "gab.channel",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CHANNEL),
    },
    {
        .name = mGAB_CALL,
        .sigil = "gab.channel",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CHANNEL),
    },
    {
        .name = mGAB_CALL,
        .sigil = "gab.channel",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_CHANNEL),
    },
    {
        .name = mGAB_BND,
        .sigil = "false",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LND),
    },
    {
        .name = mGAB_BOR,
        .sigil = "false",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LOR),
    },
    {
        .name = mGAB_LIN,
        .sigil = "false",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LIN),
    },
    {
        .name = mGAB_BND,
        .sigil = "true",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LND),
    },
    {
        .name = mGAB_BOR,
        .sigil = "true",
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_LOR),
    },
    {
        .name = mGAB_LIN,
        .sigil = "true",
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
    {
        .name = mGAB_PUT,
        .kind = kGAB_CHANNEL,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_PUT),
    },
    {
        .name = mGAB_TAKE,
        .kind = kGAB_CHANNEL,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_TAKE),
    },
};

static const struct timespec t = {.tv_nsec = GAB_YIELD_SLEEPTIME_NS};

void gab_yield(struct gab_triple gab) {
  if (gab.eg->gc->schedule == gab.wkid)
    gab_gcepochnext(gab);

  thrd_sleep(&t, nullptr);
  thrd_yield();
}

int32_t wkid(struct gab_eg *eg) {
  for (size_t i = 0; i < eg->len; i++) {
    struct gab_jb *wk = &eg->jobs[i];
    if (wk->td == thrd_current())
      return i;
  }

  return -1;
}

int32_t gc_job(void *data) {
  struct gab_triple *g = data;
  struct gab_triple gab = *g;
  gab.wkid = gab.eg->len - 1;

  gab.eg->jobs[gab.wkid].fiber = gab_undefined;

  while (gab.eg->njobs >= 0) {
    if (gab.eg->gc->schedule == gab.wkid)
      gab_gcdocollect(gab);
  }

  free(g);
  return 0;
}

int32_t worker_job(void *data) {
  struct gab_triple *g = data;
  struct gab_triple gab = *g;
  gab.wkid = wkid(gab.eg);

  gab.eg->njobs++;

  while (!gab_chnisclosed(gab.eg->work_channel)) {
    gab_value fiber = gab_chntake(gab, gab.eg->work_channel);

    // we get undefined if:
    //  - the channel is closed
    //  - we timed out
    if (fiber == gab_undefined)
      break;

    gab.eg->jobs[gab.wkid].fiber = fiber;

    gab_vmexec(gab, fiber);

    gab.eg->jobs[gab.wkid].fiber = gab_undefined;
  }

  gab.eg->jobs[gab.wkid].alive = false;
  gab.eg->jobs[gab.wkid].fiber = gab_undefined;
  gab.eg->njobs--;

  assert(gab.eg->jobs[gab.wkid].locked == 0);

  free(g);

  return 0;
}

struct gab_jb *next_available_job(struct gab_triple gab) {

  // Try to reuse an existing job, thats exited after idling
  for (size_t i = 0; i < gab.eg->len; i++) {
    // If we have a dead thread, revive it
    if (!gab.eg->jobs[i].alive)
      return gab.eg->jobs + i;
  }

  // No room for new jobs
  return nullptr;
}

bool gab_jbcreate(struct gab_triple gab, struct gab_jb *job, int(fn)(void *)) {
  if (!job)
    return false;

  job->epoch = 0;
  job->locked = 0;
  job->fiber = gab_undefined;
  job->alive = true;
  v_gab_obj_create(&job->lock_keep, 8);

  struct gab_triple *gabcpy = malloc(sizeof(struct gab_triple));
  memcpy(gabcpy, &gab, sizeof(struct gab_triple));

  size_t res = thrd_create(&job->td, fn, gabcpy);

  if (res != thrd_success) {
    printf("UHOH\n");
    exit(1);
  }

  return true;
}

struct gab_triple gab_create(struct gab_create_argt args) {
  size_t njobs = args.jobs ? args.jobs : 8;

  size_t egsize = sizeof(struct gab_eg) + sizeof(struct gab_jb) * (njobs + 1);

  struct gab_eg *eg = malloc(egsize);
  memset(eg, 0, egsize);

  assert(args.os_dynsymbol);
  assert(args.os_dynopen);

  eg->len = njobs + 1;
  eg->njobs = 0;
  eg->os_dynsymbol = args.os_dynsymbol;
  eg->os_dynopen = args.os_dynopen;
  eg->hash_seed = time(nullptr);

  mtx_init(&eg->shapes_mtx, mtx_plain);
  mtx_init(&eg->sources_mtx, mtx_plain);
  mtx_init(&eg->strings_mtx, mtx_plain);
  mtx_init(&eg->modules_mtx, mtx_plain);

  d_gab_src_create(&eg->sources, 8);

  struct gab_triple gab = {.eg = eg, .flags = args.flags, .wkid = njobs};

  size_t gcsize =
      sizeof(struct gab_gc) +
      sizeof(struct gab_gcbuf[kGAB_NBUF][GAB_GCNEPOCHS]) * (njobs + 1);

  eg->gc = malloc(gcsize);
  gab_gccreate(gab);

  gab_jbcreate(gab, gab.eg->jobs + njobs, gc_job);

  eg->shapes = __gab_shape(gab, 0);
  eg->messages = gab_record(gab, 0, 0, &eg->shapes, &eg->shapes);
  eg->macros = gab_record(gab, 0, 0, &eg->shapes, &eg->shapes);
  eg->work_channel = gab_channel(gab, 0);

  gab_iref(gab, eg->work_channel);

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

  eg->types[kGAB_SHAPELIST] = gab_string(gab, "gab.shape");
  gab_iref(gab, eg->types[kGAB_SHAPELIST]);

  eg->types[kGAB_RECORD] = gab_string(gab, "gab.record");
  gab_iref(gab, eg->types[kGAB_RECORD]);

  eg->types[kGAB_BOX] = gab_string(gab, "gab.box");
  gab_iref(gab, eg->types[kGAB_BOX]);

  eg->types[kGAB_FIBER] = gab_string(gab, "gab.fiber");
  gab_iref(gab, eg->types[kGAB_FIBER]);

  eg->types[kGAB_CHANNEL] = gab_string(gab, "gab.channel");
  gab_iref(gab, eg->types[kGAB_CHANNEL]);

  eg->types[kGAB_CHANNELCLOSED] = gab_string(gab, "gab.channel");
  gab_iref(gab, eg->types[kGAB_CHANNELCLOSED]);

  eg->types[kGAB_CHANNELBUFFERED] = gab_string(gab, "gab.channel");
  gab_iref(gab, eg->types[kGAB_CHANNELBUFFERED]);

  eg->types[kGAB_PRIMITIVE] = gab_string(gab, "gab.primitive");
  gab_iref(gab, eg->types[kGAB_PRIMITIVE]);

  gab_negkeep(gab.eg, kGAB_NKINDS, eg->types);

  for (int i = 0; i < LEN_CARRAY(kind_primitives); i++) {
    gab_egkeep(
        gab.eg,
        gab_iref(gab,
                 gab_def(gab, (struct gab_def_argt){
                                  gab_message(gab, kind_primitives[i].name),
                                  gab_type(gab, kind_primitives[i].kind),
                                  kind_primitives[i].primitive,
                              })));
  }

  for (int i = 0; i < LEN_CARRAY(sigil_primitives); i++) {
    gab_egkeep(
        gab.eg,
        gab_iref(gab,
                 gab_def(gab, (struct gab_def_argt){
                                  gab_message(gab, sigil_primitives[i].name),
                                  gab_sigil(gab, sigil_primitives[i].sigil),
                                  sigil_primitives[i].primitive,
                              })));
  }

  for (int i = 0; i < LEN_CARRAY(all_primitives); i++) {
    for (int t = 1; t < kGAB_NKINDS; t++) {
      gab_egkeep(
          gab.eg,
          gab_iref(gab,
                   gab_def(gab, (struct gab_def_argt){
                                    gab_message(gab, all_primitives[i].name),
                                    gab_type(gab, t),

                                    all_primitives[i].primitive,
                                })));
    }
  }

  gab_suse(gab, "cassignment");

  if (!(gab.flags & fGAB_ENV_EMPTY))
    gab_suse(gab, "core");

  return gab;
}

void gab_destroy(struct gab_triple gab) {
  gab_chnclose(gab.eg->work_channel);

  while (gab.eg->njobs > 0)
    continue;

  gab_dref(gab, gab.eg->work_channel);
  gab_ndref(gab, 1, gab.eg->scratch.len, gab.eg->scratch.data);

  gab.eg->messages = gab_undefined;
  gab.eg->macros = gab_undefined;
  gab.eg->shapes = gab_undefined;

  gab_collect(gab);
  while (gab.eg->gc->schedule >= 0)
    ;

  gab.eg->njobs = -1;

  thrd_join(gab.eg->jobs[gab.eg->len - 1].td, nullptr);
  gab_gcdestroy(gab);
  free(gab.eg->gc);

  for (uint64_t i = 0; i < gab.eg->modules.cap; i++) {
    if (d_gab_modules_iexists(&gab.eg->modules, i)) {
      a_gab_value *module = d_gab_modules_ival(&gab.eg->modules, i);
      free(module);
    }
  }

  for (uint64_t i = 0; i < gab.eg->sources.cap; i++) {
    if (d_gab_src_iexists(&gab.eg->sources, i)) {
      struct gab_src *s = d_gab_src_ival(&gab.eg->sources, i);
      gab_srcdestroy(s);
    }
  }

  for (int i = 0; i < gab.eg->len; i++) {
    struct gab_jb *wk = &gab.eg->jobs[i];
    v_gab_obj_destroy(&wk->lock_keep);
  }

  d_strings_destroy(&gab.eg->strings);
  d_gab_modules_destroy(&gab.eg->modules);
  d_gab_src_destroy(&gab.eg->sources);

  v_gab_value_destroy(&gab.eg->scratch);

  mtx_destroy(&gab.eg->shapes_mtx);
  mtx_destroy(&gab.eg->strings_mtx);
  mtx_destroy(&gab.eg->sources_mtx);
  mtx_destroy(&gab.eg->modules_mtx);

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

gab_value dodef(struct gab_triple gab, gab_value messages, size_t len,
                struct gab_def_argt args[static len]) {

  gab_gclock(gab);

  for (size_t i = 0; i < len; i++) {
    struct gab_def_argt arg = args[i];

    gab_value specs = gab_recat(messages, arg.message);

    if (specs == gab_undefined)
      specs = gab_record(gab, 0, 0, &specs, &specs);

    gab_value newspecs =
        gab_recput(gab, specs, arg.receiver, arg.specialization);

    messages = gab_recput(gab, messages, arg.message, newspecs);
  }

  return gab_gcunlock(gab), messages;
}

int gab_ndef(struct gab_triple gab, size_t len,
             struct gab_def_argt args[static len]) {
  gab_gclock(gab);

  gab_value m = dodef(gab, gab.eg->messages, len, args);

  if (m == gab_undefined)
    return false;

  gab.eg->messages = m;

  gab_gcunlock(gab);
  return true;
}

int gab_ndefmacro(struct gab_triple gab, size_t len,
                  struct gab_defmacro_argt args[static len]) {
  gab_gclock(gab);

  for (size_t i = 0; i < len; i++) {
    struct gab_defmacro_argt arg = args[i];

    gab_value spec = gab_recat(gab.eg->macros, arg.message);

    if (spec != gab_undefined)
      return false;

    gab.eg->macros =
        gab_recput(gab, gab.eg->macros, arg.message, arg.specialization);
  }

  gab_gcunlock(gab);
  return true;
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

  mtx_lock(&eg->modules_mtx);

  a_gab_value *module = d_gab_modules_read(&eg->modules, hash);

  mtx_unlock(&eg->modules_mtx);

  return module;
}

a_gab_value *gab_segmodput(struct gab_eg *eg, const char *name, gab_value mod,
                           size_t len, gab_value values[len]) {
  size_t hash = s_char_hash(s_char_cstr(name), eg->hash_seed);

  mtx_lock(&eg->modules_mtx);

  if (d_gab_modules_exists(&eg->modules, hash))
    return mtx_unlock(&eg->modules_mtx), nullptr;

  a_gab_value *module = a_gab_value_empty(len + 1);
  module->data[0] = mod;
  memcpy(module->data + 1, values, len * sizeof(gab_value));

  d_gab_modules_insert(&eg->modules, hash, module);
  return mtx_unlock(&eg->modules_mtx), module;
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

int gab_fprintf(FILE *stream, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);

  int res = gab_vfprintf(stream, fmt, va);

  va_end(va);

  return res;
}

// TODO: Bounds check this
int gab_nfprintf(FILE *stream, const char *fmt, size_t argc,
                 gab_value argv[argc]) {
  const char *c = fmt;
  int bytes = 0;
  size_t i = 0;

  while (*c != '\0') {
    switch (*c) {
    case '$': {
      if (i < argc) {
        gab_value arg = argv[i++];
        int idx = gab_valkind(arg) % GAB_COLORS_LEN;
        const char *color = ANSI_COLORS[idx];
        bytes += fprintf(stream, "%s", color);
        bytes += gab_fvalinspect(stream, arg, 1);
        bytes += fprintf(stream, GAB_RESET);
        break;
      }
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
    gab_fprintf(stream, ": $.", status_name);
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

  const char *src_name = args.src ? gab_strdata(&args.src->name) : "C";

  const char *msg_name = gab_strdata(&args.message);

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
    gab_fpanic(gab, "Couldn't open module");
    return nullptr;
  }

  module_f symbol = gab.eg->os_dynsymbol(handle, MODULE_SYMBOL);

  if (!symbol) {
    gab_fpanic(gab, "Missing symbol " MODULE_SYMBOL);
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

  if (src == nullptr) {
    gab_value reason = gab_string(gab, strerror(errno));
    return gab_fpanic(gab, "Failed to load module: $", reason);
  }

  gab_value pkg = gab_build(gab, (struct gab_build_argt){
                                     .name = path,
                                     .source = (const char *)src->data,
                                     .flags = gab.flags | fGAB_ERR_EXIT,
                                     .len = 0,
                                 });

  a_char_destroy(src);

  gab_value fb = gab_arun(gab, (struct gab_run_argt){
                                   .main = pkg,
                                   .flags = gab.flags | fGAB_ERR_EXIT,
                                   .len = 0,
                               });

  if (fb == gab_undefined)
    return nullptr;

  struct gab_obj_fiber *f = GAB_VAL_TO_FIBER(fb);

  a_gab_value *res = gab_fibawait(gab, fb);

  gab_value fbparent = gab_thisfiber(gab);

  if (fbparent == gab_undefined) {
    gab.eg->messages = f->messages;
    gab.eg->macros = f->macros;
  } else {
    struct gab_obj_fiber *parent = GAB_VAL_TO_FIBER(fbparent);
    parent->messages = f->messages;
    parent->macros = f->macros;
  }

  if (res == nullptr) {
    return gab_fpanic(gab, "Failed to load module: module did not run");
  }

  if (res->data[0] != gab_ok) {
    return gab_fpanic(gab,
                      "Failed to load module: module returned $, expected $",
                      res->data[0], gab_ok);
  }

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
    {
        .prefix = "./build/libcgab",
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

  const char *name = gab_strdata(&path);

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

      a_char_destroy(path);
    }
  }

  return nullptr;
}

a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args) {
  gab_value fb = gab_arun(gab, args);

  if (fb == gab_undefined)
    return nullptr;

  a_gab_value *res = gab_fibawait(gab, fb);
  assert(res != nullptr);

  return res;
}

gab_value gab_arun(struct gab_triple gab, struct gab_run_argt args) {
  gab.flags = args.flags;

  if (gab.flags & fGAB_BUILD_CHECK)
    return gab_undefined;

  gab_value fb = gab_fiber(gab, (struct gab_fiber_argt){
                                    .message = gab_message(gab, mGAB_CALL),
                                    .receiver = args.main,
                                    .argv = args.argv,
                                    .argc = args.len,
                                });

  gab_iref(gab, fb);

  // Somehow check if the put will block, and create a job in that case.
  // Should check to see if the channel has takers waiting already.
  gab_jbcreate(gab, next_available_job(gab), worker_job);

  gab_chnput(gab, gab.eg->work_channel, fb);

  gab_dref(gab, fb);

  return fb;
}

a_gab_value *gab_sendmacro(struct gab_triple gab, struct gab_send_argt args) {
  gab.flags = args.flags;

  gab_value fb = gab_fiber(gab, (struct gab_fiber_argt){
                                    .message = args.message,
                                    .receiver = args.receiver,
                                    .argv = args.argv,
                                    .argc = args.len,
                                    .is_macro = true,
                                });

  if (fb == gab_undefined)
    return nullptr;

  gab_iref(gab, fb);

  gab_jbcreate(gab, next_available_job(gab), worker_job);

  gab_chnput(gab, gab.eg->work_channel, fb);

  a_gab_value *res = gab_fibawait(gab, fb);

  gab_dref(gab, fb);

  return res;
};
