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

gab_obj *gab_obj_create(gab_engine *gab, gab_obj *self, gab_kind k) {
  // Maintain the global list of objects
  self->next = gab->objects;

  gab->objects = self;

  self->kind = k;
  // Objects start out with one reference.
  self->references = 1;

  self->flags = 0;

#if GAB_LOG_GC
  u64 curr_amt = d_u64_read(&gab->gc.object_counts, k);
  d_u64_insert(&gab->gc.object_counts, k, curr_amt + 1);
#endif

  return self;
}

i32 gab_obj_dump(FILE *stream, gab_value value) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case GAB_KIND_STRING: {
    gab_obj_string *obj = GAB_VAL_TO_STRING(value);
    return fprintf(stream, "%.*s", (i32)obj->len, (const char *)obj->data);
  }
  case GAB_KIND_MESSAGE: {
    gab_obj_message *obj = GAB_VAL_TO_MESSAGE(value);
    return fprintf(stream, "[message:%V]", obj->name);
  }
  case GAB_KIND_UPVALUE: {
    gab_obj_upvalue *upv = GAB_VAL_TO_UPVALUE(value);
    return fprintf(stream, "[upvalue:%V]", upv->data);
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    return fprintf(stream, "[shape:%V]", shape->name);
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    return fprintf(stream, "[record:%V]", obj->shape->name);
  }
  case GAB_KIND_LIST: {
    gab_obj_list *obj = GAB_VAL_TO_LIST(value);
    return fprintf(stream, "[list:%p]", obj);
  }
  case GAB_KIND_MAP: {
    gab_obj_map *obj = GAB_VAL_TO_MAP(value);
    return fprintf(stream, "[map:%p]", obj);
  }
  case GAB_KIND_BUILTIN: {
    gab_obj_builtin *obj = GAB_VAL_TO_BUILTIN(value);
    return fprintf(stream, "[builtin:%V]", obj->name);
  }
  case GAB_KIND_CONTAINER: {
    gab_obj_container *obj = GAB_VAL_TO_CONTAINER(value);
    return fprintf(stream, "[container:%p]", obj->destructor);
  }
  case GAB_KIND_SYMBOL: {
    gab_obj_symbol *obj = GAB_VAL_TO_SYMBOL(value);
    return fprintf(stream, "[symbol:%V]", obj->name);
  }
  case GAB_KIND_PROTOTYPE: {
    gab_obj_prototype *obj = GAB_VAL_TO_PROTOTYPE(value);
    gab_value name =
        v_gab_constant_val_at(&obj->mod->constants, obj->mod->name);
    return fprintf(stream, "[prototype:%V]", name);
  }
  case GAB_KIND_BLOCK: {
    gab_obj_block *obj = GAB_VAL_TO_BLOCK(value);
    gab_value name =
        v_gab_constant_val_at(&obj->p->mod->constants, obj->p->mod->name);
    return fprintf(stream, "[block:%V]", name);
  }
  case GAB_KIND_EFFECT: {
    gab_obj_effect *obj = GAB_VAL_TO_EFFECT(value);
    gab_value name =
        v_gab_constant_val_at(&obj->c->p->mod->constants, obj->c->p->mod->name);
    return fprintf(stream, "[effect:%V]", name);
  }
  default: {
    fprintf(stderr, "%d is not an object.\n", GAB_VAL_TO_OBJ(value)->kind);
    exit(0);
  }
  }
}

