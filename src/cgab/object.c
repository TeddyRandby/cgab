#include "engine.h"
#include "gab.h"
#include <stddef.h>

#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((struct obj_type *)gab_obj_create(gab, sizeof(struct obj_type), kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((struct obj_type *)gab_obj_create(                                          \
      gab, sizeof(struct obj_type) + sizeof(flex_type) * (flex_count), kind))

struct gab_obj *gab_obj_create(struct gab_triple gab, size_t sz,
                               enum gab_kind k) {
  struct gab_obj *self = gab_egalloc(gab, NULL, sz);

  self->kind = k;
  self->references = 1;
  self->flags = fGAB_OBJ_NEW;

#if cGAB_LOG_GC
  printf("CREATE\t%p\t%lu\t%d\n", (void *)self, sz, k);
#endif

  gab_dref(gab, __gab_obj(self));

  return self;
}

int shape_dump_properties(FILE *stream, struct gab_obj_shape *shape,
                          int depth) {
  if (shape->len == 0)
    return fprintf(stream, "~ ");

  if (shape->len > 8)
    return fprintf(stream, "...");

  int32_t bytes = 0;

  for (uint64_t i = 0; i < shape->len - 1; i++) {
    bytes += gab_fvalinspect(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, " ");
  }

  bytes += gab_fvalinspect(stream, shape->data[shape->len - 1], depth - 1);

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
    bytes += gab_fvalinspect(stream, shape->data[i], depth - 1);

    bytes += fprintf(stream, " = ");

    if (rec->data[i] == __gab_obj(rec))
      bytes += fprintf(stream, "{ ... }");
    else
      bytes += gab_fvalinspect(stream, rec->data[i], depth - 1);

    bytes += fprintf(stream, " ");
  }

  bytes += gab_fvalinspect(stream, shape->data[rec->len - 1], depth - 1);

  bytes += fprintf(stream, " = ");

  if (rec->data[rec->len - 1] == __gab_obj(rec))
    bytes += fprintf(stream, "{ ... }");
  else
    bytes += gab_fvalinspect(stream, rec->data[rec->len - 1], depth - 1);

  return bytes;
}

int gab_fvalinspect(FILE *stream, gab_value self, int depth) {
  if (depth < 0)
    depth = 1;

  switch (gab_valkind(self)) {
  case kGAB_NIL:
    return fprintf(stream, "%s", "nil");
  case kGAB_TRUE:
    return fprintf(stream, "%s", "true");
  case kGAB_FALSE:
    return fprintf(stream, "%s", "false");
  case kGAB_PRIMITIVE:
    return fprintf(stream, "%s", gab_opcode_names[gab_valtop(self)]);
  case kGAB_NUMBER:
    return fprintf(stream, "%g", gab_valton(self));
  case kGAB_UNDEFINED:
    return fprintf(stream, "%s", "undefined");
  case kGAB_STRING:
    return fprintf(stream, "%s", gab_strdata(&self));
  case kGAB_MESSAGE: {
    struct gab_obj_message *msg = GAB_VAL_TO_MESSAGE(self);
    return fprintf(stream, "&:") + gab_fvalinspect(stream, msg->name, depth);
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
    return fprintf(stream, "<") + gab_fvalinspect(stream, con->type, depth) +
           fprintf(stream, " %p>", con->data);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);
    return fprintf(stream, "<Block ") +
           gab_fvalinspect(stream, p->name, depth) + fprintf(stream, ">");
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = GAB_VAL_TO_SUSPENSE(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(sus->p);
    return fprintf(stream, "<Suspense ") +
           gab_fvalinspect(stream, p->name, depth) + fprintf(stream, ">");
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(self);
    return fprintf(stream, "<Native ") +
           gab_fvalinspect(stream, n->name, depth) + fprintf(stream, ">");
  }
  case kGAB_BPROTOTYPE:
  case kGAB_SPROTOTYPE: {
    struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(self);
    return fprintf(stream, "<Prototype ") +
           gab_fvalinspect(stream, proto->name, depth) + fprintf(stream, ">");
  }
  default: {
    assert(false && "NOT AN OBJECT");
  }
  }
}

void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *self) {
  GAB_OBJ_FREED(self);

  switch (self->kind) {
  case kGAB_BOX: {
    struct gab_obj_box *box = (struct gab_obj_box *)self;
    if (box->do_destroy)
      box->do_destroy(box->len, box->data);
    break;
  }
  case kGAB_SHAPE:
    d_shapes_remove(&gab->shapes, (struct gab_obj_shape *)self);
    break;
  case kGAB_STRING:
    d_strings_remove(&gab->strings, (struct gab_obj_string *)self);
    break;
  case kGAB_MESSAGE:
    d_messages_remove(&gab->messages, (struct gab_obj_message *)self);
    break;
  default:
    break;
  }
}

static inline uint64_t hash_keys(uint64_t seed, uint64_t len, uint64_t stride,
                                 gab_value values[len]) {
  gab_value words[len];
  memset(words, 0, sizeof(gab_value) * len);

  for (uint64_t i = 0; i < len; i++) {
    words[i] = values[i * stride];
  }

  return hash_words(seed, len, words);
};

