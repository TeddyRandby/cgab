#include "include/colors.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/types.h"
#include "include/vm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GAB_CREATE_ARRAY(type, count)                                          \
  ((type *)gab_memalloc(gab, NULL, sizeof(type) * count))

#define GAB_CREATE_STRUCT(obj_type)                                            \
  ((struct obj_type *)gab_memalloc(gab, NULL, sizeof(struct obj_type)))

#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                \
  ((struct obj_type *)gab_memalloc(                                            \
      gab, NULL, sizeof(struct obj_type) + sizeof(flex_type) * flex_count))

#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((struct obj_type *)gab_obj_create(                                          \
      gab, (struct gab_obj *)GAB_CREATE_STRUCT(obj_type), kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((struct obj_type *)gab_obj_create(gab,                                      \
                                     (struct gab_obj *)GAB_CREATE_FLEX_STRUCT( \
                                         obj_type, flex_type, flex_count),     \
                                     kind))

static const char *gab_kind_names[] = {
    [kGAB_TRUE] = "true",
    [kGAB_FALSE] = "false",
    [kGAB_NIL] = "nil",
    [kGAB_PRIMITIVE] = "primitive",
    [kGAB_NUMBER] = "number",
    [kGAB_UNDEFINED] = "undefined",
    [kGAB_STRING] = "obj_string",
    [kGAB_MESSAGE] = "obj_message",
    [kGAB_SHAPE] = "obj_shape",
    [kGAB_RECORD] = "obj_record",
    [kGAB_BOX] = "obj_box",
    [kGAB_BLOCK] = "obj_block",
    [kGAB_SUSPENSE] = "obj_suspense",
    [kGAB_BUILTIN] = "obj_builtin",
    [kGAB_BLOCK_PROTO] = "obj_block_proto",
    [kGAB_SUSPENSE_PROTO] = "obj_suspense_proto",
};

struct gab_obj *gab_obj_create(struct gab_triple gab, struct gab_obj *self,
                               enum gab_kind k) {
  self->kind = k;
  self->references = 1;
  self->flags = fGAB_OBJ_NEW;

#if cGAB_LOG_GC
  printf("CREATE\t%p\t%s\n", self, gab_kind_names[k]);
#endif

  return self;
}

int32_t __dump_value(FILE *stream, gab_value self, uint8_t depth);

