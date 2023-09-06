#include "include/engine.h"
#include "include/builtins.h"
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
#include <unistd.h>

struct primitive {
  const char *name;
  gab_kind type;
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
        .type = kGAB_STRING,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_NUMBER,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_TRUE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_FALSE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_SHAPE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_MESSAGE,
        .primitive = gab_primitive(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_NIL,
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

gab_eg *gab_create(uint64_t argc, gab_value argv_names[argc],
                   gab_value argv_values[argc]) {
  gab_eg *gab = NEW(gab_eg);
  memset(gab, 0, sizeof(gab_eg));

  for (uint64_t i = 0; i < argc; i++) {
    v_gab_value_push(&gab->argv_names, argv_names[i]);
    v_gab_value_push(&gab->argv_values, argv_values[i]);
  }

  memset(gab->types, 0, sizeof(gab->types));

  gab->hash_seed = time(NULL);

  gab->types[kGAB_UNDEFINED] = gab_undefined;
  gab->types[kGAB_NIL] = gab_nil;
  gab->types[kGAB_NUMBER] = gab_string(gab, "Number");
  gab->types[kGAB_TRUE] = gab_string(gab, "bool");
  gab->types[kGAB_FALSE] = gab_string(gab, "bool");
  gab->types[kGAB_STRING] = gab_string(gab, "String");
  gab->types[kGAB_MESSAGE] = gab_string(gab, "Message");
  gab->types[kGAB_BLOCK_PROTO] = gab_string(gab, "Prototype");
  gab->types[kGAB_BUILTIN] = gab_string(gab, "Builtin");
  gab->types[kGAB_BLOCK] = gab_string(gab, "Block");
  gab->types[kGAB_RECORD] = gab_string(gab, "Record");
  gab->types[kGAB_SHAPE] = gab_string(gab, "Shape");
  gab->types[kGAB_BOX] = gab_string(gab, "Box");
  gab->types[kGAB_SUSPENSE] = gab_string(gab, "Suspsense");
  gab->types[kGAB_PRIMITIVE] = gab_string(gab, "Primitive");

  for (int i = 0; i < LEN_CARRAY(primitives); i++) {
    gab_spec(gab, NULL,
             (struct gab_spec_argt){
                 .name = primitives[i].name,
                 .receiver = gab_typ(gab, primitives[i].type),
                 .specialization = primitives[i].primitive,
             });
  }

  gab_setup_builtins(gab);

  return gab;
}

void gab_free(gab_eg *gab) {
  if (!gab)
    return;

  gab_gc *gc = NEW(gab_gc);
  gab_gc_create(gc);

  gab_ngcdref(gc, NULL, kGAB_NKINDS, gab->types);
  gab_ngcdref(gc, NULL, gab->scratch.len, gab->scratch.data);

  for (uint64_t i = 0; i < gab->interned_strings.cap; i++) {
    if (d_strings_iexists(&gab->interned_strings, i)) {
      gab_obj_string *v = d_strings_ikey(&gab->interned_strings, i);
      gab_gcdref(gc, NULL, __gab_obj(v));
    }
  }

  for (uint64_t i = 0; i < gab->interned_shapes.cap; i++) {
    if (d_shapes_iexists(&gab->interned_shapes, i)) {
      gab_obj_shape *v = d_shapes_ikey(&gab->interned_shapes, i);
      gab_gcdref(gc, NULL, __gab_obj(v));
    }
  }

  for (uint64_t i = 0; i < gab->interned_messages.cap; i++) {
    if (d_messages_iexists(&gab->interned_messages, i)) {
      gab_obj_message *v = d_messages_ikey(&gab->interned_messages, i);
      gab_gcdref(gc, NULL, __gab_obj(v));
    }
  }

  while (gab->sources) {
    gab_src *s = gab->sources;
    gab->sources = s->next;
    gab_srcfree(s);
  }

  while (gab->modules) {
    gab_mod *m = gab->modules;
    gab->modules = m->next;
    gab_mod_destroy(gab, gc, m);
  }

  gab_gc_run(gc, NULL);

  gab_impdestroy(gab, gc);

  d_strings_destroy(&gab->interned_strings);
  d_shapes_destroy(&gab->interned_shapes);
  d_messages_destroy(&gab->interned_messages);
  d_gab_imp_destroy(&gab->imports);

  v_gab_value_destroy(&gab->argv_values);
  v_gab_value_destroy(&gab->argv_names);

  v_gab_value_destroy(&gab->scratch);

  gab_gc_destroy(gc);
  DESTROY(gc);
  DESTROY(gab);
}

void gab_collect(gab_eg *gab, gab_vm *vm) { gab_gc_run(&vm->gc, vm); }

uint64_t gab_argpush(gab_eg *gab, gab_value name) {
  v_gab_value_push(&gab->argv_values, gab_undefined);
  return v_gab_value_push(&gab->argv_names, name);
}

void gab_argput(gab_eg *gab, gab_value value, uint64_t index) {
  v_gab_value_set(&gab->argv_values, index, value);
}

void gab_argpop(gab_eg *gab) {
  if (gab->argv_names.len > 0) {
    v_gab_value_pop(&gab->argv_names);
    v_gab_value_pop(&gab->argv_values);
  }
};

a_gab_value *gab_run(gab_eg *gab, gab_value main, size_t flags) {
  return gab_vm_run(gab, main, flags, gab->argv_values.len,
                    gab->argv_values.data);
};

a_gab_value *gab_execute(gab_eg *gab, const char *name, const char *source,
                         size_t flags) {
  gab_value main = gab_block(gab, (struct gab_block_argt){name, source, flags});

  if (main == gab_undefined)
    return NULL;

  return gab_run(gab, main, flags);
}

gab_value gab_panic(gab_eg *gab, gab_vm *vm, const char *msg) {
  gab_value vm_container = gab_vm_panic(gab, vm, msg);
  gab_gcdref(&vm->gc, vm, vm_container);
  return vm_container;
}

gab_value gab_spec(gab_eg *gab, gab_vm *vm, struct gab_spec_argt args) {
  gab_obj_message *m = gab_obj_message_create(gab, gab_string(gab, args.name));

  if (gab_obj_message_find(m, args.receiver) != UINT64_MAX)
    return gab_nil;

  gab_obj_message_insert(m, args.receiver, args.specialization);

  if (vm)
    gab_ngciref(&vm->gc, vm, 2,
                (gab_value[2]){args.receiver, args.specialization});

  return __gab_obj(m);
}

a_gab_value *send_msg(gab_eg *gab, gab_vm *vm, gab_value msg,
                      gab_value receiver, size_t argc, gab_value argv[argc]) {
  if (msg == gab_undefined)
    return a_gab_value_one(gab_undefined);

  gab_value mod =
      gab_bccompsend(gab, msg, receiver, fGAB_DUMP_ERROR, argc, argv);

  if (mod == gab_undefined)
    return a_gab_value_one(gab_undefined);

  gab_egkeep(gab, mod);

  return gab_vm_run(gab, mod, fGAB_DUMP_ERROR, 0, NULL);
}

a_gab_value *gab_send(gab_eg *gab, gab_vm *vm, gab_value msg,
                      gab_value receiver, size_t argc, gab_value argv[argc]) {
  switch (gab_valknd(msg)) {
  case kGAB_STRING: {
    gab_obj_message *main = gab_obj_message_create(gab, msg);
    return send_msg(gab, vm, __gab_obj(main), receiver, argc, argv);
  }
  case kGAB_MESSAGE:
    return send_msg(gab, vm, msg, receiver, argc, argv);
  default:
    return a_gab_value_one(gab_nil);
  }
}

gab_obj_message *gab_eg_find_message(gab_eg *self, gab_value name,
                                     uint64_t hash) {
  if (self->interned_messages.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_messages.cap - 1);

  for (;;) {
    gab_obj_message *key = d_messages_ikey(&self->interned_messages, index);
    d_status status = d_messages_istatus(&self->interned_messages, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && name == key->name) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_messages.cap - 1);
  }
}

gab_obj_string *gab_eg_find_string(gab_eg *self, s_int8_t str, uint64_t hash) {
  if (self->interned_strings.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_strings.cap - 1);

  for (;;) {
    gab_obj_string *key = d_strings_ikey(&self->interned_strings, index);
    d_status status = d_strings_istatus(&self->interned_strings, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && s_int8_t_match(str, gab_obj_string_ref(key))) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_strings.cap - 1);
  }
}

