#include "include/object.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/module.h"
#include "include/types.h"
#include "include/value.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GAB_CREATE_ARRAY(type, count)                                          \
  ((type *)gab_reallocate(NULL, 0, sizeof(type) * count))
#define GAB_CREATE_STRUCT(obj_type)                                            \
  ((obj_type *)gab_reallocate(NULL, 0, sizeof(obj_type)))
#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                \
  ((obj_type *)gab_reallocate(                                                 \
      NULL, 0, sizeof(obj_type) + sizeof(flex_type) * flex_count))

#define GAB_DESTROY_STRUCT(ptr) gab_reallocate(ptr, 0, 0)
#define GAB_DESTROY_ARRAY(type, loc, count)                                    \
  (gab_reallocate(loc, sizeof(type) * count, 0))

/*
  Helper macros for allocating gab objects.
*/
#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((obj_type *)gab_obj_create((gab_obj *)GAB_CREATE_STRUCT(obj_type), kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((obj_type *)gab_obj_create(                                                 \
      (gab_obj *)GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count),      \
      kind))

/*
  Initialize the header of any gab objects, with kind (k)
*/
gab_obj *gab_obj_create(gab_obj *self, gab_type k) {
  self->kind = k;
  // Objects start out with one reference.
  self->references = 1;

  self->flags = 0;
#if GAB_LOG_GC

  printf("Created (%p) \n", self);

#endif
  return self;
}

void gab_val_dump(gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    printf("%s", GAB_VAL_TO_BOOLEAN(self) ? "true" : "false");
  } else if (GAB_VAL_IS_NUMBER(self)) {
    printf("%g", GAB_VAL_TO_NUMBER(self));
  } else if (GAB_VAL_IS_NIL(self)) {
    printf("nil");
  } else if (GAB_VAL_IS_OBJ(self)) {
    gab_obj_dump(self);
  } else if (GAB_VAL_IS_PRIMITIVE(self)) {
    printf("[primitive:%s]", gab_opcode_names[GAB_VAL_TO_PRIMITIVE(self)]);
  }
}

/*
  Helpers for converting a gab value to a string object.
*/
gab_obj_string *gab_obj_to_obj_string(gab_engine *gab, gab_obj *self) {
  switch (self->kind) {
  case TYPE_STRING: {
    return (gab_obj_string *)self;
  }
  case TYPE_CLOSURE: {
    return gab_obj_string_create(gab, s_i8_cstr("[closure]"));
  }
  case TYPE_RECORD: {
    return gab_obj_string_create(gab, s_i8_cstr("[record]"));
  }
  case TYPE_SHAPE: {
    return gab_obj_string_create(gab, s_i8_cstr("[shape]"));
  }
  case TYPE_UPVALUE: {
    return gab_obj_string_create(gab, s_i8_cstr("[upvalue]"));
  }
  case TYPE_MESSAGE: {
    return gab_obj_string_create(gab, s_i8_cstr("[message]"));
  }
  case TYPE_PROTOTYPE: {
    return gab_obj_string_create(gab, s_i8_cstr("[prototype]"));
  }
  case TYPE_BUILTIN: {
    return gab_obj_string_create(gab, s_i8_cstr("[builtin]"));
  }
  case TYPE_SYMBOL: {
    return gab_obj_string_create(gab, s_i8_cstr("[symbol]"));
  }
  case TYPE_CONTAINER: {
    return gab_obj_string_create(gab, s_i8_cstr("[container]"));
  }
  case TYPE_EFFECT: {
    return gab_obj_string_create(gab, s_i8_cstr("[effect]"));
  }
  default: {
    fprintf(stderr, "%d is not an object.\n", self->kind);
    exit(0);
  }
  }
}

gab_value gab_val_to_string(gab_engine *gab, gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(
        gab, s_i8_cstr(GAB_VAL_TO_BOOLEAN(self) ? "true" : "false")));
  }

  if (GAB_VAL_IS_NIL(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("nil")));
  }

  if (GAB_VAL_IS_NUMBER(self)) {
    char str[24];
    snprintf(str, 24, "%g", GAB_VAL_TO_NUMBER(self));
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(str)));
  }

  if (GAB_VAL_IS_PRIMITIVE(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("[primitive]")));
  }

  return GAB_VAL_OBJ(gab_obj_to_obj_string(gab, GAB_VAL_TO_OBJ(self)));
}

