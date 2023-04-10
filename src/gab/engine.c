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
        .name = GAB_MESSAGE_BOR,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_BOR),
    },
    {
        .name = GAB_MESSAGE_BND,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_BND),
    },
    {
        .name = GAB_MESSAGE_LSH,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LSH),
    },
    {
        .name = GAB_MESSAGE_RSH,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_RSH),
    },
    {
        .name = GAB_MESSAGE_ADD,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_ADD),
    },
    {
        .name = GAB_MESSAGE_SUB,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_SUB),
    },
    {
        .name = GAB_MESSAGE_MUL,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MUL),
    },
    {
        .name = GAB_MESSAGE_DIV,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_DIV),
    },
    {
        .name = GAB_MESSAGE_MOD,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MOD),
    },
    {
        .name = GAB_MESSAGE_LT,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LT),
    },
    {
        .name = GAB_MESSAGE_LTE,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LTE),
    },
    {
        .name = GAB_MESSAGE_GT,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GT),
    },
    {
        .name = GAB_MESSAGE_GTE,
        .type = GAB_KIND_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GTE),
    },
    {
        .name = GAB_MESSAGE_ADD,
        .type = GAB_KIND_STRING,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = GAB_MESSAGE_EQ,
        .type = GAB_KIND_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = GAB_MESSAGE_SET,
        .type = GAB_KIND_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_STORE_ANA),
    },
    {
        .name = GAB_MESSAGE_GET,
        .type = GAB_KIND_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LOAD_ANA),
    },
    {
        .name = GAB_MESSAGE_CAL,
        .type = GAB_KIND_BUILTIN,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CALL_BUILTIN),
    },
    {
        .name = GAB_MESSAGE_CAL,
        .type = GAB_KIND_BLOCK,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CALL_BLOCK),
    },
    {
        .name = GAB_MESSAGE_CAL,
        .type = GAB_KIND_SUSPENSE,
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

  d_strings_create(&gab->interned_strings, INTERN_INITIAL_CAP);
  d_shapes_create(&gab->interned_shapes, INTERN_INITIAL_CAP);
  d_messages_create(&gab->interned_messages, INTERN_INITIAL_CAP);
  d_gab_import_create(&gab->imports, 8);
  v_gab_value_create(&gab->scratch, 8);

  memset(&gab->allocator, 0, sizeof(gab->allocator));

  gab->types[GAB_KIND_UNDEFINED] = GAB_VAL_UNDEFINED();
  gab->types[GAB_KIND_NIL] = GAB_VAL_NIL();
  gab->types[GAB_KIND_NUMBER] = GAB_SYMBOL("Number");
  gab->types[GAB_KIND_BOOLEAN] = GAB_SYMBOL("Boolean");
  gab->types[GAB_KIND_STRING] = GAB_SYMBOL("String");
  gab->types[GAB_KIND_MESSAGE] = GAB_SYMBOL("Message");
  gab->types[GAB_KIND_PROTOTYPE] = GAB_SYMBOL("Prototype");
  gab->types[GAB_KIND_BUILTIN] = GAB_SYMBOL("Builtin");
  gab->types[GAB_KIND_BLOCK] = GAB_SYMBOL("Block");
  gab->types[GAB_KIND_RECORD] = GAB_SYMBOL("Record");
  gab->types[GAB_KIND_SHAPE] = GAB_SYMBOL("Shape");
  gab->types[GAB_KIND_SYMBOL] = GAB_SYMBOL("Symbol");
  gab->types[GAB_KIND_CONTAINER] = GAB_SYMBOL("Container");
  gab->types[GAB_KIND_SUSPENSE] = GAB_SYMBOL("Suspsense");
  gab->types[GAB_KIND_LIST] = GAB_SYMBOL("List");
  gab->types[GAB_KIND_PRIMITIVE] = GAB_SYMBOL("Primitive");
  gab->types[GAB_KIND_MAP] = GAB_SYMBOL("Map");

  for (int i = 0; i < LEN_CARRAY(primitives); i++) {
    gab_value name =
        GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(primitives[i].name)));

    gab_specialize(gab, NULL, name, gab->types[primitives[i].type],
                   primitives[i].primitive);
  }

  return gab;
}

void gab_destroy(gab_engine *gab) {
  if (gab == NULL)
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

  for (u8 i = 0; i < GAB_KIND_NKINDS; i++) {
    gab_gc_dref(gc, NULL, gab->types[i]);
  }

  while (gab->modules) {
    gab_module *m = gab->modules;
    gab->modules = m->next;
    gab_module_destroy(gab, gc, m);
  }

  while (gab->sources) {
    gab_source *s = gab->sources;
    gab->sources = s->next;
    gab_source_destroy(s);
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
  gab_gc_run(&vm->gc, vm);
  gab_engine_collect(gab);
}

void gab_args(gab_engine *gab, u8 argc, gab_value argv_names[argc],
              gab_value argv_values[argc]) {
  v_gab_value_destroy(&gab->argv_values);
  v_gab_value_destroy(&gab->argv_names);

  v_gab_value_create(&gab->argv_values, argc * 2);
  v_gab_value_create(&gab->argv_names, argc * 2);

  for (u8 i = 0; i < argc; i++) {
    v_gab_value_push(&gab->argv_names, argv_names[i]);
    v_gab_value_push(&gab->argv_values, argv_values[i]);
  }
}

void gab_arg_push(gab_engine* gab, gab_value name, gab_value value) {
    v_gab_value_push(&gab->argv_names, name);
    v_gab_value_push(&gab->argv_values, value);
}

void gab_arg_pop(gab_engine* gab) {
    v_gab_value_pop(&gab->argv_names);
    v_gab_value_pop(&gab->argv_values);
};

gab_value gab_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags) {
  return gab_bc_compile(gab, name, source, flags, gab->argv_values.len,
                        gab->argv_names.data);
}

gab_value gab_run(gab_engine *gab, gab_value main, u8 flags) {
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
    gab_val_iref(vm, receiver);
    gab_val_iref(vm, specialization);
  }

  return GAB_VAL_OBJ(m);
}

gab_value send_msg(gab_engine *gab, gab_vm *vm, gab_value msg,
                   gab_value receiver, u8 argc, gab_value argv[argc]) {
  if (GAB_VAL_IS_UNDEFINED(msg))
    return msg;

  gab_value mod =
      gab_bc_compile_send(gab, msg, receiver, GAB_FLAG_DUMP_ERROR, argc, argv);

  if (GAB_VAL_IS_UNDEFINED(mod))
    return mod;

  gab_scratch(gab, mod);

  gab_value result = gab_vm_run(gab, mod, GAB_FLAG_DUMP_ERROR, 0, NULL);

  return result;
}

gab_value gab_send(gab_engine *gab, gab_vm *vm, gab_value msg,
                   gab_value receiver, u8 argc, gab_value argv[argc]) {
  if (GAB_VAL_IS_STRING(msg)) {
    gab_obj_message *main = gab_obj_message_create(gab, msg);
    return send_msg(gab, vm, GAB_VAL_OBJ(main), receiver, argc, argv);
  } else if (GAB_VAL_IS_MESSAGE(msg)) {
    return send_msg(gab, vm, msg, receiver, argc, argv);
  }

  return GAB_VAL_NIL();
}

/**
 * Gab internal stuff
 */

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

gab_value gab_type(gab_engine *gab, gab_kind t) { return gab->types[t]; }

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
