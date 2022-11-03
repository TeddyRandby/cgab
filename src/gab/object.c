#include "include/object.h"
#include "include/core.h"
#include "include/engine.h"
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
gab_obj *gab_obj_create(gab_obj *self, gab_obj_kind k) {
  self->kind = k;
  // Objects start out with one reference.
  self->references = 1;

  self->flags = 0;
  return self;
}

void gab_val_dump(gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    printf("%s", GAB_VAL_TO_BOOLEAN(self) ? "true" : "false");
  } else if (GAB_VAL_IS_NUMBER(self)) {
    printf("%g", GAB_VAL_TO_NUMBER(self));
  } else if (GAB_VAL_IS_NULL(self)) {
    printf("null");
  } else if (GAB_VAL_IS_OBJ(self)) {
    gab_obj_dump(self);
  }
}

/*
  Helpers for converting a gab value to a string object.
*/
gab_obj_string *gab_obj_to_obj_string(gab_engine *gab, gab_obj *self) {
  switch (self->kind) {
  case OBJECT_STRING: {
    return (gab_obj_string *)self;
  }
  case OBJECT_CLOSURE: {
    return gab_obj_string_create(gab, s_i8_cstr("[closure]"));
  }
  case OBJECT_RECORD: {
    return gab_obj_string_create(gab, s_i8_cstr("[record]"));
  }
  case OBJECT_SHAPE: {
    return gab_obj_string_create(gab, s_i8_cstr("[shape]"));
  }
  case OBJECT_UPVALUE: {
    return gab_obj_string_create(gab, s_i8_cstr("[upvalue]"));
  }
  case OBJECT_FUNCTION: {
    return gab_obj_string_create(gab, s_i8_cstr("[function]"));
  }
  case OBJECT_BUILTIN: {
    return gab_obj_string_create(gab, s_i8_cstr("[builtin]"));
  }
  case OBJECT_SYMBOL: {
    return gab_obj_string_create(gab, s_i8_cstr("[symbol]"));
  }
  case OBJECT_CONTAINER: {
    return gab_obj_string_create(gab, s_i8_cstr("[container]"));
  }
  }
}

gab_value gab_val_to_string(gab_engine *gab, gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(
        gab, s_i8_cstr(GAB_VAL_TO_BOOLEAN(self) ? "true" : "false")));
  }

  if (GAB_VAL_IS_NULL(self)) {
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("null")));
  }

  if (GAB_VAL_IS_NUMBER(self)) {
    char str[24];
    snprintf(str, 24, "%g", GAB_VAL_TO_NUMBER(self));
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(str)));
  }

  return GAB_VAL_OBJ(gab_obj_to_obj_string(gab, GAB_VAL_TO_OBJ(self)));
}

void gab_obj_dump(gab_value value) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case OBJECT_STRING: {
    gab_obj_string *obj = GAB_VAL_TO_STRING(value);
    printf("%.*s", (i32)obj->len, (const char *)obj->data);
    break;
  }
  case OBJECT_FUNCTION: {
    gab_obj_function *obj = GAB_VAL_TO_FUNCTION(value);
    printf("[function:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case OBJECT_CLOSURE: {
    gab_obj_closure *obj = GAB_VAL_TO_CLOSURE(value);
    printf("[closure:%.*s]", (i32)obj->func->name.len,
           (char *)obj->func->name.data);
    break;
  }
  case OBJECT_UPVALUE: {
    gab_obj_upvalue *upv = GAB_VAL_TO_UPVALUE(value);
    printf("[upvalue:");
    gab_val_dump(*upv->data);
    printf("]");
    break;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    printf("[shape:%.*s]", (i32)shape->name.len, shape->name.data);
    break;
  }
  case OBJECT_RECORD: {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(value);
    printf("[object:%.*s]", (i32)obj->shape->name.len, obj->shape->name.data);
    break;
  }
  case OBJECT_BUILTIN: {
    gab_obj_builtin *obj = GAB_VAL_TO_BUILTIN(value);
    printf("[builtin:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  case OBJECT_CONTAINER: {
    gab_obj_container *obj = GAB_VAL_TO_CONTAINER(value);
    printf("[container:%p]", obj->destructor);
    break;
  }
  case OBJECT_SYMBOL: {
    gab_obj_symbol *obj = GAB_VAL_TO_SYMBOL(value);
    printf("[symbol:%.*s]", (i32)obj->name.len, obj->name.data);
    break;
  }
  }
}

/*
  A generic function used to free a gab object of any kind.
*/
void gab_obj_destroy(gab_obj *self) {
  switch (self->kind) {
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)(self);
    d_u64_destroy(&shape->properties);
    GAB_DESTROY_STRUCT(shape);
    return;
  }

  case OBJECT_RECORD: {
    gab_obj_record *object = (gab_obj_record *)(self);
    if (object->is_dynamic) {
      v_u64_destroy(&object->dynamic_values);
    }
    GAB_DESTROY_STRUCT(object);
    return;
  }

  case OBJECT_CONTAINER: {
    gab_obj_container *container = (gab_obj_container *)(self);
    container->destructor(container);
    GAB_DESTROY_STRUCT(container);
    return;
  }

  /*
    These cases are allocated with a single malloc() call, so don't require
    anything fancy.
  */
  case OBJECT_FUNCTION:
  case OBJECT_CLOSURE:
  case OBJECT_UPVALUE:
  case OBJECT_BUILTIN:
  case OBJECT_SYMBOL:
  case OBJECT_STRING: {
    GAB_DESTROY_STRUCT(self);
    return;
  }
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
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, str.len, OBJECT_STRING);

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
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, size, OBJECT_STRING);

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
    gab_obj_destroy((gab_obj *)self);
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