void gab_obj_dump(gab_value value) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case TYPE_STRING: {
    gab_obj_string *obj = GAB_VAL_TO_STRING(value);
    printf("%.*s", (i32)obj->len, (const char *)obj->data);
    break;
  }
  case TYPE_MESSAGE: {
    gab_obj_message *obj = GAB_VAL_TO_MESSAGE(value);
    printf("[message:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case TYPE_UPVALUE: {
    gab_obj_upvalue *upv = GAB_VAL_TO_UPVALUE(value);
    printf("[upvalue:");
    gab_val_dump(*upv->data);
    printf("]");
    break;
  }
  case TYPE_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    printf("[shape:%.*s]", (i32)shape->name.len, shape->name.data);
    break;
  }
  case TYPE_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    printf("[object:%.*s]", (i32)obj->shape->name.len, obj->shape->name.data);
    break;
  }
  case TYPE_BUILTIN: {
    gab_obj_builtin *obj = GAB_VAL_TO_BUILTIN(value);
    printf("[builtin:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case TYPE_CONTAINER: {
    gab_obj_container *obj = GAB_VAL_TO_CONTAINER(value);
    printf("[container:%p]", obj->destructor);
    break;
  }
  case TYPE_SYMBOL: {
    gab_obj_symbol *obj = GAB_VAL_TO_SYMBOL(value);
    printf("[symbol:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case TYPE_PROTOTYPE: {
    printf("[prototype]");
    break;
  }
  case TYPE_CLOSURE: {
    gab_obj_closure *obj = GAB_VAL_TO_CLOSURE(value);
    printf("[closure:%.*s]", (i32)obj->p->name.len, obj->p->name.data);
    break;
  }
  case TYPE_EFFECT: {
    gab_obj_effect *obj = GAB_VAL_TO_EFFECT(value);
    printf("[effect:%.*s]", (i32)obj->c->p->name.len, obj->c->p->name.data);
    break;
  }
  default: {
    fprintf(stderr, "%d is not an object.\n", GAB_VAL_TO_OBJ(value)->kind);
    exit(0);
  }
  }
}

/*
  A generic function used to free a gab object of any kind.
*/
void gab_obj_destroy(gab_engine *gab, gab_vm *vm, gab_obj *self) {
  switch (self->kind) {
  case TYPE_RECORD: {
    gab_obj_record *rec = (gab_obj_record *)self;

    if (rec->dyn_data.cap) {
      v_u64_destroy(&rec->dyn_data);
    }

    GAB_DESTROY_STRUCT(rec);
    return;
  }
  case TYPE_CONTAINER: {
    gab_obj_container *container = (gab_obj_container *)(self);
    container->destructor(gab, vm, container->data);
    GAB_DESTROY_STRUCT(container);
    return;
  }

  case TYPE_MESSAGE: {
    gab_obj_message *function = (gab_obj_message *)(self);
    d_specs_destroy(&function->specs);
    GAB_DESTROY_STRUCT(function);
    return;
  }

    /*
      These cases are allocated with a single malloc() call, so don't require
      anything fancy.
    */
  case TYPE_SHAPE:
  case TYPE_EFFECT:
  case TYPE_UPVALUE:
  case TYPE_PROTOTYPE:
  case TYPE_CLOSURE:
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_STRING: {
    GAB_DESTROY_STRUCT(self);
    return;
  }
  case TYPE_BOOLEAN:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_UNDEFINED:
  case GAB_NTYPES:
    return;
  }
}

static inline u64 keys_hash(u64 size, u64 stride, gab_value values[size]) {
  gab_value words[size];
  for (u64 i = 0; i < size; i++) {
    words[i] = values[i * stride];
  }
  return hash_words(size, words);
};

gab_obj_string *gab_obj_string_create(gab_engine *gab, s_i8 str) {
  u64 hash = s_i8_hash(str);

  gab_obj_string *interned = gab_engine_find_string(gab, str, hash);

  if (interned != NULL)
    return interned;

  gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, str.len, TYPE_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  // Strings cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_obj_string *gab_obj_string_concat(gab_engine *gab, gab_obj_string *a,
                                      gab_obj_string *b) {

  if (a->len == 0)
    return b;

  if (b->len == 0)
    return a;

  u64 size = a->len + b->len;

  gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, size, TYPE_STRING);

  // Copy the data into the string obj.
  memcpy(self->data, a->data, a->len);

  memcpy(self->data + a->len, b->data, b->len);

  self->len = size;

  // Pre compute the hash
  s_i8 ref = s_i8_create(self->data, self->len);
  self->hash = s_i8_hash(ref);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  gab_obj_string *interned = gab_engine_find_string(gab, ref, self->hash);
  if (interned) {
    DESTROY(self);
    return interned;
  }

  // Intern the string in the module
  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  // Strings cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  // The module contains a reference to the string, so add a reference.
  return self;
};

/*
  Get a reference to a gab string's string data.
*/
s_i8 gab_obj_string_ref(gab_obj_string *self) {
  // Ignore the null character.
  s_i8 ref = {.data = self->data, .len = self->len};
  return ref;
}

gab_obj_prototype *gab_obj_prototype_create(gab_module *mod, s_i8 name) {
  gab_obj_prototype *self = GAB_CREATE_OBJ(gab_obj_prototype, TYPE_PROTOTYPE);

  self->mod = mod;
  self->name = name;
  self->narguments = 0;
  self->nslots = 0;
  self->nupvalues = 0;
  self->var = 0;
  self->offset = 0;
  self->len = 0;

  return self;
}

