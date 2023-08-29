#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/module.h"
#include "include/types.h"
#include "include/vm.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

gab_obj *gab_obj_create(gab_eg *gab, gab_obj *self, gab_kind k) {
  self->kind = k;
  self->references = 1;
  self->flags = 0;

  return self;
}

int32_t __dump_value(FILE *stream, gab_value self, uint8_t depth);

int32_t shape_dump_properties(FILE *stream, gab_obj_shape *shape, uint8_t depth) {
  if (shape->len == 0)
    return fprintf(stream, "");

  if (shape->len > 8)
    return fprintf(stream, "...");

  int32_t bytes = 0;

  for (uint64_t i = 0; i < shape->len - 1; i++) {
    bytes += __dump_value(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, ", ");
  }

  bytes += __dump_value(stream, shape->data[shape->len - 1], depth - 1);

  return bytes;
}

int32_t rec_dump_properties(FILE *stream, gab_obj_record *rec, uint8_t depth) {
  if (rec->len == 0)
    return fprintf(stream, "");

  if (rec->len > 8)
    return fprintf(stream, "...");

  int32_t bytes = 0;

  for (uint64_t i = 0; i < rec->len - 1; i++) {
    bytes += __dump_value(stream, rec->shape->data[i], depth - 1);

    bytes += fprintf(stream, " = ");

    if (rec->data[i] == __gab_obj(rec))
      bytes += fprintf(stream, "{ ... }");
    else
      bytes += __dump_value(stream, rec->data[i], depth - 1);

    bytes += fprintf(stream, ", ");
  }

  bytes += __dump_value(stream, rec->shape->data[rec->len - 1], depth - 1);

  bytes += fprintf(stream, " = ");

  if (rec->data[rec->len - 1] == __gab_obj(rec))
    bytes += fprintf(stream, "{ ... }");
  else
    bytes += __dump_value(stream, rec->data[rec->len - 1], depth - 1);

  return bytes;
}

int32_t __dump_value(FILE *stream, gab_value self, uint8_t depth) {
  switch (gab_valknd(self)) {
  case kGAB_TRUE:
    return fprintf(stream, "%s", "true");
  case kGAB_FALSE:
    return fprintf(stream, "%s", "false");
  case kGAB_NIL:
    return fprintf(stream, "%s", "nil");
  case kGAB_PRIMITIVE:
    return fprintf(stream, "%s", gab_opcode_names[gab_valtop(self)]);
  case kGAB_NUMBER:
    return fprintf(stream, "%g", gab_valton(self));
  case kGAB_UNDEFINED:
    return fprintf(stream, "%s", "undefined");
  case kGAB_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(self);
    return fprintf(stream, "{") + shape_dump_properties(stream, shape, depth) +
           fprintf(stream, "}");
  }
  case kGAB_RECORD: {
    gab_obj_record *rec = GAB_VAL_TO_RECORD(self);
    return fprintf(stream, "{") + rec_dump_properties(stream, rec, depth) +
           fprintf(stream, "}");
  }
  case kGAB_STRING: {
    gab_obj_string *str = GAB_VAL_TO_STRING(self);
    return fprintf(stream, "%.*s", (int32_t)str->len, (const char *)str->data);
  }
  case kGAB_MESSAGE: {
    gab_obj_message *msg = GAB_VAL_TO_MESSAGE(self);
    return fprintf(stream, "&%V", msg->name);
  }
  case kGAB_BOX: {
    gab_obj_box *con = GAB_VAL_TO_BOX(self);
    return fprintf(stream, "<%V %p>", con->type, con->data);
  }
  case kGAB_BLOCK: {
    gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    gab_value name = blk->p->mod->name;
    return fprintf(stream, "<block %V>", name);
  }
  case kGAB_SUSPENSE: {
    gab_obj_suspense *sus = GAB_VAL_TO_SUSPENSE(self);
    gab_value name = sus->b->p->mod->name;
    return fprintf(stream, "<suspense %V>", name);
  }
  case kGAB_BUILTIN: {
    gab_obj_builtin *blt = GAB_VAL_TO_BUILTIN(self);
    return fprintf(stream, "<builtin %V>", blt->name);
  }
  case kGAB_BLOCK_PROTO:
  case kGAB_SUSPENSE_PROTO:
    return fprintf(stream, "<prototype>");
  default: {
    fprintf(stderr, "%d is not an object.\n", gab_valtoo(self)->kind);
    exit(0);
  }
  }

  assert(false && "Unknown value type.");
}

int32_t gab_fdump(FILE *stream, gab_value self) {
  return __dump_value(stream, self, 2);
}