gab_obj_function *gab_obj_function_create(u8 narguments, u8 nupvalues,
                                          u8 nlocals, u64 offset, s_i8 name) {
  gab_obj_function *self = GAB_CREATE_OBJ(gab_obj_function, OBJECT_FUNCTION);

  self->narguments = narguments;
  self->nupvalues = nupvalues;
  self->nlocals = nlocals;
  self->name = name;
  self->offset = offset;

  // Functions cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_builtin function, s_i8 name) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, OBJECT_BUILTIN);

  self->function = function;
  self->name = name;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_closure *gab_obj_closure_create(gab_obj_function *func,
                                        gab_value upvs[]) {
  gab_obj_closure *self = GAB_CREATE_FLEX_OBJ(gab_obj_closure, gab_obj_upvalue,
                                              func->nupvalues, OBJECT_CLOSURE);

  self->func = func;

  memcpy(self->upvalues, upvs, sizeof(gab_value) * func->nupvalues);
  return self;
}

gab_obj_upvalue *gab_obj_upvalue_create(gab_value *slot) {
  gab_obj_upvalue *self = GAB_CREATE_OBJ(gab_obj_upvalue, OBJECT_UPVALUE);
  self->data = slot;
  self->closed = GAB_VAL_NULL();
  self->next = NULL;
  return self;
}

gab_obj_shape *gab_obj_shape_create_array(gab_engine *gab, gab_vm *vm,
                                          u64 size) {
  gab_value keys[size];

  for (u64 i = 0; i < size; i++) {
    keys[i] = GAB_VAL_NUMBER(i);
  }

  return gab_obj_shape_create(gab, vm, size, 1, keys);
}

gab_obj_shape *gab_obj_shape_create(gab_engine *gab, gab_vm *vm, u64 size,
                                    u64 stride, gab_value keys[size]) {
  u64 hash = keys_hash(size, stride, keys);

  gab_obj_shape *interned =
      gab_engine_find_shape(gab, size, stride, hash, keys);

  if (interned)
    return interned;

  if (vm != NULL) {
    for (u64 i = 0; i < size; i++) {
      gab_gc_iref(&gab->gc, vm, keys[i * stride]);
    }
  }

  gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, size, OBJECT_SHAPE);

  self->hash = hash;
  self->name = s_i8_cstr("anonymous");

  d_u64_create(&self->properties, OBJECT_INITIAL_CAP);

  for (u64 i = 0; i < size; i++) {
    gab_value key = keys[i * stride];

    self->keys[i] = key;

    d_u64_insert(&self->properties, key, i);
  }

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_shape *gab_obj_shape_grow(gab_engine *gab, gab_vm* vm, gab_obj_shape *self,
                                  gab_value property) {
  gab_value keys[self->properties.len + 1];

  memcpy(keys, self->keys, self->properties.len * sizeof(gab_value));

  keys[self->properties.len] = property;

  return gab_obj_shape_create(gab, vm, self->properties.len + 1, 1, keys);
}

gab_obj_record *gab_obj_record_create(gab_obj_shape *shape, u64 size,
                                      u64 stride, gab_value values[size]) {

  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, size, OBJECT_RECORD);

  self->shape = shape;
  self->static_len = size;

  self->is_dynamic = false;
  self->dynamic_values.len = 0;
  self->dynamic_values.cap = 0;
  self->dynamic_values.data = NULL;

  for (u64 i = 0; i < size; i++) {
    self->static_values[i] = values[i * stride];
  }

  return self;
}

i16 gab_obj_record_grow(gab_engine *gab, gab_vm* vm, gab_obj_record *self,
                        gab_value key, gab_value value) {
  self->shape = gab_obj_shape_grow(gab, vm, self->shape, key);

  if (!self->is_dynamic) {
    self->is_dynamic = true;

    u64 len = self->static_len < 8 ? 8 : self->static_len * 2;

    v_u64_create(&self->dynamic_values, len);

    for (u8 i = 0; i < self->static_len; i++) {
        v_u64_push(&self->dynamic_values, self->static_values[i]);
    }
  }

  return v_u64_push(&self->dynamic_values, value);
}

void gab_obj_record_shrink(gab_engine *gab, gab_vm* vm, gab_obj_record *self,
                           gab_value key) {
  if (self->is_dynamic) {
  }
};

gab_obj_container *gab_obj_container_create(gab_obj_container_cb destructor,
                                            void *data) {

  gab_obj_container *self = GAB_CREATE_OBJ(gab_obj_container, OBJECT_CONTAINER);

  self->data = data;
  self->destructor = destructor;

  return self;
}

gab_obj_symbol *gab_obj_symbol_create(s_i8 name) {
  gab_obj_symbol *self = GAB_CREATE_OBJ(gab_obj_symbol, OBJECT_SYMBOL);
  self->name = name;
  return self;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
