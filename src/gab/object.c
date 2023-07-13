#include "include/object.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/module.h"
#include "include/types.h"
#include "include/value.h"
#include "include/vm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

#define GAB_CREATE_ARRAY(type, count)                                          \
  ((type *)gab_obj_alloc(gab, NULL, sizeof(type) * count))
#define GAB_CREATE_STRUCT(obj_type)                                            \
  ((obj_type *)gab_obj_alloc(gab, NULL, sizeof(obj_type)))
#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                \
  ((obj_type *)gab_obj_alloc(                                                  \
      gab, NULL, sizeof(obj_type) + sizeof(flex_type) * flex_count))

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
  self->references = 1;
  self->flags = 0;

  return self;
}

i32 __dump_value(FILE *stream, gab_value self, u8 depth);

i32 shape_dump_properties(FILE *stream, gab_obj_shape *shape, u8 depth) {
  if (shape->len == 0)
    return 0;

  i32 bytes = 0;

  for (u64 i = 0; i < shape->len - 1; i++) {
    bytes += __dump_value(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, ", ");
  }

  bytes += __dump_value(stream, shape->data[shape->len - 1], depth - 1);

  return bytes;
}

i32 rec_dump_properties(FILE *stream, gab_obj_record *rec, u8 depth) {
  if (rec->len == 0)
    return 0;

  i32 bytes = 0;

  for (u64 i = 0; i < rec->len - 1; i++) {
    bytes += __dump_value(stream, rec->shape->data[i], depth - 1);

    bytes += fprintf(stream, " = ");

    if (rec->data[i] == GAB_VAL_OBJ(rec))
      bytes += fprintf(stream, "{ ... }");
    else
      bytes += __dump_value(stream, rec->data[i], depth - 1);

    bytes += fprintf(stream, ", ");
  }

  bytes += __dump_value(stream, rec->shape->data[rec->len - 1], depth - 1);

  bytes += fprintf(stream, " = ");

  if (rec->data[rec->len - 1] == GAB_VAL_OBJ(rec))
    bytes += fprintf(stream, "{ ... }");
  else
    bytes += __dump_value(stream, rec->data[rec->len - 1], depth - 1);

  return bytes;
}

i32 gab_obj_dump(FILE *stream, gab_value value, u8 depth) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case kGAB_SHAPE: {
    if (depth == 0)
      return fprintf(stream, "{...}");

    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    return fprintf(stream, "{") + shape_dump_properties(stream, shape, depth) +
           fprintf(stream, "}");
  }
  case kGAB_RECORD: {
    if (depth == 0)
      return fprintf(stream, "{...}");

    gab_obj_record *rec = GAB_VAL_TO_RECORD(value);
    return fprintf(stream, "{") + rec_dump_properties(stream, rec, depth) +
           fprintf(stream, "}");
  }
  case kGAB_STRING: {
    gab_obj_string *str = GAB_VAL_TO_STRING(value);
    return fprintf(stream, "%.*s", (i32)str->len, (const char *)str->data);
  }
  case kGAB_MESSAGE: {
    gab_obj_message *msg = GAB_VAL_TO_MESSAGE(value);
    return fprintf(stream, "&:%V", msg->name);
  }
  case kGAB_CONTAINER: {
    gab_obj_container *con = GAB_VAL_TO_CONTAINER(value);
    return fprintf(stream, "<%V %p>", con->type, con->data);
  }
  case kGAB_BLOCK: {
    gab_obj_block *blk = GAB_VAL_TO_BLOCK(value);
    gab_value name =
        v_gab_constant_val_at(&blk->p->mod->constants, blk->p->mod->name);
    return fprintf(stream, "<block %V>", name);
  }
  case kGAB_SUSPENSE: {
    gab_obj_suspense *sus = GAB_VAL_TO_SUSPENSE(value);
    gab_value name =
        v_gab_constant_val_at(&sus->b->p->mod->constants, sus->b->p->mod->name);
    return fprintf(stream, "<suspense %V>", name);
  }
  case kGAB_BUILTIN: {
    gab_obj_builtin *blt = GAB_VAL_TO_BUILTIN(value);
    return fprintf(stream, "<builtin %V>", blt->name);
  }
  case kGAB_BLOCK_PROTO:
  case kGAB_SUSPENSE_PROTO:
    return fprintf(stream, "<prototype>");
  default: {
    fprintf(stderr, "%d is not an object.\n", GAB_VAL_TO_OBJ(value)->kind);
    exit(0);
  }
  }
}

i32 __dump_value(FILE *stream, gab_value self, u8 depth) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    return fprintf(stream, "%s", GAB_VAL_TO_BOOLEAN(self) ? "true" : "false");
  } else if (GAB_VAL_IS_NUMBER(self)) {
    return fprintf(stream, "%g", GAB_VAL_TO_NUMBER(self));
  } else if (GAB_VAL_IS_NIL(self)) {
    return fprintf(stream, "nil");
  } else if (GAB_VAL_IS_OBJ(self)) {
    return gab_obj_dump(stream, self, depth);
  } else if (GAB_VAL_IS_PRIMITIVE(self)) {
    return fprintf(stream, "[primitive:%s]",
                   gab_opcode_names[GAB_VAL_TO_PRIMITIVE(self)]);
  } else if (GAB_VAL_IS_UNDEFINED(self)) {
    return fprintf(stream, "undefined");
  }
}