bool gab_valtob(gab_value self) {
  return !(self == gab_false || self == gab_nil);
}

gab_value gab_valtos(gab_eg *gab, gab_value self) {
  switch (gab_valknd(self)) {
  case kGAB_STRING:
    return self;
  case kGAB_BLOCK:
    return gab_string(gab, "<block>");
  case kGAB_RECORD:
    return gab_string(gab, "<record>");
  case kGAB_SHAPE:
    return gab_string(gab, "<shape>");
  case kGAB_MESSAGE:
    return gab_string(gab, "<message>");
  case kGAB_BLOCK_PROTO:
    return gab_string(gab, "<prototype>");
  case kGAB_BUILTIN:
    return gab_string(gab, "<builtin>");
  case kGAB_BOX:
    return gab_string(gab, "<container>");
  case kGAB_SUSPENSE:
    return gab_string(gab, "<suspense>");
  case kGAB_TRUE:
    return gab_string(gab, "true");
  case kGAB_FALSE:
    return gab_string(gab, "false");
  case kGAB_NIL:
    return gab_string(gab, "nil");
  case kGAB_UNDEFINED:
    return gab_string(gab, "undefined");
  case kGAB_PRIMITIVE:
    return gab_string(gab, gab_opcode_names[gab_valtop(self)]);
  case kGAB_NUMBER: {
    char str[24];
    snprintf(str, 24, "%g", gab_valton(self));
    return gab_string(gab, str);
  }
  default:
    assert(false && "Unhandled type in gab_valtos");
    return gab_undefined;
  }
}

char *gab_valtocs(gab_eg *gab, gab_value value) {
  gab_value str = gab_valtos(gab, value);

  if (gab_valknd(str) == kGAB_STRING) {
    gab_obj_string *string = GAB_VAL_TO_STRING(str);
    return strndup((char *)string->data, string->len);
  }

  assert(false && "Unhandled type in gab_valtosref");
  return "";
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
  case kGAB_BOX: {
    gab_obj_box *container = (gab_obj_box *)self;
    if (container->do_destroy)
      container->do_destroy(container->data);
    return;
  }
  default:
    return;
  }
}

uint64_t gab_obj_size(gab_obj *self) {
  switch (self->kind) {
  case kGAB_MESSAGE:
    return sizeof(gab_obj_message);
  case kGAB_BUILTIN:
    return sizeof(gab_obj_builtin);
  case kGAB_BOX:
    return sizeof(gab_obj_box);
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
    return sizeof(gab_obj_string) + obj->len * sizeof(int8_t);
  }
  default:
    return sizeof(gab_value);
  }
}

static inline uint64_t hash_keys(uint64_t seed, uint64_t len, uint64_t stride,
                            gab_value values[len]) {
  gab_value words[len];
  for (uint64_t i = 0; i < len; i++) {
    words[i] = values[i * stride];
  }
  return hash_words(seed, len, words);
};

gab_obj_string *gab_obj_string_create(gab_eg *gab, s_int8_t str) {
  uint64_t hash = s_int8_t_hash(str, gab->hash_seed);

  gab_obj_string *interned = gab_eg_find_string(gab, str, hash);

  if (interned != NULL)
    return interned;

  gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, uint8_t, str.len, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;

  GAB_OBJ_GREEN((gab_obj *)self);

  d_strings_insert(&gab->interned_strings, self, 0);

  return self;
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_obj_string *gab_obj_string_concat(gab_eg *gab, gab_obj_string *a,
                                      gab_obj_string *b) {
  if (a->len == 0)
    return b;

  if (b->len == 0)
    return a;

  uint64_t len = a->len + b->len;

  int8_t data[len];

  // Copy the data into the string obj.
  memcpy(data, a->data, a->len);

  memcpy(data + a->len, b->data, b->len);

  // Pre compute the hash
  s_int8_t ref = s_int8_t_create(data, len);
  uint64_t hash = s_int8_t_hash(ref, gab->hash_seed);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  gab_obj_string *interned = gab_eg_find_string(gab, ref, hash);

  if (interned)
    return interned;

  gab_obj_string *self = gab_obj_string_create(gab, ref);

  // The module contains a reference to the string, so add a reference.
  return self;
};

s_int8_t gab_obj_string_ref(gab_obj_string *self) {
  s_int8_t ref = {.data = self->data, .len = self->len};
  return ref;
}

gab_obj_block_proto *gab_obj_prototype_create(gab_eg *gab, gab_mod *mod,
                                              uint8_t narguments, uint8_t nslots,
                                              uint8_t nlocals, uint8_t nupvalues,
                                              uint8_t flags[nupvalues],
                                              uint8_t indexes[nupvalues]) {
  gab_obj_block_proto *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_block_proto, uint8_t, nupvalues * 2, kGAB_BLOCK_PROTO);

  self->mod = mod;
  self->narguments = narguments;
  self->nslots = nslots;
  self->nlocals = nlocals;
  self->nupvalues = nupvalues;

  for (uint8_t i = 0; i < nupvalues; i++) {
    self->upv_desc[i * 2] = flags[i];
    self->upv_desc[i * 2 + 1] = indexes[i];
  }

  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_message *gab_obj_message_create(gab_eg *gab, gab_value name) {
  uint64_t hash = GAB_VAL_TO_STRING(name)->hash;
  gab_obj_message *interned = gab_eg_find_message(gab, name, hash);

  if (interned != NULL) {
    return interned;
  }

  gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, kGAB_MESSAGE);

  self->name = name;
  self->hash = hash;
  self->version = 0;

  d_specs_create(&self->specs, 8);

  GAB_OBJ_GREEN((gab_obj *)self);

  d_messages_insert(&gab->interned_messages, self, 0);

  return self;
}