static inline bool shape_matches_keys(gab_obj_shape *self, gab_value values[],
                                      uint64_t len, uint64_t stride) {

  if (self->len != len)
    return false;

  for (uint64_t i = 0; i < len; i++) {
    gab_value key = values[i * stride];
    if (self->data[i] != key)
      return false;
  }

  return true;
}

gab_obj_shape *gab_eg_find_shape(gab_eg *self, uint64_t size, uint64_t stride,
                                 uint64_t hash, gab_value keys[size]) {
  if (self->interned_shapes.len == 0)
    return NULL;

  uint64_t index = hash & (self->interned_shapes.cap - 1);

  for (;;) {
    d_status status = d_shapes_istatus(&self->interned_shapes, index);

    if (status != D_FULL)
      return NULL;

    gab_obj_shape *key = d_shapes_ikey(&self->interned_shapes, index);

    if (key->hash == hash && shape_matches_keys(key, keys, size, stride))
      return key;

    index = (index + 1) & (self->interned_shapes.cap - 1);
  }
}

const char *gab_strerr(gab_eg *gab) { return gab->error; }

gab_value gab_typ(gab_eg *gab, gab_kind k) { return gab->types[k]; }

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

size_t gab_egkeep(gab_eg *gab, gab_value v) {
  v_gab_value_push(&gab->scratch, v);
  return gab->scratch.len;
}

