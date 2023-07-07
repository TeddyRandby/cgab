#include "include/engine.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/import.h"
#include "include/lexer.h"
#include "include/module.h"
#include "include/object.h"
#include "include/types.h"
#include "include/value.h"
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
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_BOR),
    },
    {
        .name = mGAB_BND,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_BND),
    },
    {
        .name = mGAB_LSH,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LSH),
    },
    {
        .name = mGAB_RSH,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_RSH),
    },
    {
        .name = mGAB_ADD,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_ADD),
    },
    {
        .name = mGAB_SUB,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_SUB),
    },
    {
        .name = mGAB_MUL,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MUL),
    },
    {
        .name = mGAB_DIV,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_DIV),
    },
    {
        .name = mGAB_MOD,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MOD),
    },
    {
        .name = mGAB_LT,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LT),
    },
    {
        .name = mGAB_LTE,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LTE),
    },
    {
        .name = mGAB_GT,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GT),
    },
    {
        .name = mGAB_GTE,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GTE),
    },
    {
        .name = mGAB_ADD,
        .type = kGAB_STRING,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_STRING,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_BOOLEAN,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_SHAPE,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_MESSAGE,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_EQ,
        .type = kGAB_NIL,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = mGAB_SET,
        .type = kGAB_RECORD,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_STORE),
    },
    {
        .name = mGAB_GET,
        .type = kGAB_RECORD,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LOAD),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_BUILTIN,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CALL_BUILTIN),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_BLOCK,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CALL_BLOCK),
    },
    {
        .name = mGAB_CALL,
        .type = kGAB_SUSPENSE,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CALL_SUSPENSE),
    },
};

gab_engine *gab_create() {
  gab_engine *gab = NEW(gab_engine);

  gab->hash_seed = time(NULL);
  gab->objects = NULL;
  gab->modules = NULL;
  gab->sources = NULL;
  v_gab_value_create(&gab->argv_values, 8);
  v_gab_value_create(&gab->argv_names, 8);

  d_strings_create(&gab->interned_strings, cGAB_INTERN_INITIAL_CAP);
  d_shapes_create(&gab->interned_shapes, cGAB_INTERN_INITIAL_CAP);
  d_messages_create(&gab->interned_messages, cGAB_INTERN_INITIAL_CAP);
  d_gab_import_create(&gab->imports, 8);
  v_gab_value_create(&gab->scratch, 8);

  memset(&gab->allocator, 0, sizeof(gab->allocator));

  gab->types[kGAB_UNDEFINED] = GAB_VAL_UNDEFINED();
  gab->types[kGAB_NIL] = GAB_VAL_NIL();
  gab->types[kGAB_NUMBER] = GAB_STRING("Number");
  gab->types[kGAB_BOOLEAN] = GAB_STRING("Boolean");
  gab->types[kGAB_STRING] = GAB_STRING("String");
  gab->types[kGAB_MESSAGE] = GAB_STRING("Message");
  gab->types[kGAB_BLOCK_PROTO] = GAB_STRING("Prototype");
  gab->types[kGAB_BUILTIN] = GAB_STRING("Builtin");
  gab->types[kGAB_BLOCK] = GAB_STRING("Block");
  gab->types[kGAB_RECORD] = GAB_STRING("Record");
  gab->types[kGAB_SHAPE] = GAB_STRING("Shape");
  gab->types[kGAB_CONTAINER] = GAB_STRING("Container");
  gab->types[kGAB_SUSPENSE] = GAB_STRING("Suspsense");
  gab->types[kGAB_PRIMITIVE] = GAB_STRING("Primitive");

  for (int i = 0; i < LEN_CARRAY(primitives); i++) {
    gab_value name =
        GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(primitives[i].name)));

    gab_specialize(gab, NULL, name, gab->types[primitives[i].type],
                   primitives[i].primitive);
  }

  return gab;
}

