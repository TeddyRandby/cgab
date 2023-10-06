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
  ((struct obj_type *)gab_obj_alloc(gab, NULL, sizeof(struct obj_type)))

#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                \
  ((struct obj_type *)gab_obj_alloc(                                           \
      gab, NULL, sizeof(struct obj_type) + sizeof(flex_type) * flex_count))

#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((struct obj_type *)gab_obj_create(                                          \
      gab, (struct gab_obj *)GAB_CREATE_STRUCT(obj_type), kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((struct obj_type *)gab_obj_create(gab,                                      \
                                     (struct gab_obj *)GAB_CREATE_FLEX_STRUCT( \
                                         obj_type, flex_type, flex_count),     \
                                     kind))

struct gab_obj *gab_obj_create(struct gab_eg *gab, struct gab_obj *self,
                               enum gab_kind k) {
  self->kind = k;
  self->references = 1;
  self->flags = 0;

  return self;
}

int32_t __dump_value(FILE *stream, gab_value self, uint8_t depth);

int32_t shape_dump_properties(FILE *stream, struct gab_obj_shape *shape,
                              uint8_t depth) {
  if (shape->len == 0)
    return 0;

  if (shape->len > 8)
    return fprintf(stream, "...");

  int32_t bytes = 0;

  for (uint64_t i = 0; i < shape->len - 1; i++) {
    bytes += __dump_value(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, " ");
  }

  bytes += __dump_value(stream, shape->data[shape->len - 1], depth - 1);

  return bytes;
}

int32_t rec_dump_properties(FILE *stream, struct gab_obj_record *rec,
                            uint8_t depth) {
  if (rec->len == 0)
    return fprintf(stream, "");

  if (rec->len > 8)
    return fprintf(stream, "...");

  int32_t bytes = 0;

  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(rec->shape);
  for (uint64_t i = 0; i < rec->len - 1; i++) {
    bytes += __dump_value(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, " = ");

    if (rec->data[i] == __gab_obj(rec))
      bytes += fprintf(stream, "{ ... }");
    else
      bytes += __dump_value(stream, rec->data[i], depth - 1);

    bytes += fprintf(stream, " ");
  }

  bytes += __dump_value(stream, shape->data[rec->len - 1], depth - 1);

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
    struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(self);
    return fprintf(stream, "{") + shape_dump_properties(stream, shape, depth) +
           fprintf(stream, "}");
  }
  case kGAB_RECORD: {
    struct gab_obj_record *rec = GAB_VAL_TO_RECORD(self);
    return fprintf(stream, "{") + rec_dump_properties(stream, rec, depth) +
           fprintf(stream, "}");
  }
  case kGAB_STRING: {
    struct gab_obj_string *str = GAB_VAL_TO_STRING(self);
    return fprintf(stream, "%.*s", (int32_t)str->len, (const char *)str->data);
  }
  case kGAB_MESSAGE: {
    struct gab_obj_message *msg = GAB_VAL_TO_MESSAGE(self);
    return fprintf(stream, "&%V", msg->name);
  }
  case kGAB_BOX: {
    struct gab_obj_box *con = GAB_VAL_TO_BOX(self);
    return fprintf(stream, "<%V %p>", con->type, con->data);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    return fprintf(stream, "<block %V>", blk->p);
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = GAB_VAL_TO_SUSPENSE(self);
    return fprintf(stream, "<suspense %V>", sus->b);
  }
  case kGAB_BUILTIN: {
    struct gab_obj_builtin *blt = GAB_VAL_TO_BUILTIN(self);
    return fprintf(stream, "<builtin %V>", blt->name);
  }
  case kGAB_BLOCK_PROTO: {
    struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(self);
    return fprintf(stream, "%V", proto->mod->name);
  }
  case kGAB_SUSPENSE_PROTO: {
    return fprintf(stream, "<prototype>");
  }
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