size_t gab_negkeep(gab_eg *gab, size_t len, gab_value *values) {
  for (uint64_t i = 0; i < len; i++)
    v_gab_value_push(&gab->scratch, values[i]);

  return gab->scratch.len;
}

gab_gc *gab_vmgc(gab_vm *vm) { return &vm->gc; }

size_t gab_vmpush(gab_vm *vm, gab_value value) {
  return gab_vm_push(vm, 1, &value);
}

size_t gab_nvmpush(gab_vm *vm, size_t argc, gab_value argv[argc]) {
  return gab_vm_push(vm, argc, argv);
}

gab_value gab_valcpy(gab_eg *gab, gab_vm *vm, gab_value value) {
  switch (gab_valknd(value)) {
  default:
    return value;

  case kGAB_MESSAGE: {
    gab_obj_message *self = GAB_VAL_TO_MESSAGE(value);
    gab_obj_message *copy =
        gab_obj_message_create(gab, gab_valcpy(gab, vm, self->name));

    return __gab_obj(copy);
  }

  case kGAB_STRING: {
    gab_obj_string *self = GAB_VAL_TO_STRING(value);
    return __gab_obj(gab_obj_string_create(gab, gab_obj_string_ref(self)));
  }

  case kGAB_BUILTIN: {
    gab_obj_builtin *self = GAB_VAL_TO_BUILTIN(value);
    return __gab_obj(gab_obj_builtin_create(gab, self->function,
                                            gab_valcpy(gab, vm, self->name)));
  }

  case kGAB_BLOCK_PROTO: {
    gab_obj_block_proto *self = GAB_VAL_TO_BLOCK_PROTO(value);
    gab->modules = gab_mod_copy(gab, self->mod);

    gab_obj_block_proto *copy = gab_obj_prototype_create(
        gab, gab->modules, self->narguments, self->nslots, self->nlocals,
        self->nupvalues, self->upv_desc, self->upv_desc);

    memcpy(copy->upv_desc, self->upv_desc, self->nupvalues * 2);

    return __gab_obj(copy);
  }

  case kGAB_BLOCK: {
    gab_obj_block *self = GAB_VAL_TO_BLOCK(value);

    gab_obj_block_proto *p_copy =
        GAB_VAL_TO_BLOCK_PROTO(gab_valcpy(gab, vm, __gab_obj(self->p)));

    gab_obj_block *copy = gab_obj_block_create(gab, p_copy);

    for (uint8_t i = 0; i < p_copy->nupvalues; i++) {
      copy->upvalues[i] = gab_valcpy(gab, vm, self->upvalues[i]);
    }

    return __gab_obj(copy);
  }

  case kGAB_SHAPE: {
    gab_obj_shape *self = GAB_VAL_TO_SHAPE(value);

    gab_value keys[self->len];

    for (uint64_t i = 0; i < self->len; i++) {
      keys[i] = gab_valcpy(gab, vm, self->data[i]);
    }

    gab_obj_shape *copy = gab_obj_shape_create(gab, vm, self->len, 1, keys);

    return __gab_obj(copy);
  }

  case kGAB_RECORD: {
    gab_obj_record *self = GAB_VAL_TO_RECORD(value);

    gab_obj_shape *s_copy =
        GAB_VAL_TO_SHAPE(gab_valcpy(gab, vm, __gab_obj(self->shape)));

    gab_value values[self->len];

    for (uint64_t i = 0; i < self->len; i++)
      values[i] = gab_valcpy(gab, vm, self->data[i]);

    gab_obj_record *copy = gab_obj_record_create(gab, vm, s_copy, 1, values);

    return __gab_obj(copy);
  }

  case kGAB_SUSPENSE_PROTO: {
    gab_obj_suspense_proto *self = GAB_VAL_TO_SUSPENSE_PROTO(value);

    gab_obj_suspense_proto *copy =
        gab_obj_suspense_proto_create(gab, self->offset, self->want);

    return __gab_obj(copy);
  }

  case kGAB_SUSPENSE: {
    gab_obj_suspense *self = GAB_VAL_TO_SUSPENSE(value);

    gab_value frame[self->len];

    gab_obj_suspense_proto *p_copy =
        GAB_VAL_TO_SUSPENSE_PROTO(gab_valcpy(gab, vm, __gab_obj(self->p)));

    for (uint64_t i = 0; i < self->len; i++)
      frame[i] = gab_valcpy(gab, vm, self->frame[i]);

    gab_obj_block *b_copy =
        GAB_VAL_TO_BLOCK(gab_valcpy(gab, vm, __gab_obj(self->b)));

    gab_obj_suspense *copy =
        gab_obj_suspense_create(gab, vm, self->len, b_copy, p_copy, frame);

    return __gab_obj(copy);
  }
  }
}