gab_value gab_shorstr(size_t len, const char data[static len]) {
  assert(len <= 5);

  gab_value v = 0;
  v |= (__GAB_QNAN | (uint64_t)kGAB_STRING << __GAB_TAGOFFSET |
        (((uint64_t)5 - len) << 40));

  for (size_t i = 0; i < len; i++) {
    v |= (size_t)(0xff & data[i]) << (i * 8);
  }

  return v;
}

gab_value gab_shortstrcat(gab_value _a, gab_value _b) {
  assert(gab_valkind(_a) == kGAB_STRING);
  assert(gab_valkind(_b) == kGAB_STRING);

  size_t alen = gab_strlen(_a);
  size_t blen = gab_strlen(_b);

  assert(alen + blen <= 5);

  uint8_t len = alen + blen;

  gab_value v = 0;
  v |= (__GAB_QNAN | (uint64_t)kGAB_STRING << __GAB_TAGOFFSET |
        (((uint64_t)5 - len) << 40));

  for (size_t i = 0; i < alen; i++) {
    v |= (size_t)(0xff & gab_strdata(&_a)[i]) << (i * 8);
  }

  for (size_t i = 0; i < blen; i++) {
    v |= (size_t)(0xff & gab_strdata(&_b)[i]) << ((i + alen) * 8);
  }

  return v;
}

gab_value gab_nstring(struct gab_triple gab, size_t len,
                      const char data[static len]) {
  if (len <= 5)
    return gab_shorstr(len, data);

  s_char str = s_char_create(data, len);

  uint64_t hash = s_char_hash(str, gab.eg->hash_seed);

  struct gab_obj_string *interned = gab_egstrfind(gab.eg, str, hash);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, char, str.len + 1, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;
  self->data[str.len] = '\0';

  GAB_OBJ_GREEN((struct gab_obj *)self);

  d_strings_insert(&gab.eg->strings, self, 0);

  return __gab_obj(self);
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_value gab_strcat(struct gab_triple gab, gab_value _a, gab_value _b) {
  assert(gab_valkind(_a) == kGAB_STRING);
  assert(gab_valkind(_b) == kGAB_STRING);

  size_t alen = gab_strlen(_a);
  size_t blen = gab_strlen(_b);

  if (alen == 0)
    return _b;

  if (blen == 0)
    return _a;

  size_t len = alen + blen;

  if (len <= 5)
    return gab_shortstrcat(_a, _b);

  a_char *buff = a_char_empty(len + 1);

  // Copy the data into the string obj.
  memcpy(buff->data, gab_strdata(&_a), alen);
  memcpy(buff->data + alen, gab_strdata(&_b), blen);

  // Pre compute the hash
  s_char ref = s_char_create(buff->data, len);
  size_t hash = s_char_hash(ref, gab.eg->hash_seed);

  /*
    If this string was interned already, return.

    Unfortunately, we can't check for this before copying and computing the
    hash.
  */
  struct gab_obj_string *interned = gab_egstrfind(gab.eg, ref, hash);

  if (interned) {
    a_char_destroy(buff);
    return __gab_obj(interned);
  }

  gab_value result = gab_nstring(gab, len, buff->data);
  a_char_destroy(buff);
  return result;
};

gab_value gab_bprototype(struct gab_triple gab, struct gab_src *src,
                         gab_value name, size_t offset, size_t len,
                         struct gab_blkproto_argt args) {

  struct gab_obj_prototype *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_prototype, uint8_t, args.nupvalues * 2, kGAB_BPROTOTYPE);

  self->src = src;
  self->name = name;
  self->offset = offset;
  self->len = args.nupvalues * 2;
  self->as.block.len = len;
  self->as.block.nslots = args.nslots;
  self->as.block.nlocals = args.nlocals;
  self->as.block.nupvalues = args.nupvalues;
  self->as.block.narguments = args.narguments;

  if (args.nupvalues > 0) {
    if (args.data) {
      memcpy(self->data, args.data, args.nupvalues * 2 * sizeof(uint8_t));
    } else if (args.flags && args.indexes) {
      for (uint8_t i = 0; i < args.nupvalues; i++) {
        self->data[i * 2] = args.flags[i];
        self->data[i * 2 + 1] = args.indexes[i];
      }
    } else {
      assert(0 && "Invalid arguments to gab_bprototype");
    }
  }

  GAB_OBJ_GREEN((struct gab_obj *)self);
  return __gab_obj(self);
}

gab_value gab_message(struct gab_triple gab, gab_value name) {
  size_t hash = gab_strhash(name);

  struct gab_obj_message *interned = gab_egmsgfind(gab.eg, name, hash);

  if (interned)
    return __gab_obj(interned);

  gab_gclock(gab.gc);

  struct gab_obj_message *self = GAB_CREATE_OBJ(gab_obj_message, kGAB_MESSAGE);

  self->name = name;
  self->hash = hash;

  self->specs = gab_etuple(gab, 0);

  d_messages_insert(&gab.eg->messages, self, 0);

  gab_gcunlock(gab.gc);

  return __gab_obj(self);
}