i32 gab_val_dump(FILE *stream, gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    return fprintf(stream, "%s", GAB_VAL_TO_BOOLEAN(self) ? "true" : "false");
  } else if (GAB_VAL_IS_NUMBER(self)) {
    return fprintf(stream, "%g", GAB_VAL_TO_NUMBER(self));
  } else if (GAB_VAL_IS_NIL(self)) {
    return fprintf(stream, "nil");
  } else if (GAB_VAL_IS_OBJ(self)) {
    return gab_obj_dump(stream, self);
  } else if (GAB_VAL_IS_PRIMITIVE(self)) {
    return fprintf(stream, "[primitive:%s]",
                   gab_opcode_names[GAB_VAL_TO_PRIMITIVE(self)]);
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
  case GAB_KIND_PROTOTYPE: {
    gab_obj_prototype *obj = (gab_obj_prototype *)self;
    return sizeof(gab_obj_prototype) + obj->nupvalues * 2;
  }
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

  u64 len = a->len + b->len;

  i8 data[len];

  // Copy the data into the string obj.
  memcpy(data, a->data, a->len);

  memcpy(data + a->len, b->data, b->len);

  // Pre compute the hash
  s_i8 ref = s_i8_create(data, len);
  u64 hash = s_i8_hash(ref, gab->hash_seed);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  gab_obj_string *interned = gab_engine_find_string(gab, ref, hash);

  if (interned)
    return interned;

  gab_obj_string *self = gab_obj_string_create(gab, ref);

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
  s_i8 ref = {.data = self->data, .len = self->len};
  return ref;
}

gab_obj_prototype *gab_obj_prototype_create(gab_engine *gab, gab_module *mod,
                                            u8 narguments, u8 nslots,
                                            u8 nupvalues, u8 nlocals,
                                            boolean var, u8 flags[nupvalues],
                                            u8 indexes[nupvalues]) {
  gab_obj_prototype *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_prototype, u8, nupvalues * 2, GAB_KIND_PROTOTYPE);

  self->mod = mod;
  self->narguments = narguments;
  self->nslots = nslots;
  self->nupvalues = nupvalues;
  self->nlocals = nlocals;
  self->var = var;

  for (u8 i = 0; i < nupvalues; i++) {
    self->upv_desc[i * 2] = flags[i];
    self->upv_desc[i * 2 + 1] = indexes[i];
  }

  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_message *gab_obj_message_create(gab_engine *gab, gab_value name) {
  u64 hash = GAB_VAL_TO_STRING(name)->hash;
  gab_obj_message *interned = gab_engine_find_message(gab, name, hash);

  if (interned != NULL)
    return interned;

  gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, GAB_KIND_MESSAGE);

  d_specs_create(&self->specs, 8);
  self->name = name;
  self->hash = hash;
  self->version = 0;

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_engine *gab, gab_builtin function,
                                        gab_value name) {

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

  for (u8 i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = GAB_VAL_NIL();
  }

  return self;
}

gab_obj_upvalue *gab_obj_upvalue_create(gab_engine *gab, gab_value *data) {
  gab_obj_upvalue *self = GAB_CREATE_OBJ(gab_obj_upvalue, GAB_KIND_UPVALUE);
  self->data = data;
  self->closed = GAB_VAL_NIL();
  self->next = NULL;

  GAB_OBJ_GREEN((gab_obj *)self);
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
  self->name = GAB_STRING("anonymous");

  for (u64 i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  gab_gc_iref_many(gab, vm, &gab->gc, len, self->data);

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  GAB_OBJ_GREEN((gab_obj *)self);

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
                                      u64 stride, gab_value values[]) {

  gab_obj_record *self = GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value,
                                             shape->len, GAB_KIND_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (u64 i = 0; i < shape->len; i++) {
    self->data[i] = values[i * stride];
  }

  return self;
}

gab_obj_record *gab_obj_record_create_empty(gab_engine *gab,
                                            gab_obj_shape *shape) {
  gab_obj_record *self = GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value,
                                             shape->len, GAB_KIND_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (u64 i = 0; i < shape->len; i++) {
    self->data[i] = GAB_VAL_NIL();
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

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_symbol *gab_obj_symbol_create(gab_engine *gab, gab_value name) {
  gab_obj_symbol *self = GAB_CREATE_OBJ(gab_obj_symbol, GAB_KIND_SYMBOL);
  self->name = name;

  GAB_OBJ_GREEN((gab_obj *)self);

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