gab_obj_builtin *gab_obj_builtin_create(gab_eg *gab, gab_builtin_f function,
                                        gab_value name) {

  gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, kGAB_BUILTIN);

  self->function = function;
  self->name = name;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((gab_obj *)self);
  return self;
}

gab_obj_block *gab_obj_block_create(gab_eg *gab, gab_obj_block_proto *p) {
  gab_obj_block *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value, p->nupvalues, kGAB_BLOCK);

  self->nupvalues = p->nupvalues;
  self->p = p;

  for (uint8_t i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = gab_nil;
  }

  return self;
}

gab_obj_shape *gab_obj_shape_create_tuple(gab_eg *gab, gab_vm *vm, uint64_t len) {
  gab_value keys[len];

  for (uint64_t i = 0; i < len; i++) {
    keys[i] = gab_number(i);
  }

  return gab_obj_shape_create(gab, vm, len, 1, keys);
}

gab_obj_shape *gab_obj_shape_create(gab_eg *gab, gab_vm *vm, uint64_t len,
                                    uint64_t stride, gab_value keys[len]) {
  uint64_t hash = hash_keys(gab->hash_seed, len, stride, keys);

  gab_obj_shape *interned = gab_eg_find_shape(gab, len, stride, hash, keys);

  if (interned)
    return interned;

  gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->hash = hash;
  self->len = len;

  for (uint64_t i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  if (vm)
    gab_ngciref(&vm->gc, vm, len, self->data);

  GAB_OBJ_GREEN((gab_obj *)self);

  d_shapes_insert(&gab->interned_shapes, self, 0);

  return self;
}

gab_obj_record *gab_obj_record_create(gab_eg *gab, gab_vm *vm,
                                      gab_obj_shape *shape, uint64_t stride,
                                      gab_value values[static shape->len]) {

  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = values[i * stride];

  if (vm)
    gab_ngciref(&vm->gc, vm, self->len, self->data);

  return self;
}

gab_obj_record *gab_obj_record_create_empty(gab_eg *gab, gab_obj_shape *shape) {
  gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shape;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = gab_nil;

  return self;
}

gab_obj_box *gab_obj_box_create(gab_eg *gab, gab_vm *vm, gab_value type,
                                gab_boxdestroy_f destructor,
                                gab_boxvisit_f visitor, void *data) {

  gab_obj_box *self = GAB_CREATE_OBJ(gab_obj_box, kGAB_BOX);

  if (vm)
    gab_gciref(&vm->gc, vm, type);

  self->do_destroy = destructor;
  self->do_visit = visitor;
  self->data = data;
  self->type = type;

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_value gab_box(gab_eg *gab, gab_vm *vm, struct gab_box_argt args) {
  return __gab_obj(gab_obj_box_create(gab, vm, args.type, args.destructor,
                                      args.visitor, args.data));
}

gab_obj_suspense_proto *gab_obj_suspense_proto_create(gab_eg *gab, uint64_t offset,
                                                      uint8_t want) {
  gab_obj_suspense_proto *self =
      GAB_CREATE_OBJ(gab_obj_suspense_proto, kGAB_SUSPENSE_PROTO);

  self->offset = offset;
  self->want = want;

  GAB_OBJ_GREEN((gab_obj *)self);

  return self;
}

gab_obj_suspense *gab_obj_suspense_create(gab_eg *gab, gab_vm *vm, uint16_t len,
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
    gab_ngciref(&vm->gc, vm, len, frame);

  return self;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