i32 gab_val_dump(FILE *stream, gab_value self) {
  return __dump_value(stream, self, 2);
}

/*
  Helpers for converting a gab value to a string object.
*/
gab_obj_string *gab_obj_to_obj_string(gab_engine *gab, gab_obj *self) {
  switch (self->kind) {
  case kGAB_STRING:
    return (gab_obj_string *)self;
  case kGAB_BLOCK:
    return gab_obj_string_create(gab, s_i8_cstr("<block>"));
  case kGAB_RECORD:
    return gab_obj_string_create(gab, s_i8_cstr("<record>"));
  case kGAB_SHAPE:
    return gab_obj_string_create(gab, s_i8_cstr("<shape>"));
  case kGAB_MESSAGE:
    return gab_obj_string_create(gab, s_i8_cstr("<message>"));
  case kGAB_BLOCK_PROTO:
    return gab_obj_string_create(gab, s_i8_cstr("<prototype>"));
  case kGAB_BUILTIN:
    return gab_obj_string_create(gab, s_i8_cstr("<builtin>"));
  case kGAB_CONTAINER:
    return gab_obj_string_create(gab, s_i8_cstr("<container>"));
  case kGAB_SUSPENSE:
    return gab_obj_string_create(gab, s_i8_cstr("<suspense>"));
  default: {
    fprintf(stderr, "%d is not an object.\n", self->kind);
    exit(0);
  }
  }
}

gab_value gab_val_to_s(gab_engine *gab, gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self))
    return GAB_VAL_TO_BOOLEAN(self) ? GAB_STRING("true") : GAB_STRING("false");

  if (GAB_VAL_IS_NIL(self))
    return GAB_STRING("nil");

  if (GAB_VAL_IS_UNDEFINED(self))
    return GAB_STRING("undefined");

  if (GAB_VAL_IS_OBJ(self))
    return GAB_VAL_OBJ(gab_obj_to_obj_string(gab, GAB_VAL_TO_OBJ(self)));

  if (GAB_VAL_IS_NUMBER(self)) {
    char str[24];
    snprintf(str, 24, "%g", GAB_VAL_TO_NUMBER(self));
    return GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr(str)));
  }

  printf("Tried to convert unhandled type to string\n");

  return GAB_VAL_NIL();
}

/*
  A generic function used to free a gab object of any kind.
*/
void gab_obj_destroy(gab_obj *self) {
  switch (self->kind) {
  case kGAB_MESSAGE: {
    gab_obj_message *function = (gab_obj_message *)self;
    d_specs_destroy(&function->specs);
    return;
  }
  case kGAB_CONTAINER: {
    gab_obj_container *container = (gab_obj_container *)self;
    if (container->do_destroy)
      container->do_destroy(container->data);
    return;
  }
  default:
    return;
  }
}

u64 gab_obj_size(gab_obj *self) {
  switch (self->kind) {
  case kGAB_MESSAGE:
    return sizeof(gab_obj_message);
  case kGAB_BUILTIN:
    return sizeof(gab_obj_builtin);
  case kGAB_CONTAINER:
    return sizeof(gab_obj_container);
  case kGAB_BLOCK_PROTO: {
    gab_obj_block_proto *obj = (gab_obj_block_proto *)self;
    return sizeof(gab_obj_block_proto) + obj->nupvalues * 2;
  }
  case kGAB_BLOCK: {
    gab_obj_block *obj = (gab_obj_block *)self;
    return sizeof(gab_obj_block) + obj->nupvalues * sizeof(gab_value);
  }
  case kGAB_RECORD: {
    gab_obj_record *obj = (gab_obj_record *)self;
    return sizeof(gab_obj_record) + obj->len * sizeof(gab_value);
  }
  case kGAB_SHAPE: {
    gab_obj_shape *obj = (gab_obj_shape *)self;
    return sizeof(gab_obj_shape) + obj->len * sizeof(gab_value);
  }
  case kGAB_SUSPENSE: {
    gab_obj_suspense *obj = (gab_obj_suspense *)self;
    return sizeof(gab_obj_suspense) + obj->len * sizeof(gab_value);
  }
  case kGAB_STRING: {
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
      GAB_CREATE_FLEX_OBJ(gab_obj_string, u8, str.len, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;

  GAB_OBJ_GREEN((gab_obj *)self);

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

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

  // The module contains a reference to the string, so add a reference.
  return self;
};

s_i8 gab_obj_string_ref(gab_obj_string *self) {
  s_i8 ref = {.data = self->data, .len = self->len};
  return ref;
}

gab_obj_block_proto *gab_obj_prototype_create(gab_engine *gab, gab_module *mod,
                                              u8 narguments, u8 nslots,
                                              u8 nlocals, u8 nupvalues,
                                              boolean var, u8 flags[nupvalues],
                                              u8 indexes[nupvalues]) {
  gab_obj_block_proto *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_block_proto, u8, nupvalues * 2, kGAB_BLOCK_PROTO);

  self->mod = mod;
  self->narguments = narguments;
  self->nslots = nslots;
  self->nlocals = nlocals;
  self->nupvalues = nupvalues;
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

  if (interned != NULL) {
    return interned;
  }

  gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, kGAB_MESSAGE);

  self->name = name;
  self->hash = hash;
  self->version = 0;

  d_specs_create(&self->specs, 8);

  GAB_OBJ_GREEN((gab_obj *)self);

  // Intern the string in the module
  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_engine *gab, gab_builtin function,
                                        gab_value name) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, kGAB_BUILTIN);

  self->function = function;
  self->name = name;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_block *gab_obj_block_create(gab_engine *gab, gab_obj_block_proto *p) {
  gab_obj_block *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value, p->nupvalues, kGAB_BLOCK);

  self->nupvalues = p->nupvalues;
  self->p = p;

  for (u8 i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = GAB_VAL_NIL();
  }

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
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->hash = hash;
  self->len = len;

  for (u64 i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  if (vm)
    gab_gc_iref_many(&vm->gc, vm, len, self->data);

  GAB_OBJ_GREEN((gab_obj *)self);

  gab_engine_intern(gab, GAB_VAL_OBJ(self));

  return self;
}

