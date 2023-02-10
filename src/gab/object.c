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
  ((type *)gab_reallocate(gab, NULL, 0, sizeof(type) * count))
#define GAB_CREATE_STRUCT(obj_type)                                            \
  ((obj_type *)gab_reallocate(gab, NULL, 0, sizeof(obj_type)))
#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                \
  ((obj_type *)gab_reallocate(                                                 \
      gab, NULL, 0, sizeof(obj_type) + sizeof(flex_type) * flex_count))

#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((obj_type *)gab_obj_create(gab, (gab_obj *)GAB_CREATE_STRUCT(obj_type),     \
                              kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((obj_type *)gab_obj_create(                                                 \
      gab, (gab_obj *)GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count), \
      kind))

// This can be heavily optimized.
gab_value gab_val_type(gab_engine *gab, gab_value value) {
  // The type of a record is it's shape
  if (GAB_VAL_IS_RECORD(value)) {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return GAB_VAL_OBJ(obj->shape);
  }

  // The type of null or symbols is themselves
  if (GAB_VAL_IS_NIL(value) || GAB_VAL_IS_UNDEFINED(value) ||
      GAB_VAL_IS_SYMBOL(value)) {
    return value;
  }

  if (GAB_VAL_IS_NUMBER(value)) {
    return gab->types[GAB_KIND_NUMBER];
  }

  if (GAB_VAL_IS_BOOLEAN(value)) {
    return gab->types[GAB_KIND_BOOLEAN];
  }

  return gab->types[GAB_VAL_TO_OBJ(value)->kind];
}

boolean gab_val_falsey(gab_value self) {
  return GAB_VAL_IS_NIL(self) || GAB_VAL_IS_FALSE(self);
}

gab_obj *gab_obj_create(gab_engine *gab, gab_obj *self, gab_kind k) {
  self->kind = k;
  // Objects start out with one reference.
  self->references = 1;

  self->flags = 0;
#if GAB_LOG_GC
  printf("Created (%p) \n", self);
  u64 curr_amt = d_u64_read(&gab->gc.object_counts, k);
  d_u64_insert(&gab->gc.object_counts, k, curr_amt + 1);
#endif
  return self;
}