gab_obj_message *gab_obj_message_create(gab_engine *gab, s_i8 name) {
  u64 hash = s_i8_hash(name);

  gab_obj_message *interned = gab_engine_find_message(gab, name, hash);

  if (interned != NULL)
    return interned;

  gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, TYPE_MESSAGE);

  d_specs_create(&self->specs, 8);
  self->name = name;
  self->hash = hash;
  self->version = 0;

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_builtin function, s_i8 name) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, TYPE_BUILTIN);

  self->function = function;
  self->name = name;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_closure *gab_obj_closure_create(gab_obj_prototype *p) {
  gab_obj_closure *self = GAB_CREATE_FLEX_OBJ(gab_obj_closure, gab_obj_upvalue,
                                              p->nupvalues, TYPE_CLOSURE);

  self->nupvalues = p->nupvalues;
  self->p = p;

  return self;
}

gab_obj_upvalue *gab_obj_upvalue_create(gab_value *data) {
  gab_obj_upvalue *self = GAB_CREATE_OBJ(gab_obj_upvalue, TYPE_UPVALUE);
  self->data = data;
  self->closed = GAB_VAL_NIL();
  self->next = NULL;
  return self;
}

gab_obj_shape *gab_obj_shape_create_array(gab_engine *gab, gab_vm *vm,
                                          u64 len) {
  gab_value keys[len];

  for (u64 i = 0; i < len; i++) {
    keys[i] = GAB_VAL_NUMBER(i);
  }

  return gab_obj_shape_create(gab, vm, len, 1, keys);
}

gab_obj_shape *gab_obj_shape_create(gab_engine *gab, gab_vm *vm, u64 len,
                                    u64 stride, gab_value keys[len]) {
  u64 hash = keys_hash(len, stride, keys);

  gab_obj_shape *interned = gab_engine_find_shape(gab, len, stride, hash, keys);

  if (interned)
    return interned;

  gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, TYPE_SHAPE);

  self->hash = hash;
  self->len = len;
  self->name = s_i8_cstr("anonymous");

  for (u64 i = 0; i < len; i++) {
    self->keys[i] = keys[i * stride];
  }

  gab_gc_iref_many(gab, vm, &gab->gc, len, self->keys);

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_shape *gab_obj_shape_grow(gab_engine *gab, gab_vm *vm,
                                  gab_obj_shape *self, gab_value key) {
  gab_value keys[self->len + 1];

  memcpy(keys, self->keys, sizeof(gab_value) * self->len);

  keys[self->len] = key;

  return gab_obj_shape_create(gab, vm, self->len + 1, 1, keys);
}

gab_obj_record *gab_obj_record_create(gab_obj_shape *shape, u64 len, u64 stride,
                                      gab_value values[len]) {

  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, len, TYPE_RECORD);

  self->shape = shape;
  self->dyn_data.len = len;
  self->dyn_data.cap = 0;
  self->dyn_data.data = NULL;

  for (u64 i = 0; i < len; i++) {
    self->data[i] = values[i * stride];
  }

  return self;
}

u16 gab_obj_record_grow(gab_engine *gab, gab_vm *vm, gab_obj_record *self,
                        gab_value key, gab_value value) {
  gab_obj_shape *new_shape = gab_obj_shape_grow(gab, vm, self->shape, key);

  u64 len = self->dyn_data.len;

  assert(len < UINT16_MAX);

  if (self->dyn_data.cap == 0) {
    // Make Dynamic
    v_u64_create(&self->dyn_data, len < 8 ? 8 : len * 2);
    memcpy(self->dyn_data.data, self->data, len * sizeof(gab_value));
    self->dyn_data.len = len;
  }

  self->shape = new_shape;
  v_u64_push(&self->dyn_data, value);

  return len;
}

gab_obj_container *gab_obj_container_create(gab_obj_container_cb destructor,
                                            void *data) {

  gab_obj_container *self = GAB_CREATE_OBJ(gab_obj_container, TYPE_CONTAINER);

  self->data = data;
  self->destructor = destructor;

  return self;
}

gab_obj_symbol *gab_obj_symbol_create(s_i8 name) {
  gab_obj_symbol *self = GAB_CREATE_OBJ(gab_obj_symbol, TYPE_SYMBOL);
  self->name = name;
  return self;
}

gab_obj_effect *gab_obj_effect_create(gab_obj_closure *c, u64 offset, u8 have,
                                      u8 want, u8 len, gab_value frame[len]) {
  gab_obj_effect *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_effect, gab_value, len, TYPE_EFFECT);

  self->c = c;
  self->offset = offset;
  self->have = have;
  self->want = want;
  self->len = len;

  for (u8 i = 0; i < len; i++) {
    self->frame[i] = frame[i];
  }

  return self;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