gab_value gab_builtin(gab_eg *gab, const char *name, gab_builtin_f f) {
  return __gab_obj(gab_obj_builtin_create(gab, f, gab_string(gab, name)));
}

gab_value gab_string(gab_eg *gab, const char *data) {
  return __gab_obj(gab_obj_string_create(gab, s_int8_t_cstr(data)));
}

gab_value gab_nstring(gab_eg *gab, size_t len, const char *data) {
  return __gab_obj(
      gab_obj_string_create(gab, s_int8_t_create((const int8_t *)data, len)));
}

gab_value gab_block(gab_eg *gab, struct gab_block_argt args) {
  return gab_bccomp(
      gab, gab_string(gab, args.name),
      s_int8_t_create((int8_t *)args.source, strlen(args.source) + 1),
      args.flags, gab->argv_values.len, gab->argv_names.data);
}

gab_value gab_record(gab_eg *gab, gab_vm *vm, uint64_t size,
                     gab_value keys[size], gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create(gab, vm, size, 1, keys);
  return __gab_obj(gab_obj_record_create(gab, vm, bundle_shape, 1, values));
}

gab_value gab_srecord(gab_eg *gab, gab_vm *vm, uint64_t size,
                      const char *keys[size], gab_value values[size]) {
  gab_value value_keys[size];

  for (uint64_t i = 0; i < size; i++)
    value_keys[i] = gab_string(gab, keys[i]);

  gab_obj_shape *bundle_shape =
      gab_obj_shape_create(gab, vm, size, 1, value_keys);

  return __gab_obj(gab_obj_record_create(gab, vm, bundle_shape, 1, values));
}