void gab_obj_dump(gab_value value) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case GAB_KIND_STRING: {
    gab_obj_string *obj = GAB_VAL_TO_STRING(value);
    printf("%.*s", (i32)obj->len, (const char *)obj->data);
    break;
  }
  case GAB_KIND_MESSAGE: {
    gab_obj_message *obj = GAB_VAL_TO_MESSAGE(value);
    printf("[message:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case GAB_KIND_UPVALUE: {
    gab_obj_upvalue *upv = GAB_VAL_TO_UPVALUE(value);
    printf("[upvalue:");
    gab_val_dump(*upv->data);
    printf("]");
    break;
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    printf("[shape:%.*s]", (i32)shape->name.len, shape->name.data);
    break;
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    printf("[record:%.*s]", (i32)obj->shape->name.len, obj->shape->name.data);
    break;
  }
  case GAB_KIND_LIST: {
    gab_obj_list *obj = GAB_VAL_TO_LIST(value);
    printf("[list:%p]", obj);
    break;
  }
  case GAB_KIND_MAP: {
    gab_obj_map *obj = GAB_VAL_TO_MAP(value);
    printf("[map:%p]", obj);
    break;
  }
  case GAB_KIND_BUILTIN: {
    gab_obj_builtin *obj = GAB_VAL_TO_BUILTIN(value);
    printf("[builtin:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case GAB_KIND_CONTAINER: {
    gab_obj_container *obj = GAB_VAL_TO_CONTAINER(value);
    printf("[container:%p]", obj->destructor);
    break;
  }
  case GAB_KIND_SYMBOL: {
    gab_obj_symbol *obj = GAB_VAL_TO_SYMBOL(value);
    printf("[symbol:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case GAB_KIND_PROTOTYPE: {
    gab_obj_prototype *obj = GAB_VAL_TO_PROTOTYPE(value);
    printf("[prototype:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case GAB_KIND_BLOCK: {
    gab_obj_block *obj = GAB_VAL_TO_BLOCK(value);
    printf("[block:%.*s]", (i32)obj->p->name.len, obj->p->name.data);
    break;
  }
  case GAB_KIND_EFFECT: {
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
  case GAB_KIND_STRING:
    return (gab_obj_string *)self;
  case GAB_KIND_BLOCK:
    return gab_obj_string_create(gab, s_i8_cstr("[block]"));
  case GAB_KIND_MAP:
    return gab_obj_string_create(gab, s_i8_cstr("[map]"));
  case GAB_KIND_LIST:
    return gab_obj_string_create(gab, s_i8_cstr("[list]"));
  case GAB_KIND_RECORD:
    return gab_obj_string_create(gab, s_i8_cstr("[record]"));
  case GAB_KIND_SHAPE:
    return gab_obj_string_create(gab, s_i8_cstr("[shape]"));
  case GAB_KIND_UPVALUE:
    return gab_obj_string_create(gab, s_i8_cstr("[upvalue]"));
  case GAB_KIND_MESSAGE:
    return gab_obj_string_create(gab, s_i8_cstr("[message]"));
  case GAB_KIND_PROTOTYPE:
    return gab_obj_string_create(gab, s_i8_cstr("[prototype]"));
  case GAB_KIND_BUILTIN:
    return gab_obj_string_create(gab, s_i8_cstr("[builtin]"));
  case GAB_KIND_SYMBOL:
    return gab_obj_string_create(gab, s_i8_cstr("[symbol]"));
  case GAB_KIND_CONTAINER:
    return gab_obj_string_create(gab, s_i8_cstr("[container]"));
  case GAB_KIND_EFFECT:
    return gab_obj_string_create(gab, s_i8_cstr("[effect]"));
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

  if (GAB_VAL_IS_UNDEFINED(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("[undefined]")));
  }

  if (GAB_VAL_IS_OBJ(self)) {
    return GAB_VAL_OBJ(gab_obj_to_obj_string(gab, GAB_VAL_TO_OBJ(self)));
  }

  printf("Tried to convert unhandled type to string\n");

  return GAB_VAL_NIL();
}

/*
  A generic function used to free a gab object of any kind.
*/
void gab_obj_destroy(gab_engine *gab, gab_vm *vm, gab_obj *self) {
  switch (self->kind) {
  case GAB_KIND_LIST: {
    gab_obj_list *lst = (gab_obj_list *)self;

    v_gab_value_destroy(&lst->data);
    return;
  }
  case GAB_KIND_MAP: {
    gab_obj_map *map = (gab_obj_map *)self;
    d_gab_value_destroy(&map->data);
    return;
  }
  case GAB_KIND_CONTAINER: {
    gab_obj_container *container = (gab_obj_container *)(self);
    if (container->destructor != NULL)
      container->destructor(gab, vm, container->data);
    return;
  }
  case GAB_KIND_MESSAGE: {
    gab_obj_message *function = (gab_obj_message *)(self);
    d_specs_destroy(&function->specs);
    return;
  }
  default:
    return;
  }
}

u64 gab_obj_size(gab_obj *self) {
  switch (self->kind) {
  case GAB_KIND_MESSAGE:
    return sizeof(gab_obj_message);
  case GAB_KIND_PROTOTYPE:
    return sizeof(gab_obj_prototype);
  case GAB_KIND_BUILTIN:
    return sizeof(gab_obj_builtin);
  case GAB_KIND_UPVALUE:
    return sizeof(gab_obj_upvalue);
  case GAB_KIND_SYMBOL:
    return sizeof(gab_obj_symbol);
  case GAB_KIND_CONTAINER:
    return sizeof(gab_obj_container);
  case GAB_KIND_LIST:
    return sizeof(gab_obj_list);
  case GAB_KIND_MAP:
    return sizeof(gab_obj_map);
  case GAB_KIND_BLOCK: {
    gab_obj_block *obj = (gab_obj_block *)self;
    return sizeof(gab_obj_block) + obj->nupvalues * sizeof(gab_value);
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *obj = (gab_obj_record *)self;
    return sizeof(gab_obj_record) + obj->len * sizeof(gab_value);
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *obj = (gab_obj_shape *)self;
    return sizeof(gab_obj_shape) + obj->len * sizeof(gab_value);
  }
  case GAB_KIND_EFFECT: {
    gab_obj_effect *obj = (gab_obj_effect *)self;
    return sizeof(gab_obj_effect) + obj->len * sizeof(gab_value);
  }
  case GAB_KIND_STRING: {
    gab_obj_string *obj = (gab_obj_string *)self;
    return sizeof(gab_obj_string) + obj->len * sizeof(i8);
  }
  default:
    return sizeof(gab_value);
  }
}

static inline u64 hash_keys(u64 seed, u64 len, u64 stride,
                            gab_value values[len]) {
  gab_value words[len];
  for (u64 i = 0; i < len; i++) {
    words[i] = values[i * stride];
  }
  return hash_words(seed, len, words);
};

gab_obj_string *gab_obj_string_create(gab_engine *gab, s_i8 str) {
  u64 hash = s_i8_hash(str, gab->hash_seed);

  gab_obj_string *interned = gab_engine_find_string(gab, str, hash);

  if (interned != NULL)
    return interned;

  gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, str.len, GAB_KIND_STRING);

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
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, size, GAB_KIND_STRING);

  // Copy the data into the string obj.
  memcpy(self->data, a->data, a->len);

  memcpy(self->data + a->len, b->data, b->len);

  self->len = size;

  // Pre compute the hash
  s_i8 ref = s_i8_create(self->data, self->len);
  self->hash = s_i8_hash(ref, gab->hash_seed);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  gab_obj_string *interned = gab_engine_find_string(gab, ref, self->hash);
  if (interned) {
    gab_reallocate(gab, self, gab_obj_size((gab_obj *)self), 0);
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

gab_obj_prototype *gab_obj_prototype_create(gab_engine *gab, gab_module *mod,
                                            s_i8 name) {
  gab_obj_prototype *self =
      GAB_CREATE_OBJ(gab_obj_prototype, GAB_KIND_PROTOTYPE);

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
  u64 hash = s_i8_hash(name, gab->hash_seed);

  gab_obj_message *interned = gab_engine_find_message(gab, name, hash);

  if (interned != NULL)
    return interned;

  gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, GAB_KIND_MESSAGE);

  d_specs_create(&self->specs, 8);
  self->name = name;
  self->hash = hash;
  self->version = 0;

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_engine *gab, gab_builtin function,
                                        s_i8 name) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, GAB_KIND_BUILTIN);

  self->function = function;
  self->name = name;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_block *gab_obj_block_create(gab_engine *gab, gab_obj_prototype *p) {
  gab_obj_block *self = GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value,
                                            p->nupvalues, GAB_KIND_BLOCK);

  self->nupvalues = p->nupvalues;
  self->p = p;

  return self;
}

