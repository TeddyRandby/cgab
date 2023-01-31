#include "include/engine.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/module.h"
#include "include/object.h"
#include "include/types.h"
#include "include/value.h"
#include "include/vm.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

struct primitive {
  const char *name;
  gab_type type;
  gab_value primitive;
};

struct primitive primitives[] = {
    {
        .name = "__add__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_ADD),
    },
    {
        .name = "__sub__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_SUB),
    },
    {
        .name = "__mul__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MUL),
    },
    {
        .name = "__div__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_DIV),
    },
    {
        .name = "__mod__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_MOD),
    },
    {
        .name = "__lt__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LT),
    },
    {
        .name = "__lte__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LTE),
    },
    {
        .name = "__gt__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GT),
    },
    {
        .name = "__gte__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_GTE),
    },
    {
        .name = "__neg__",
        .type = TYPE_NUMBER,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_NEG),
    },
    {
        .name = "__eq__",
        .type = TYPE_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_EQ),
    },
    {
        .name = "__add__",
        .type = TYPE_STRING,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_CONCAT),
    },
    {
        .name = "__set__",
        .type = TYPE_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_STORE_ANA),
    },
    {
        .name = "__get__",
        .type = TYPE_UNDEFINED,
        .primitive = GAB_VAL_PRIMITIVE(OP_SEND_PRIMITIVE_LOAD_ANA),
    },
};

/**
 * Gab API stuff
 */
gab_engine *gab_create() {
  gab_engine *gab = NEW(gab_engine);

  d_strings_create(&gab->interned_strings, INTERN_INITIAL_CAP);
  d_shapes_create(&gab->interned_shapes, INTERN_INITIAL_CAP);
  d_messages_create(&gab->interned_messages, INTERN_INITIAL_CAP);

  gab_gc_create(&gab->gc);

  gab->types[TYPE_UNDEFINED] = GAB_VAL_UNDEFINED();
  gab->types[TYPE_NIL] = GAB_VAL_NIL();
  gab->types[TYPE_NUMBER] = GAB_SYMBOL("number");
  gab->types[TYPE_BOOLEAN] = GAB_SYMBOL("boolean");
  gab->types[TYPE_STRING] = GAB_SYMBOL("string");
  gab->types[TYPE_MESSAGE] = GAB_SYMBOL("message");
  gab->types[TYPE_PROTOTYPE] = GAB_SYMBOL("prototype");
  gab->types[TYPE_BUILTIN] = GAB_SYMBOL("builtin");
  gab->types[TYPE_CLOSURE] = GAB_SYMBOL("closure");
  gab->types[TYPE_UPVALUE] = GAB_SYMBOL("upvalue");
  gab->types[TYPE_RECORD] = GAB_SYMBOL("record");
  gab->types[TYPE_SHAPE] = GAB_SYMBOL("shape");
  gab->types[TYPE_SYMBOL] = GAB_SYMBOL("symbol");
  gab->types[TYPE_CONTAINER] = GAB_SYMBOL("container");
  gab->types[TYPE_EFFECT] = GAB_SYMBOL("effect");

  for (int i = 0; i < LEN_CARRAY(primitives); i++) {
    gab_specialize(gab, s_i8_cstr(primitives[i].name),
                   gab->types[primitives[i].type], primitives[i].primitive);
  }

  return gab;
}