gab_value gab_native(struct gab_triple gab, gab_value name, gab_native_f f) {
  assert(gab_valkind(name) == kGAB_STRING);

  struct gab_obj_native *self = GAB_CREATE_OBJ(gab_obj_native, kGAB_NATIVE);

  self->name = name;
  self->function = f;

  // Builtins cannot reference other objects - mark them green.
  GAB_OBJ_GREEN((struct gab_obj *)self);
  return __gab_obj(self);
}

gab_value gab_snative(struct gab_triple gab, const char *name, gab_native_f f) {
  return gab_native(gab, gab_string(gab, name), f);
}

gab_value gab_block(struct gab_triple gab, gab_value prototype) {
  assert(gab_valkind(prototype) == kGAB_BPROTOTYPE);
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(prototype);

  struct gab_obj_block *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_block, gab_value, p->as.block.nupvalues, kGAB_BLOCK);

  self->p = prototype;
  self->nupvalues = p->as.block.nupvalues;

  for (uint8_t i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = gab_undefined;
  }

  return __gab_obj(self);
}

gab_value gab_nshape(struct gab_triple gab, size_t len) {
  gab_value keys[len];

  for (size_t i = 0; i < len; i++) {
    keys[i] = gab_number(i);
  }

  return gab_shape(gab, 1, len, keys);
}

gab_value gab_shapewith(struct gab_triple gab, gab_value shape, gab_value key) {
  assert(gab_valkind(shape) == kGAB_SHAPE);
  struct gab_obj_shape *obj = GAB_VAL_TO_SHAPE(shape);

  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, obj->len + 1, kGAB_SHAPE);

  memcpy(self->data, obj->data, obj->len * sizeof(gab_value));
  self->data[obj->len] = key;
  self->len = obj->len + 1;

  uint64_t hash = hash_keys(gab.eg->hash_seed, self->len, 1, self->data);

  self->hash = hash;

  struct gab_obj_shape *interned =
      gab_egshpfind(gab.eg, self->len, 1, hash, self->data);

  if (interned) {
    // NOTE: We don't free the intermediate self shape, even if we hit the
    // cache. We queued a decrement for it already when it was created, so we
    // can let it clean up that way.
    return __gab_obj(interned);
  }

  d_shapes_insert(&gab.eg->shapes, self, 0);

  return __gab_obj(self);
}

gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[len]) {
  uint64_t hash = hash_keys(gab.eg->hash_seed, len, stride, keys);

  struct gab_obj_shape *interned =
      gab_egshpfind(gab.eg, len, stride, hash, keys);

  if (interned)
    return __gab_obj(interned);

  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->hash = hash;
  self->len = len;

  for (uint64_t i = 0; i < len; i++) {
    self->data[i] = keys[i * stride];
  }

  d_shapes_insert(&gab.eg->shapes, self, 0);

  return __gab_obj(self);
}

gab_value gab_recordwith(struct gab_triple gab, gab_value rec, gab_value key,
                         gab_value value) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_record *obj = GAB_VAL_TO_RECORD(rec);

  gab_gclock(gab.gc);

  gab_value shp = gab_shapewith(gab, obj->shape, key);
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *shape = GAB_VAL_TO_SHAPE(shp);

  struct gab_obj_record *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_record, gab_value, shape->len, kGAB_RECORD);

  self->shape = shp;
  self->len = shape->len;

  memcpy(self->data, obj->data, obj->len * sizeof(gab_value));

  self->data[self->len - 1] = value;

  gab_gcunlock(gab.gc);

  return __gab_obj(self);
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

  return __gab_obj(self);
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

  return __gab_obj(self);
}

gab_value gab_box(struct gab_triple gab, struct gab_box_argt args) {
  struct gab_obj_box *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_box, unsigned char, args.size, kGAB_BOX);

  self->do_destroy = args.destructor;
  self->do_visit = args.visitor;
  self->type = args.type;
  self->len = args.size;

  if (args.data) {
    memcpy(self->data, args.data, args.size);
  } else {
    memset(self->data, 0, args.size);
  }

  return __gab_obj(self);
}

gab_value gab_sprototype(struct gab_triple gab, struct gab_src *src,
                         gab_value name, size_t begin, uint8_t want) {

  struct gab_obj_prototype *self =
      GAB_CREATE_OBJ(gab_obj_prototype, kGAB_SPROTOTYPE);

  self->src = src;
  self->name = name;
  self->offset = begin;
  self->len = 0;
  self->as.suspense.want = want;

  GAB_OBJ_GREEN((struct gab_obj *)self);

  return __gab_obj(self);
}

gab_value gab_suspense(struct gab_triple gab, gab_value b, gab_value p,
                       uint64_t len, gab_value data[static len]) {
  assert(gab_valkind(p) == kGAB_SPROTOTYPE);

  struct gab_obj_suspense *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_suspense, gab_value, len, kGAB_SUSPENSE);

  self->b = b;
  self->p = p;
  self->nslots = len;

  memcpy(self->slots, data, len * sizeof(gab_value));

  return __gab_obj(self);
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