int32_t shape_dump_properties(FILE *stream, struct gab_obj_shape *shape,
                              uint8_t depth) {
  if (shape->len == 0)
    return fprintf(stream, "~");

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
    return fprintf(stream, " ~ ");

  if (rec->len > 8)
    return fprintf(stream, " ... ");

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

int __dump_value(FILE *stream, gab_value self, uint8_t depth) {
  switch (gab_valkind(self)) {
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
  case kGAB_STRING: {
    struct gab_obj_string *str = GAB_VAL_TO_STRING(self);
    return fprintf(stream, "%.*s", (int32_t)str->len, (const char *)str->data);
  }
  case kGAB_MESSAGE: {
    struct gab_obj_message *msg = GAB_VAL_TO_MESSAGE(self);
    return fprintf(stream, "&:%V", msg->name);
  }
  case kGAB_SHAPE: {
    struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(self);
    return fprintf(stream, "<Shape ") +
           shape_dump_properties(stream, shape, depth) + fprintf(stream, ">");
  }
  case kGAB_RECORD: {
    struct gab_obj_record *rec = GAB_VAL_TO_RECORD(self);
    return fprintf(stream, "{") + rec_dump_properties(stream, rec, depth) +
           fprintf(stream, "}");
  }
  case kGAB_BOX: {
    struct gab_obj_box *con = GAB_VAL_TO_BOX(self);
    return fprintf(stream, "<%V %p>", con->type, con->data);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    return fprintf(stream, "<Block %V>", blk->p);
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = GAB_VAL_TO_SUSPENSE(self);
    return fprintf(stream, "<Suspense %V>", sus->b);
  }
  case kGAB_BUILTIN: {
    struct gab_obj_builtin *blt = GAB_VAL_TO_BUILTIN(self);
    return fprintf(stream, "<Builtin %V>", blt->name);
  }
  case kGAB_BLOCK_PROTO: {
    struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(self);
    return fprintf(stream, "%V", proto->name);
  }
  case kGAB_SUSPENSE_PROTO: {
    struct gab_obj_suspense_proto *proto = GAB_VAL_TO_SUSPENSE_PROTO(self);
    return fprintf(stream, "%V", proto->name);
  }
  default: {
    fprintf(stderr, "%d is not an object.\n", gab_valtoo(self)->kind);
    exit(0);
  }
  }

  assert(false && "Unknown value type.");
}

int gab_fvaldump(FILE *stream, gab_value self) {
  return __dump_value(stream, self, 2);
}

void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *self) {
  GAB_OBJ_FREED(self);

  switch (self->kind) {
  case kGAB_BOX: {
    struct gab_obj_box *container = (struct gab_obj_box *)self;
    if (container->do_destroy)
      container->do_destroy(container->data);
    break;
  }
  case kGAB_BLOCK_PROTO: {
    struct gab_obj_block_proto *p = (struct gab_obj_block_proto *)self;
    v_uint8_t_destroy(&p->bytecode);
    v_gab_value_destroy(&p->constants);
    v_uint64_t_destroy(&p->bytecode_toks);
    break;
  }
  case kGAB_SHAPE:
    d_shapes_remove(&gab->interned_shapes, (struct gab_obj_shape *)self);
    break;
  case kGAB_STRING:
    d_strings_remove(&gab->interned_strings, (struct gab_obj_string *)self);
    break;
  case kGAB_MESSAGE:
    d_messages_remove(&gab->interned_messages, (struct gab_obj_message *)self);
    break;
  default:
    break;
  }
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

gab_value gab_nstring(struct gab_triple gab, size_t len,
                      const char data[static len]) {
  s_char str = s_char_create(data, len);

  uint64_t hash = s_char_hash(str, gab.eg->hash_seed);

  struct gab_obj_string *interned = gab_eg_find_string(gab.eg, str, hash);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, char, str.len, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  d_strings_insert(&gab.eg->interned_strings, self, 0);

  return gab_gcdref(gab, __gab_obj(self));
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_value gab_strcat(struct gab_triple gab, gab_value _a, gab_value _b) {
  assert(gab_valkind(_a) == kGAB_STRING);
  assert(gab_valkind(_b) == kGAB_STRING);

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
  size_t hash = s_char_hash(ref, gab.eg->hash_seed);

  /*
    If this string was interned already, destroy and return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  struct gab_obj_string *interned = gab_eg_find_string(gab.eg, ref, hash);

  if (interned)
    return __gab_obj(interned);

  return gab_nstring(gab, len, data);
};

gab_value gab_blkproto(struct gab_triple gab, struct gab_src *src,
                       gab_value name, struct gab_blkproto_argt args) {
  struct gab_obj_block_proto *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_block_proto, uint8_t, args.nupvalues * 2, kGAB_BLOCK_PROTO);

  self->next = gab.eg->prototypes;
  gab.eg->prototypes = self;

  self->src = src;
  self->name = name;
  self->nslots = args.nslots;
  self->nlocals = args.nlocals;
  self->bytecode = args.bytecode;
  self->constants = args.constants;
  self->nupvalues = args.nupvalues;
  self->narguments = args.narguments;
  self->bytecode_toks = args.bytecode_toks;

  for (uint8_t i = 0; i < args.nupvalues; i++) {
    self->upv_desc[i * 2] = args.flags[i];
    self->upv_desc[i * 2 + 1] = args.indexes[i];
  }

  GAB_OBJ_GREEN((struct gab_obj *)self);
  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_message(struct gab_triple gab, gab_value name) {
  struct gab_obj_message *interned =
      gab_eg_find_message(gab.eg, name, GAB_VAL_TO_STRING(name)->hash);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, kGAB_MESSAGE);

  self->name = name;
  self->version = 0;
  self->hash = GAB_VAL_TO_STRING(name)->hash;

  self->specs = gab_erecordof(gab, gab_nshape(gab, 0));

  d_messages_insert(&gab.eg->interned_messages, self, 0);

  /* The message is owned by the engine. */
  return gab_gciref(gab, gab_gcdref(gab, __gab_obj(self)));
}

gab_value gab_builtin(struct gab_triple gab, gab_value name, gab_builtin_f f) {
  assert(gab_valkind(name) == kGAB_STRING);

  struct gab_obj_builtin *self = GAB_CREATE_OBJ(gab_obj_builtin, kGAB_BUILTIN);

  self->name = name;
  self->function = f;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((struct gab_obj *)self);
  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_sbuiltin(struct gab_triple gab, const char *name,
                       gab_builtin_f f) {
  return gab_builtin(gab, gab_string(gab, name), f);
}

gab_value gab_block(struct gab_triple gab, gab_value prototype) {
  assert(gab_valkind(prototype) == kGAB_BLOCK_PROTO);
  struct gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(prototype);

  struct gab_obj_block *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value, p->nupvalues, kGAB_BLOCK);

  self->p = prototype;
  self->nupvalues = p->nupvalues;

  for (uint8_t i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = gab_nil;
  }

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_nshape(struct gab_triple gab, size_t len) {
  gab_value keys[len];

  for (size_t i = 0; i < len; i++) {
    keys[i] = gab_number(i);
  }

  return gab_shape(gab, 1, len, keys);
}

gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[len]) {
  uint64_t hash = hash_keys(gab.eg->hash_seed, len, stride, keys);

  struct gab_obj_shape *interned =
      gab_eg_find_shape(gab.eg, len, stride, hash, keys);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->hash = hash;
  self->len = len;

  for (uint64_t i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  d_shapes_insert(&gab.eg->interned_shapes, self, 0);

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_recordof(struct gab_triple gab, gab_value shp, uint64_t stride,
                       gab_value values[static GAB_VAL_TO_SHAPE(shp)->len]) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(shp);

  struct gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shp;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = values[i * stride];

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_erecordof(struct gab_triple gab, gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(shp);

  struct gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shp;
  self->len = shape->len;

  for (uint64_t i = 0; i < shape->len; i++)
    self->data[i] = gab_nil;

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_box(struct gab_triple gab, struct gab_box_argt args) {
  struct gab_obj_box *self = GAB_CREATE_OBJ(gab_obj_box, kGAB_BOX);

  self->do_destroy = args.destructor;
  self->do_visit = args.visitor;
  self->data = args.data;
  self->type = args.type;

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_susproto(struct gab_triple gab, struct gab_src *src,
                       gab_value name, uint64_t offset, uint8_t want) {
  struct gab_obj_suspense_proto *self =
      GAB_CREATE_OBJ(gab_obj_suspense_proto, kGAB_SUSPENSE_PROTO);

  self->src = src;
  self->name = name;
  self->offset = offset;
  self->want = want;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  return gab_gcdref(gab, __gab_obj(self));
}

gab_value gab_suspense(struct gab_triple gab, uint16_t len, gab_value b,
                       gab_value p, gab_value frame[static len]) {
  assert(gab_valkind(b) == kGAB_BLOCK);
  assert(gab_valkind(p) == kGAB_SUSPENSE_PROTO);

  struct gab_obj_suspense *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_suspense, gab_value, len, kGAB_SUSPENSE);

  self->p = p;
  self->b = b;
  self->len = len;

  memcpy(self->frame, frame, len * sizeof(gab_value));

  return gab_gcdref(gab, __gab_obj(self));
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