void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *self) {
  switch (self->kind) {
  case kGAB_MESSAGE: {
    struct gab_obj_message *function = (struct gab_obj_message *)self;
    d_specs_destroy(&function->specs);
    break;
  }
  case kGAB_BOX: {
    struct gab_obj_box *container = (struct gab_obj_box *)self;
    if (container->do_destroy)
      container->do_destroy(container->data);
    break;
  }
  default:
    break;
  }

  gab_obj_alloc(gab, self, 0);
}

uint64_t gab_obj_size(struct gab_obj *self) {
  switch (self->kind) {
  case kGAB_MESSAGE:
    return sizeof(struct gab_obj_message);
  case kGAB_BUILTIN:
    return sizeof(struct gab_obj_builtin);
  case kGAB_BOX:
    return sizeof(struct gab_obj_box);
  case kGAB_SUSPENSE_PROTO:
    return sizeof(struct gab_obj_suspense_proto);
  case kGAB_BLOCK_PROTO: {
    struct gab_obj_block_proto *obj = (struct gab_obj_block_proto *)self;
    return sizeof(struct gab_obj_block_proto) + obj->nupvalues * 2;
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *obj = (struct gab_obj_block *)self;
    return sizeof(struct gab_obj_block) + obj->nupvalues * sizeof(gab_value);
  }
  case kGAB_RECORD: {
    struct gab_obj_record *obj = (struct gab_obj_record *)self;
    return sizeof(struct gab_obj_record) + obj->len * sizeof(gab_value);
  }
  case kGAB_SHAPE: {
    struct gab_obj_shape *obj = (struct gab_obj_shape *)self;
    return sizeof(struct gab_obj_shape) + obj->len * sizeof(gab_value);
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *obj = (struct gab_obj_suspense *)self;
    return sizeof(struct gab_obj_suspense) + obj->len * sizeof(gab_value);
  }
  case kGAB_STRING: {
    struct gab_obj_string *obj = (struct gab_obj_string *)self;
    return sizeof(struct gab_obj_string) + obj->len * sizeof(char);
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

gab_value gab_nstring(struct gab_eg *gab, size_t len,
                      const char data[static len]) {
  s_char str = s_char_create(data, len);

  uint64_t hash = s_char_hash(str, gab->hash_seed);

  struct gab_obj_string *interned = gab_eg_find_string(gab, str, hash);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, char, str.len, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  d_strings_insert(&gab->interned_strings, self, 0);

  return __gab_obj(self);
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_value gab_strcat(struct gab_eg *gab, gab_value _a, gab_value _b) {
  assert(gab_valknd(_a) == kGAB_STRING);
  assert(gab_valknd(_b) == kGAB_STRING);

  struct gab_obj_string *a = GAB_VAL_TO_STRING(_a);
  struct gab_obj_string *b = GAB_VAL_TO_STRING(_b);
  if (a->len == 0)
    return _b;

  if (b->len == 0)
    return _a;

  size_t len = a->len + b->len;

  char data[len];

  // Copy the data into the string obj.
  memcpy(data, a->data, a->len);

  memcpy(data + a->len, b->data, b->len);

  // Pre compute the hash
  s_char ref = s_char_create(data, len);
  size_t hash = s_char_hash(ref, gab->hash_seed);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  struct gab_obj_string *interned = gab_eg_find_string(gab, ref, hash);

  if (interned)
    return __gab_obj(interned);

  return gab_nstring(gab, len, data);
};

gab_value gab_blkproto(struct gab_eg *gab, struct gab_mod *mod,
                       uint8_t narguments, uint8_t nslots, uint8_t nlocals,
                       uint8_t nupvalues, uint8_t flags[nupvalues],
                       uint8_t indexes[nupvalues]) {
  struct gab_obj_block_proto *self = GAB_CREATE_FLEX_OBJ(
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

  GAB_OBJ_GREEN((struct gab_obj *)self);
  return __gab_obj(self);
}

gab_value gab_message(struct gab_eg *gab, gab_value name) {
  uint64_t hash = GAB_VAL_TO_STRING(name)->hash;

  struct gab_obj_message *interned = gab_eg_find_message(gab, name, hash);

  if (interned != NULL)
    return __gab_obj(interned);

  struct gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, kGAB_MESSAGE);

  self->name = name;
  self->hash = hash;
  self->version = 0;

  self->specs = (d_specs){0};

  GAB_OBJ_GREEN((struct gab_obj *)self);

  d_messages_insert(&gab->interned_messages, self, 0);

  return __gab_obj(self);
}

gab_value gab_builtin(struct gab_eg *gab, gab_value name, gab_builtin_f f) {
  assert(gab_valknd(name) == kGAB_STRING);

  struct gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, kGAB_BUILTIN);

  self->name = name;
  self->function = f;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((struct gab_obj *)self);
  return __gab_obj(self);
}

gab_value gab_sbuiltin(struct gab_eg *gab, const char *name, gab_builtin_f f) {
  return gab_builtin(gab, gab_string(gab, name), f);
}

gab_value gab_block(struct gab_eg *gab, gab_value prototype) {
  assert(gab_valknd(prototype) == kGAB_BLOCK_PROTO);
  struct gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(prototype);

  struct gab_obj_block *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value, p->nupvalues, kGAB_BLOCK);

  self->p = prototype;
  self->nupvalues = p->nupvalues;

  for (uint8_t i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = gab_nil;
  }

  return __gab_obj(self);
}

gab_value gab_nshape(struct gab_eg *gab, size_t len) {
  gab_value keys[len];

  for (size_t i = 0; i < len; i++) {
    keys[i] = gab_number(i);
  }

  return gab_shape(gab, NULL, 1, len, keys);
}

gab_value gab_shape(struct gab_eg *gab, bool *internedOut, size_t stride,
                    size_t len, gab_value keys[len]) {
  uint64_t hash = hash_keys(gab->hash_seed, len, stride, keys);

  struct gab_obj_shape *interned =
      gab_eg_find_shape(gab, len, stride, hash, keys);

  if (internedOut)
    *internedOut = interned != NULL;

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->hash = hash;
  self->len = len;

  for (uint64_t i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  GAB_OBJ_GREEN((struct gab_obj *)self);

  d_shapes_insert(&gab->interned_shapes, self, 0);

  return __gab_obj(self);
}

gab_value gab_recordof(struct gab_eg *gab, gab_value shp, uint64_t stride,
                       gab_value values[static GAB_VAL_TO_SHAPE(shp)->len]) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(shp);

  struct gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shp;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = values[i * stride];

  return __gab_obj(self);
}

gab_value gab_erecordof(struct gab_eg *gab, gab_value shp) {
  assert(gab_valknd(shp) == kGAB_SHAPE);
  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(shp);

  struct gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shp;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = gab_nil;

  return __gab_obj(self);
}

gab_value gab_box(struct gab_eg *gab, struct gab_box_argt args) {
  struct gab_obj_box *self = GAB_CREATE_OBJ(gab_obj_box, kGAB_BOX);

  self->do_destroy = args.destructor;
  self->do_visit = args.visitor;
  self->data = args.data;
  self->type = args.type;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  return __gab_obj(self);
}

gab_value gab_susproto(struct gab_eg *gab, uint64_t offset, uint8_t want) {
  struct gab_obj_suspense_proto *self =
      GAB_CREATE_OBJ(gab_obj_suspense_proto, kGAB_SUSPENSE_PROTO);

  self->offset = offset;
  self->want = want;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  return __gab_obj(self);
}

gab_value gab_suspense(struct gab_eg *gab, uint16_t len, gab_value b,
                       gab_value p, gab_value frame[static len]) {
  assert(gab_valknd(b) == kGAB_BLOCK);
  assert(gab_valknd(p) == kGAB_SUSPENSE_PROTO);

  struct gab_obj_suspense *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_suspense, gab_value, len, kGAB_SUSPENSE);

  self->p = p;
  self->b = b;
  self->len = len;

  memcpy(self->frame, frame, len * sizeof(gab_value));

  return __gab_obj(self);
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