void gab_destroy(gab_engine *gab) {
  if (!gab)
    return;

  gab_gc *gc = NEW(gab_gc);
  gab_gc_create(gc);

  for (u64 i = 0; i < gab->scratch.len; i++) {
    gab_gc_dref(gc, NULL, v_gab_value_val_at(&gab->scratch, i));
  }

  for (u64 i = 0; i < gab->interned_strings.cap; i++) {
    if (d_strings_iexists(&gab->interned_strings, i)) {
      gab_obj_string *v = d_strings_ikey(&gab->interned_strings, i);
      gab_gc_dref(gc, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_shapes.cap; i++) {
    if (d_shapes_iexists(&gab->interned_shapes, i)) {
      gab_obj_shape *v = d_shapes_ikey(&gab->interned_shapes, i);
      gab_gc_dref(gc, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_messages.cap; i++) {
    if (d_messages_iexists(&gab->interned_messages, i)) {
      gab_obj_message *v = d_messages_ikey(&gab->interned_messages, i);
      gab_gc_dref(gc, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u8 i = 0; i < kGAB_NKINDS; i++) {
    gab_gc_dref(gc, NULL, gab->types[i]);
  }

  while (gab->sources) {
    gab_source *s = gab->sources;
    gab->sources = s->next;
    gab_source_destroy(s);
  }

  while (gab->modules) {
    gab_module *m = gab->modules;
    gab->modules = m->next;
    gab_module_destroy(gab, gc, m);
  }

  gab_gc_run(gc, NULL);

  gab_engine_collect(gab);

  gab_imports_destroy(gab, gc);

  d_strings_destroy(&gab->interned_strings);
  d_shapes_destroy(&gab->interned_shapes);
  d_messages_destroy(&gab->interned_messages);
  d_gab_import_destroy(&gab->imports);

  v_gab_value_destroy(&gab->argv_values);
  v_gab_value_destroy(&gab->argv_names);

  v_gab_value_destroy(&gab->scratch);

  gab_gc_destroy(gc);
  DESTROY(gc);
  DESTROY(gab);
}

void gab_collect(gab_engine *gab, gab_vm *vm) {
  if (vm != NULL)
    gab_gc_run(&vm->gc, vm);

  gab_engine_collect(gab);
}

void gab_args(gab_engine *gab, u8 argc, gab_value argv_names[argc],
              gab_value argv_values[argc]) {
  gab->argv_values.len = 0;
  gab->argv_names.len = 0;

  for (u8 i = 0; i < argc; i++) {
    v_gab_value_push(&gab->argv_names, argv_names[i]);
    v_gab_value_push(&gab->argv_values, argv_values[i]);
  }
}

u64 gab_args_push(gab_engine *gab, gab_value name) {
  v_gab_value_push(&gab->argv_values, GAB_VAL_UNDEFINED());
  return v_gab_value_push(&gab->argv_names, name);
}

void gab_args_put(gab_engine *gab, gab_value value, u64 index) {
  gab->argv_values.data[index] = value;
}

void gab_args_pop(gab_engine *gab) {
  v_gab_value_pop(&gab->argv_names);
  v_gab_value_pop(&gab->argv_values);
};

gab_value gab_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags) {
  return gab_bc_compile(gab, name, source, flags, gab->argv_values.len,
                        gab->argv_names.data);
}

a_gab_value *gab_run(gab_engine *gab, gab_value main, u8 flags) {
  return gab_vm_run(gab, main, flags, gab->argv_values.len,
                    gab->argv_values.data);
};

gab_value gab_panic(gab_engine *gab, gab_vm *vm, const char *msg) {
  gab_value vm_container = gab_vm_panic(gab, vm, msg);

  gab_val_dref(vm, vm_container);

  return vm_container;
}

void gab_val_dref(gab_vm *vm, gab_value value) {
  gab_gc_dref(&vm->gc, vm, value);
}

void gab_val_dref_many(gab_vm *vm, u64 len, gab_value values[len]) {
  gab_gc_dref_many(&vm->gc, vm, len, values);
}

void gab_val_iref(gab_vm *vm, gab_value value) {
  gab_gc_iref(&vm->gc, vm, value);
}

void gab_val_iref_many(gab_vm *vm, u64 len, gab_value values[len]) {
  gab_gc_iref_many(&vm->gc, vm, len, values);
}

gab_value gab_record(gab_engine *gab, gab_vm *vm, u64 size, s_i8 keys[size],
                     gab_value values[size]) {
  gab_value value_keys[size];

  for (u64 i = 0; i < size; i++) {
    value_keys[i] = GAB_VAL_OBJ(gab_obj_string_create(gab, keys[i]));
  }

  gab_obj_shape *bundle_shape =
      gab_obj_shape_create(gab, vm, size, 1, value_keys);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(gab, vm, bundle_shape, 1, values));

  return bundle;
}

gab_value gab_tuple(gab_engine *gab, gab_vm *vm, u64 size,
                    gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_tuple(gab, vm, size);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(gab, vm, bundle_shape, 1, values));

  return bundle;
}

gab_value gab_specialize(gab_engine *gab, gab_vm *vm, gab_value name,
                         gab_value receiver, gab_value specialization) {
  gab_obj_message *m = gab_obj_message_create(gab, name);

  if (gab_obj_message_find(m, receiver) != UINT64_MAX)
    return GAB_VAL_NIL();

  gab_obj_message_insert(m, receiver, specialization);

  if (vm) {
    gab_gc_iref(&vm->gc, vm, receiver);
    gab_gc_iref(&vm->gc, vm, specialization);
  }

  return GAB_VAL_OBJ(m);
}

a_gab_value *send_msg(gab_engine *gab, gab_vm *vm, gab_value msg,
                      gab_value receiver, u8 argc, gab_value argv[argc]) {
  if (GAB_VAL_IS_UNDEFINED(msg))
    return a_gab_value_one(GAB_VAL_UNDEFINED());

  gab_value mod =
      gab_bc_compile_send(gab, msg, receiver, fGAB_DUMP_ERROR, argc, argv);

  if (GAB_VAL_IS_UNDEFINED(mod))
    return a_gab_value_one(mod);

  gab_scratch(gab, mod);

  a_gab_value *result = gab_vm_run(gab, mod, fGAB_DUMP_ERROR, 0, NULL);

  return result;
}

a_gab_value *gab_send(gab_engine *gab, gab_vm *vm, gab_value msg,
                      gab_value receiver, u8 argc, gab_value argv[argc]) {
  if (GAB_VAL_IS_STRING(msg)) {
    gab_obj_message *main = gab_obj_message_create(gab, msg);
    return send_msg(gab, vm, GAB_VAL_OBJ(main), receiver, argc, argv);
  } else if (GAB_VAL_IS_MESSAGE(msg)) {
    return send_msg(gab, vm, msg, receiver, argc, argv);
  }

  return a_gab_value_one(GAB_VAL_NIL());
}

i32 gab_engine_intern(gab_engine *self, gab_value value) {
  if (GAB_VAL_IS_STRING(value)) {
    gab_obj_string *k = GAB_VAL_TO_STRING(value);

    d_strings_insert(&self->interned_strings, k, 0);

    return d_strings_index_of(&self->interned_strings, k);
  }

  if (GAB_VAL_IS_SHAPE(value)) {
    gab_obj_shape *k = GAB_VAL_TO_SHAPE(value);

    d_shapes_insert(&self->interned_shapes, k, 0);

    return d_shapes_index_of(&self->interned_shapes, k);
  }

  if (GAB_VAL_IS_MESSAGE(value)) {
    gab_obj_message *k = GAB_VAL_TO_MESSAGE(value);

    d_messages_insert(&self->interned_messages, k, 0);

    return d_messages_index_of(&self->interned_messages, k);
  }

  return -1;
}

gab_obj_message *gab_engine_find_message(gab_engine *self, gab_value name,
                                         u64 hash) {
  if (self->interned_messages.len == 0)
    return NULL;

  u64 index = hash & (self->interned_messages.cap - 1);

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

gab_obj_string *gab_engine_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->interned_strings.len == 0)
    return NULL;

  u64 index = hash & (self->interned_strings.cap - 1);

  for (;;) {
    gab_obj_string *key = d_strings_ikey(&self->interned_strings, index);
    d_status status = d_strings_istatus(&self->interned_strings, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && s_i8_match(str, gab_obj_string_ref(key))) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_strings.cap - 1);
  }
}

static inline boolean shape_matches_keys(gab_obj_shape *self,
                                         gab_value values[], u64 len,
                                         u64 stride) {

  if (self->len != len)
    return false;

  for (u64 i = 0; i < len; i++) {
    gab_value key = values[i * stride];
    if (self->data[i] != key)
      return false;
  }

  return true;
}

gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]) {
  if (self->interned_shapes.len == 0)
    return NULL;

  u64 index = hash & (self->interned_shapes.cap - 1);

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

gab_value gab_type(gab_engine *gab, gab_kind k) { return gab->types[k]; }

int gab_val_printf_handler(FILE *stream, const struct printf_info *info,
                           const void *const *args) {
  const gab_value value = *(const gab_value *const)args[0];
  return gab_val_dump(stream, value);
}
int gab_val_printf_arginfo(const struct printf_info *i, size_t n, int *argtypes,
                           int *sizes) {
  if (n > 0) {
    argtypes[0] = PA_INT | PA_FLAG_LONG;
    sizes[0] = sizeof(gab_value);
  }

  return 1;
}

u64 gab_seed(gab_engine *gab) { return gab->hash_seed; }

void gab_engine_collect(gab_engine *gab) {
  // Maintain the global list of objects
  gab_obj *obj = gab->objects;
  gab_obj *prev = NULL;

  while (obj) {
    if (GAB_OBJ_IS_GARBAGE(obj)) {

      gab_obj *old_obj = obj;
      obj = obj->next;

      gab_obj_destroy(old_obj);
      gab_obj_alloc(gab, old_obj, 0);

      if (prev)
        prev->next = obj;

      if (old_obj == gab->objects)
        gab->objects = obj;

    } else {
      prev = obj;
      obj = obj->next;
    }
  }
}

gab_value gab_scratch(gab_engine *gab, gab_value value) {
  v_gab_value_push(&gab->scratch, value);
  return value;
}

i32 gab_push(gab_vm *vm, u64 argc, gab_value argv[argc]) {
  return gab_vm_push(vm, argc, argv);
}

gab_value gab_val_copy(gab_engine *gab, gab_vm *vm, gab_value value) {
  switch (gab_val_kind(value)) {
  case kGAB_CONTAINER:
  case kGAB_BOOLEAN:
  case kGAB_NUMBER:
  case kGAB_NIL:
  case kGAB_UNDEFINED:
  case kGAB_PRIMITIVE:
  case kGAB_NKINDS:
    return value;

  case kGAB_MESSAGE: {
    gab_obj_message *self = GAB_VAL_TO_MESSAGE(value);
    gab_obj_message *copy =
        gab_obj_message_create(gab, gab_val_copy(gab, vm, self->name));

    return GAB_VAL_OBJ(copy);
  }

  case kGAB_STRING: {
    gab_obj_string *self = GAB_VAL_TO_STRING(value);
    return GAB_VAL_OBJ(gab_obj_string_create(gab, gab_obj_string_ref(self)));
  }

  case kGAB_BUILTIN: {
    gab_obj_builtin *self = GAB_VAL_TO_BUILTIN(value);
    return GAB_VAL_OBJ(gab_obj_builtin_create(
        gab, self->function, gab_val_copy(gab, vm, self->name)));
  }

  case kGAB_BLOCK_PROTO: {
    gab_obj_block_proto *self = GAB_VAL_TO_BLOCK_PROTO(value);
    gab->modules = gab_module_copy(gab, self->mod);

    gab_obj_block_proto *copy = gab_obj_prototype_create(
        gab, gab->modules, self->narguments, self->nslots, self->nlocals,
        self->nupvalues, self->var, self->upv_desc, self->upv_desc);

    memcpy(copy->upv_desc, self->upv_desc, self->nupvalues * 2);

    return GAB_VAL_OBJ(copy);
  }

  case kGAB_BLOCK: {
    gab_obj_block *self = GAB_VAL_TO_BLOCK(value);

    gab_obj_block_proto *p_copy =
        GAB_VAL_TO_BLOCK_PROTO(gab_val_copy(gab, vm, GAB_VAL_OBJ(self->p)));

    gab_obj_block *copy = gab_obj_block_create(gab, p_copy);

    for (u8 i = 0; i < p_copy->nupvalues; i++) {
      copy->upvalues[i] = gab_val_copy(gab, vm, self->upvalues[i]);
    }

    return GAB_VAL_OBJ(copy);
  }

  case kGAB_SHAPE: {
    gab_obj_shape *self = GAB_VAL_TO_SHAPE(value);

    gab_value keys[self->len];

    for (u64 i = 0; i < self->len; i++) {
      keys[i] = gab_val_copy(gab, vm, self->data[i]);
    }

    gab_obj_shape *copy = gab_obj_shape_create(gab, vm, self->len, 1, keys);

    return GAB_VAL_OBJ(copy);
  }

  case kGAB_RECORD: {
    gab_obj_record *self = GAB_VAL_TO_RECORD(value);

    gab_obj_shape *s_copy =
        GAB_VAL_TO_SHAPE(gab_val_copy(gab, vm, GAB_VAL_OBJ(self->shape)));

    gab_value values[self->len];

    for (u64 i = 0; i < self->len; i++)
      values[i] = gab_val_copy(gab, vm, self->data[i]);

    gab_obj_record *copy = gab_obj_record_create(gab, vm, s_copy, 1, values);

    return GAB_VAL_OBJ(copy);
  }

  case kGAB_SUSPENSE: {
    gab_obj_suspense *self = GAB_VAL_TO_SUSPENSE(value);

    gab_value frame[self->len];

    for (u64 i = 0; i < self->len; i++)
      frame[i] = gab_val_copy(gab, vm, self->frame[i]);

    gab_obj_block *b_copy =
        GAB_VAL_TO_BLOCK(gab_val_copy(gab, vm, GAB_VAL_OBJ(self->c)));

    gab_obj_suspense *copy =
        gab_obj_suspense_create(gab, vm, b_copy, self->offset, self->have,
                                self->want, self->len, frame);

    return GAB_VAL_OBJ(copy);
  }
  }
}
