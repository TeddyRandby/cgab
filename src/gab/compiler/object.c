#include "object.h"
#include "engine.h"

#include <stdio.h>

/*
  Helper macros for allocating gab objects.
*/
#define GAB_CREATE_OBJ(obj_type, eng, kind)                                    \
  ((obj_type *)gab_obj_create((gab_obj *)GAB_CREATE_STRUCT(obj_type, eng->gc), \
                              eng, kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, eng, kind)        \
  ((obj_type *)gab_obj_create((gab_obj *)GAB_CREATE_FLEX_STRUCT(               \
                                  obj_type, flex_type, flex_count, eng->gc),   \
                              eng, kind))

/*
  Initialize the header of any gab objects, with kind (k)
*/
gab_obj *gab_obj_create(gab_obj *self, gab_engine *eng, gab_obj_kind k) {
  self->kind = k;
  // References start at one
  self->references = 1;
  // Zero out the flags.
  self->flags = 0;
  return self;
}

/*
  Helpers for converting a gab value to a string object.
*/
gab_obj_string *gab_val_to_obj_string(gab_value self, gab_engine *eng) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    return gab_obj_string_create(
        eng, s_u8_ref_create_cstr(GAB_VAL_TO_BOOLEAN(self) ? "true" : "false"));
  }

  if (GAB_VAL_IS_NULL(self)) {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("null"));
  }

  if (GAB_VAL_IS_NUMBER(self)) {
    char str[24];
    snprintf(str, 24, "%g", GAB_VAL_TO_NUMBER(self));
    return gab_obj_string_create(eng, s_u8_ref_create_cstr(str));
  }

  return gab_obj_to_obj_string(GAB_VAL_TO_OBJ(self), eng);
}

gab_obj_string *gab_obj_to_obj_string(gab_obj *self, gab_engine *eng) {
  switch (self->kind) {
  case OBJECT_STRING: {
    return (gab_obj_string *)self;
  }
  case OBJECT_CLOSURE: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<closure>"));
  }
  case OBJECT_OBJECT: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<object>"));
  }
  case OBJECT_UPVALUE: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<upvalue>"));
  }
  case OBJECT_FUNCTION: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<function>"));
  }
  case OBJECT_BUILTIN: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<builtin>"));
  }
  default: {
    return gab_obj_string_create(eng, s_u8_ref_create_cstr("<unknown>"));
  }
  }
}

/*
  A generic function used to free a gab object of any kind.
*/
void gab_obj_destroy(gab_obj *self, gab_engine *eng) {
#if GAB_LOG_GC
  printf("Freeing obj: [");
  gab_val_dump(GAB_VAL_OBJ(self));
  printf("]\n");
#endif
  switch (self->kind) {
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)(self);
    d_u64_destroy(&shape->properties);
    GAB_DESTROY_STRUCT(shape, eng->gc);
    return;
  }

  case OBJECT_OBJECT: {
    gab_obj_object *object = (gab_obj_object *)(self);
    if (object->is_dynamic) {
      v_u64_destroy(&object->dynamic_values);
    }
    GAB_DESTROY_STRUCT(object, eng->gc);
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
  case OBJECT_STRING: {
    GAB_DESTROY_STRUCT(self, eng->gc);
    return;
  }
  }
}

static inline u64 s_u8_ref_hash(s_u8_ref str) {
  u64 hash = 2166136261u;
  for (int i = 0; i < str.size; i++) {
    hash ^= (u8)str.data[i];
    hash *= 16777619;
  }
  return hash;
};

static inline u64 keys_hash(gab_value values[], u64 size, u64 stride) {
  u64 hash = 2166136261u;
  for (int i = 0; i < size; i++) {
    hash ^= (u8)values[i * stride];
    hash *= 16777619;
  }
  return hash;
};

gab_obj_string *gab_obj_string_create(gab_engine *eng, s_u8_ref other) {
  u64 hash = s_u8_ref_hash(other);

  gab_obj_string *interned = gab_engine_find_string(eng, other, hash);
  if (interned != NULL)
    return interned;

  gab_obj_string *self = GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, other.size + 1,
                                             eng, OBJECT_STRING);
  self->hash = hash;

  // Then copy the string data.
  COPY(self->data, other.data, other.size);

  // Copy the size and insert the null char.
  self->size = other.size + 1;
  self->data[other.size] = '\0';

  // Intern the string in the module
  gab_engine_add_constant(eng, GAB_VAL_OBJ(self));

  // Strings cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_obj_string *gab_obj_string_concat(gab_engine *eng, gab_obj_string *a,
                                      gab_obj_string *b) {

  if (a->size == 0)
    return b;

  if (b->size == 0)
    return a;

  u64 size = a->size + b->size - 1;

  gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, size, eng, OBJECT_STRING);

  // Copy the data into the string obj.
  // printf("Copying %lu bytes from a to concated.\n", a->size - 1);
  COPY(self->data, a->data, a->size);

  // printf("a before copy: [%s]\n", a->data);

  // printf("Copying %lu bytes from b to concated, starting %lu bytes in.\n",
  //        b->size - 1, a->size - 1);
  COPY(self->data + a->size - 1, b->data, b->size);

  // printf("a after copy: [%s]\n", a->data);

  self->size = size;

  // Add the null
  self->data[self->size - 1] = '\0';

  // Pre compute the hash
  s_u8_ref ref = s_u8_ref_create(self->data, self->size);
  self->hash = s_u8_ref_hash(ref);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  gab_obj_string *interned = gab_engine_find_string(eng, ref, self->hash);
  if (interned != NULL) {
    gab_obj_destroy((gab_obj *)self, eng);
    return interned;
  }

  // Intern the string in the module
  gab_engine_add_constant(eng, GAB_VAL_OBJ(self));

  // Strings cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  // The module contains a reference to the string, so add a reference.
  return self;
};