void gab_destroy(gab_engine *gab) {
  if (gab == NULL)
    return;

  for (u64 i = 0; i < gab->interned_strings.cap; i++) {
    if (d_strings_iexists(&gab->interned_strings, i)) {
      gab_obj_string *v = d_strings_ikey(&gab->interned_strings, i);
      gab_gc_dref(gab, NULL, &gab->gc, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_shapes.cap; i++) {
    if (d_shapes_iexists(&gab->interned_shapes, i)) {
      gab_obj_shape *v = d_shapes_ikey(&gab->interned_shapes, i);
      gab_gc_dref(gab, NULL, &gab->gc, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_messages.cap; i++) {
    if (d_messages_iexists(&gab->interned_messages, i)) {
      gab_obj_message *v = d_messages_ikey(&gab->interned_messages, i);
      gab_gc_dref(gab, NULL, &gab->gc, GAB_VAL_OBJ(v));
    }
  }

  for (u8 i = 0; i < GAB_NTYPES; i++) {
    gab_gc_dref(gab, NULL, &gab->gc, gab->types[i]);
  }

  for (u8 i = 0; i < gab->argc; i++) {
    gab_gc_dref(gab, NULL, &gab->gc, gab->argv_values[i]);
  }

  gab_gc_collect(gab, NULL, &gab->gc);

  gab_gc_destroy(&gab->gc);

  d_strings_destroy(&gab->interned_strings);
  d_shapes_destroy(&gab->interned_shapes);
  d_messages_destroy(&gab->interned_messages);

  DESTROY(gab);
}

void gab_collect(gab_engine *gab, gab_vm *vm) {
  gab_gc_collect(gab, vm, &gab->gc);
}

void gab_args(gab_engine *gab, u8 argc, s_i8 argv_names[argc],
              gab_value argv_values[argc]) {
  gab->argc = argc;
  gab->argv_names = argv_names;
  gab->argv_values = argv_values;
}

gab_module *gab_compile(gab_engine *gab, s_i8 name, s_i8 source, u8 flags) {
  return gab_bc_compile(gab, name, source, flags, gab->argc, gab->argv_names);
}

gab_value gab_run(gab_engine *gab, gab_module *main, u8 flags) {
  return gab_vm_run(gab, main, flags, gab->argc, gab->argv_values);
};

void gab_panic(gab_engine *gab, gab_vm *vm, const char *msg) {
  gab_vm_panic(gab, vm, msg);
}

void gab_dref(gab_engine *gab, gab_vm *vm, gab_value value) {
  gab_gc_dref(gab, vm, &gab->gc, value);
};

void gab_iref(gab_engine *gab, gab_vm *vm, gab_value value) {
  gab_gc_iref(gab, vm, &gab->gc, value);
};

gab_value gab_bundle_record(gab_engine *gab, gab_vm *vm, u64 size,
                            s_i8 keys[size], gab_value values[size]) {
  gab_value value_keys[size];

  for (u64 i = 0; i < size; i++) {
    value_keys[i] = GAB_VAL_OBJ(gab_obj_string_create(gab, keys[i]));
  }

  gab_obj_shape *bundle_shape =
      gab_obj_shape_create(gab, vm, size, 1, value_keys);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, size, 1, values));

  return bundle;
}

gab_value gab_bundle_array(gab_engine *gab, gab_vm *vm, u64 size,
                           gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_array(gab, vm, size);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, size, 1, values));

  return bundle;
}

gab_value gab_specialize(gab_engine *gab, s_i8 name, gab_value receiver,
                         gab_value specialization) {

  gab_obj_message *f = gab_obj_message_create(gab, name);

  if (gab_obj_message_find(f, receiver) != UINT16_MAX)
    return GAB_VAL_NIL();

  gab_iref(gab, NULL, receiver);
  gab_iref(gab, NULL, specialization);
  gab_obj_message_insert(f, receiver, specialization);

  return GAB_VAL_OBJ(f);
}

gab_value gab_send(gab_engine *gab, s_i8 name, gab_value receiver, u8 argc,
                   gab_value argv[argc]) {

  // Cleanup the module
  gab_module *mod = gab_bc_compile_send(gab, name, receiver, argc, argv);

  gab_value result = gab_vm_run(gab, mod, GAB_FLAG_PANIC_ON_FAIL, 0, NULL);

  gab_module_collect(gab, mod);

  gab_module_destroy(gab, mod);

  return result;
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

gab_obj_message *gab_engine_find_message(gab_engine *self, s_i8 name,
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
      if (key->hash == hash && s_i8_match(name, key->name)) {
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

  if (self->len != len) {
    return false;
  }

  for (u64 i = 0; i < len; i++) {
    gab_value key = values[i * stride];
    if (self->keys[i] != key)
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
    gab_obj_shape *key = d_shapes_ikey(&self->interned_shapes, index);
    d_status status = d_shapes_istatus(&self->interned_shapes, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && shape_matches_keys(key, keys, size, stride)) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_shapes.cap - 1);
  }
}

gab_value gab_get_type(gab_engine *gab, gab_type t) { return gab->types[t]; }

static inline gab_value pack_simple(gab_engine *gab, gab_vm *vm,
                                    const char *str, gab_value val) {
  s_i8 key[] = {s_i8_cstr(str)};
  return gab_bundle_record(gab, vm, 1, key, &val);
}

gab_value gab_result_ok(gab_engine *gab, gab_vm *vm, gab_value val) {
  return pack_simple(gab, vm, "ok", val);
}

gab_value gab_result_err(gab_engine *gab, gab_vm *vm, gab_value err) {
  return pack_simple(gab, vm, "err", err);
}

gab_value gab_option_some(gab_engine *gab, gab_vm *vm, gab_value some) {
  return pack_simple(gab, vm, "some", some);
}

gab_value gab_option_none(gab_engine *gab, gab_vm *vm) {
  return pack_simple(gab, vm, "none", GAB_VAL_BOOLEAN(true));
}