gab_obj_record *gab_obj_record_create(gab_engine *gab, gab_vm *vm,
                                      gab_obj_shape *shape, u64 stride,
                                      gab_value values[static shape->len]) {

  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (u64 i = 0; i < shape->len; i++)
    self->data[i] = values[i * stride];

  if (vm)
    gab_gc_iref_many(&vm->gc, vm, self->len, self->data);

  return self;
}

gab_obj_record *gab_obj_record_create_empty(gab_engine *gab,
                                            gab_obj_shape *shape) {
  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (u64 i = 0; i < shape->len; i++)
    self->data[i] = GAB_VAL_NIL();

  return self;
}

void gab_obj_record_set(gab_vm *vm, gab_obj_record *obj, u64 offset,
                        gab_value value) {
  assert(offset < obj->len);

  if (vm) {
    gab_gc_dref(&vm->gc, vm, obj->data[offset]);
    gab_gc_iref(&vm->gc, vm, value);
  }

  obj->data[offset] = value;
}

gab_value gab_obj_record_get(gab_obj_record *obj, u64 offset) {
  assert(offset < obj->len);
  return obj->data[offset];
}

boolean gab_obj_record_put(gab_vm *vm, gab_obj_record *obj, gab_value key,
                           gab_value value) {
  u16 prop_offset = gab_obj_shape_find(obj->shape, key);

  if (prop_offset == UINT16_MAX) {
    return false;
  }

  gab_obj_record_set(vm, obj, prop_offset, value);

  return true;
}

gab_value gab_obj_record_at(gab_obj_record *self, gab_value prop) {
  u64 prop_offset = gab_obj_shape_find(self->shape, prop);

  if (prop_offset >= self->len)
    return GAB_VAL_NIL();

  return gab_obj_record_get(self, prop_offset);
}

boolean gab_obj_record_has(gab_obj_record *self, gab_value prop) {
  return !GAB_VAL_IS_NIL(gab_obj_record_at(self, prop));
}

gab_obj_container *
gab_obj_container_create(gab_engine *gab, gab_vm *vm, gab_value type,
                         gab_obj_container_destructor destructor,
                         gab_obj_container_visitor visitor, void *data) {

  gab_obj_container *self = GAB_CREATE_OBJ(gab_obj_container, kGAB_CONTAINER);

  if (vm)
    gab_gc_iref(&vm->gc, vm, type);

  self->do_destroy = destructor;
  self->do_visit = visitor;
  self->data = data;
  self->type = type;

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_suspense_proto *gab_obj_suspense_proto_create(gab_engine *gab,
                                                      u64 offset, u8 want) {
  gab_obj_suspense_proto *self =
      GAB_CREATE_OBJ(gab_obj_suspense_proto, kGAB_SUSPENSE_PROTO);

  self->offset = offset;
  self->want = want;

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_suspense *gab_obj_suspense_create(gab_engine *gab, gab_vm *vm, u16 len,
                                          gab_obj_block *b,
                                          gab_obj_suspense_proto *p,
                                          gab_value frame[static len]) {
  gab_obj_suspense *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_suspense, gab_value, len, kGAB_SUSPENSE);

  self->p = p;
  self->b = b;
  self->len = len;

  memcpy(self->frame, frame, len * sizeof(gab_value));

  if (vm)
    gab_gc_iref_many(&vm->gc, vm, len, frame);

  return self;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