gab_obj_upvalue *gab_obj_upvalue_create(gab_engine *gab, gab_value *data) {
  gab_obj_upvalue *self = GAB_CREATE_OBJ(gab_obj_upvalue, GAB_KIND_UPVALUE);
  self->data = data;
  self->closed = GAB_VAL_NIL();
  self->next = NULL;
  return self;
}

gab_obj_shape *gab_obj_shape_create_tuple(gab_engine *gab, gab_vm *vm,
                                          u64 len) {
  gab_value keys[len];

  for (u64 i = 0; i < len; i++) {
    keys[i] = GAB_VAL_NUMBER(i);
  }

  return gab_obj_shape_create(gab, vm, len, 1, keys);
}

gab_obj_shape *gab_obj_shape_create(gab_engine *gab, gab_vm *vm, u64 len,
                                    u64 stride, gab_value keys[len]) {
  u64 hash = hash_keys(gab->hash_seed, len, stride, keys);

  gab_obj_shape *interned = gab_engine_find_shape(gab, len, stride, hash, keys);

  if (interned)
    return interned;

  gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, GAB_KIND_SHAPE);

  self->hash = hash;
  self->len = len;
  self->name = s_i8_cstr("anonymous");

  for (u64 i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  gab_gc_iref_many(gab, vm, &gab->gc, len, self->data);

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_shape *gab_obj_shape_grow(gab_engine *gab, gab_vm *vm,
                                  gab_obj_shape *self, gab_value key) {
  gab_value keys[self->len + 1];

  memcpy(keys, self->data, sizeof(gab_value) * self->len);

  keys[self->len] = key;

  return gab_obj_shape_create(gab, vm, self->len + 1, 1, keys);
}

gab_obj_map *gab_obj_map_create(gab_engine *gab, u64 len, u64 stride,
                                gab_value keys[len], gab_value values[len]) {
  gab_obj_map *self = GAB_CREATE_OBJ(gab_obj_map, GAB_KIND_MAP);

  d_gab_value_create(&self->data, len < 8 ? 8 : len * 2);

  for (u64 i = 0; i < len; i++) {
    d_gab_value_insert(&self->data, keys[i * stride], values[i * stride]);
  }

  return self;
}

gab_obj_list *gab_obj_list_create_empty(gab_engine *gab, u64 len) {
  gab_obj_list *self = GAB_CREATE_OBJ(gab_obj_list, GAB_KIND_LIST);

  v_gab_value_create(&self->data, len);


  return self;
}

gab_obj_list *gab_obj_list_create(gab_engine *gab, u64 len, u64 stride,
                                  gab_value values[len]) {
  gab_obj_list *self = GAB_CREATE_OBJ(gab_obj_list, GAB_KIND_LIST);

  v_gab_value_create(&self->data, len < 8 ? 8 : len * 2);

  self->data.len = len;

  for (u64 i = 0; i < len; i++) {
    self->data.data[i] = values[i * stride];
  }

  return self;
}

gab_obj_record *gab_obj_record_create(gab_engine *gab, gab_obj_shape *shape,
                                      u64 len, u64 stride,
                                      gab_value values[len]) {

  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, len, GAB_KIND_RECORD);

  self->shape = shape;
  self->len = len;

  for (u64 i = 0; i < len; i++) {
    self->data[i] = values[i * stride];
  }

  return self;
}

gab_obj_container *gab_obj_container_create(gab_engine *gab,
                                            gab_obj_container_cb destructor,
                                            void *data) {

  gab_obj_container *self =
      GAB_CREATE_OBJ(gab_obj_container, GAB_KIND_CONTAINER);

  self->data = data;
  self->destructor = destructor;

  return self;
}

gab_obj_symbol *gab_obj_symbol_create(gab_engine *gab, s_i8 name) {
  gab_obj_symbol *self = GAB_CREATE_OBJ(gab_obj_symbol, GAB_KIND_SYMBOL);
  self->name = name;
  return self;
}

gab_obj_effect *gab_obj_effect_create(gab_engine *gab, gab_obj_block *c,
                                      u64 offset, u8 have, u8 want, u8 len,
                                      gab_value frame[len]) {
  gab_obj_effect *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_effect, gab_value, len, GAB_KIND_EFFECT);

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