gab_value gab_erecord(gab_eg *gab, gab_value shape) {
  assert(gab_valknd(shape) == kGAB_SHAPE);
  return __gab_obj(gab_obj_record_create_empty(gab, GAB_VAL_TO_SHAPE(shape)));
}

gab_value gab_tuple(gab_eg *gab, gab_vm *vm, uint64_t size,
                    gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_tuple(gab, vm, size);
  return __gab_obj(gab_obj_record_create(gab, vm, bundle_shape, 1, values));
}

void *gab_boxdata(gab_value value) {
  assert(gab_valknd(value) == kGAB_BOX);
  return GAB_VAL_TO_BOX(value)->data;
}

gab_value gab_recshp(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return __gab_obj(GAB_VAL_TO_RECORD(value)->shape);
}

size_t gab_reclen(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->len;
}

gab_value *gab_recdata(gab_value value) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return GAB_VAL_TO_RECORD(value)->data;
}

size_t gab_shplen(gab_value value) {
  assert(gab_valknd(value) == kGAB_SHAPE);
  return GAB_VAL_TO_SHAPE(value)->len;
}

gab_value *gab_shpdata(gab_value value) {
  assert(gab_valknd(value) == kGAB_SHAPE);
  return GAB_VAL_TO_SHAPE(value)->data;
}

size_t gab_strlen(gab_value value) {
  assert(gab_valknd(value) == kGAB_STRING);
  return GAB_VAL_TO_STRING(value)->len;
}

bool gab_rechas(gab_value value, gab_value key) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return gab_obj_record_has(GAB_VAL_TO_RECORD(value), key);
}

bool gab_srechas(gab_eg *gab, gab_value value, const char *key) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return gab_obj_record_has(GAB_VAL_TO_RECORD(value), gab_string(gab, key));
}

gab_value gab_recat(gab_value value, gab_value key) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return gab_obj_record_at(GAB_VAL_TO_RECORD(value), key);
}

gab_value gab_srecat(gab_eg *gab, gab_value value, const char *key) {
  assert(gab_valknd(value) == kGAB_RECORD);
  return gab_obj_record_at(GAB_VAL_TO_RECORD(value), gab_string(gab, key));
}

void gab_recput(gab_vm *vm, gab_value value, gab_value key, gab_value val) {
  assert(gab_valknd(value) == kGAB_RECORD);
  gab_obj_record_put(vm, GAB_VAL_TO_RECORD(value), key, val);
}

void gab_srecput(gab_eg *gab, gab_vm *vm, gab_value value, const char *key,
                 gab_value val) {
  assert(gab_valknd(value) == kGAB_RECORD);
  gab_obj_record_put(vm, GAB_VAL_TO_RECORD(value), gab_string(gab, key), val);
}

size_t gab_shpfind(gab_value value, gab_value needle) {
  assert(gab_valknd(value) == kGAB_SHAPE);
  return gab_obj_shape_find(GAB_VAL_TO_SHAPE(value), needle);
}

gab_value gab_msgfind(gab_value msg, gab_value spec) {
  assert(gab_valknd(msg) == kGAB_MESSAGE);
  return gab_obj_message_find(GAB_VAL_TO_MESSAGE(msg), spec);
}