/*
  Get a reference to a gab string's string data.
*/
s_u8_ref gab_obj_string_get_ref(gab_obj_string *self) {
  // Ignore the null character.
  s_u8_ref ref = {.data = self->data, .size = self->size - 1};
  return ref;
}

gab_obj_function *gab_obj_function_create(gab_engine *eng, s_u8_ref name) {

  gab_obj_function *self =
      GAB_CREATE_OBJ(gab_obj_function, eng, OBJECT_FUNCTION);

  self->narguments = 0;
  self->nupvalues = 0;
  self->nlocals = 0;
  self->name = name;
  self->offset = 0;
  self->module = NULL;

  // Functions cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_engine *eng, gab_builtin function,
                                        const char *name, u8 arity) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, eng, OBJECT_BUILTIN);

  self->function = function;
  self->name = s_u8_ref_create_cstr(name);
  self->narguments = arity;

  // Functions cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_closure *gab_obj_closure_create(gab_engine *eng, gab_obj_function *func,
                                        gab_obj_upvalue *upvs[]) {
  gab_obj_closure *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_closure, gab_obj_upvalue, func->nupvalues, eng, OBJECT_CLOSURE);

  self->func = func;

  COPY(self->upvalues, upvs, sizeof(gab_obj_upvalue *) * func->nupvalues);

  return self;
}

gab_obj_upvalue *gab_obj_upvalue_create(gab_engine *eng, gab_value *slot) {
  gab_obj_upvalue *self = GAB_CREATE_OBJ(gab_obj_upvalue, eng, OBJECT_UPVALUE);
  self->data = slot;
  self->closed = GAB_VAL_NULL();
  self->next = NULL;
  return self;
}

gab_obj_shape *gab_obj_shape_create_arr(gab_engine *eng, u64 size) {
  gab_value keys[size];

  for (u64 i = 0; i < size; i++) {
    keys[i] = GAB_VAL_NUMBER(i);
  }

  return gab_obj_shape_create(eng, keys, size, 1);
}

gab_obj_shape *gab_obj_shape_create(gab_engine *eng, gab_value keys[], u64 size,
                                    u64 stride) {
  u64 hash = keys_hash(keys, size, stride);

  gab_obj_shape *interned =
      gab_engine_find_shape(eng, size, keys, stride, hash);

  if (interned != NULL)
    return interned;

  gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, size, eng, OBJECT_SHAPE);

  self->hash = hash;

  d_u64_create(&self->properties, OBJECT_INITIAL_CAP);

  for (u64 i = 0; i < size; i++) {
    gab_value key = keys[i * stride];

    self->keys[i] = key;

    d_u64_insert(&self->properties, key, i, key);
  }

  gab_engine_add_constant(eng, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_shape *gab_obj_shape_extend(gab_obj_shape *self, gab_engine *eng,
                                    gab_value property) {
  gab_value keys[self->properties.size + 1];

  for (u64 i = 0; i < self->properties.size; i++) {
    keys[i] = self->keys[i];
  }

  keys[self->properties.size] = property;

  gab_obj_shape *new_shape =
      gab_obj_shape_create(eng, keys, self->properties.size + 1, 1);

  return new_shape;
}

gab_obj_object *gab_obj_object_create(gab_engine *eng, gab_obj_shape *shape,
                                      gab_value values[], u64 size,
                                      u64 stride) {

  gab_obj_object *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_object, gab_value, size, eng, OBJECT_OBJECT);

  self->shape = shape;
  self->static_size = size;

  self->is_dynamic = false;
  self->dynamic_values.size = 0;
  self->dynamic_values.capacity = 0;
  self->dynamic_values.data = NULL;

  for (u64 i = 0; i < size; i++) {
    self->static_values[i] = values[i * stride];
  }

  return self;
}

i16 gab_obj_object_extend(gab_obj_object *self, gab_engine *eng,
                          gab_obj_shape *new_shape, gab_value value) {

  self->shape = new_shape;

  if (!self->is_dynamic) {
    self->is_dynamic = true;

    v_u64_create(&self->dynamic_values);
    for (u8 i = 0; i < self->static_size; i++) {
      v_u64_push(&self->dynamic_values, self->static_values[i]);
    }
  }

  return v_u64_push(&self->dynamic_values, value);
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
