#include "engine.h"
#include "gab.h"

#define GAB_CREATE_OBJ(obj_type, kind)                                         \
  ((struct obj_type *)gab_obj_create(gab, sizeof(struct obj_type), kind))

#define GAB_CREATE_FLEX_OBJ(obj_type, flex_type, flex_count, kind)             \
  ((struct obj_type *)gab_obj_create(                                          \
      gab, sizeof(struct obj_type) + sizeof(flex_type) * (flex_count), kind))

struct gab_obj *gab_obj_create(struct gab_triple gab, size_t sz,
                               enum gab_kind k) {
  struct gab_obj *self = gab_egalloc(gab, nullptr, sz);

  self->kind = k;
  self->references = 1;
  self->flags = fGAB_OBJ_NEW;

#if cGAB_LOG_GC
  printf("CREATE\t%p\t%lu\t%d\n", (void *)self, sz, k);
#endif

  gab_dref(gab, __gab_obj(self));

  return self;
}

size_t gab_obj_size(struct gab_obj *obj) {
  switch (obj->kind) {
  case kGAB_BOX: {
    struct gab_obj_box *o = (struct gab_obj_box *)obj;
    return sizeof(struct gab_obj_box) + o->len * sizeof(char);
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *o = (struct gab_obj_recnode *)obj;
    size_t len = __builtin_popcountl(o->mask);
    return sizeof(struct gab_obj_recnode) + len * 2 * sizeof(gab_value);
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *o = (struct gab_obj_rec *)obj;
    size_t len = __builtin_popcountl(o->mask);
    return sizeof(struct gab_obj_rec) + len * 2 * sizeof(gab_value);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *o = (struct gab_obj_block *)obj;
    return sizeof(struct gab_obj_block) + o->nupvalues * sizeof(gab_value);
  }
  case kGAB_PROTOTYPE: {
    struct gab_obj_prototype *o = (struct gab_obj_prototype *)obj;
    return sizeof(struct gab_obj_prototype) + o->nupvalues * sizeof(char);
  }
  case kGAB_SHAPE: {
    struct gab_obj_shape *o = (struct gab_obj_shape *)obj;
    return sizeof(struct gab_obj_shape) + o->len * sizeof(gab_value);
  }
  case kGAB_STRING: {
    struct gab_obj_string *o = (struct gab_obj_string *)obj;
    return sizeof(struct gab_obj_string) + (o->len + 1) * sizeof(char);
  }
  case kGAB_NATIVE:
    return sizeof(struct gab_obj_native);
  default:
    assert(false && "unreachable");
  }
}

int shape_dump_keys(FILE *stream, gab_value shape, int depth) {
  struct gab_obj_shape *shp = GAB_VAL_TO_SHAPE(shape);
  size_t len = shp->len;

  if (len == 0)
    return fprintf(stream, "~ ");

  if (len > 8)
    return fprintf(stream, "... ");

  int bytes = 0;

  for (size_t i = 0; i < len; i++) {
    bytes += gab_fvalinspect(stream, shp->keys[i], depth - 1);

    if (i + 1 < len)
      bytes += fprintf(stream, " ");
  }

  return bytes;
}

int map_dump_properties(FILE *stream, gab_value map, int depth) {
  switch (gab_valkind(map)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(map);
    size_t len = __builtin_popcountl(m->mask);

    if (len == 0)
      return fprintf(stream, " ~ ");

    if (len > 8)
      return fprintf(stream, " ... ");

    int32_t bytes = 0;

    for (uint64_t i = 0; i < len; i++) {
      if (m->vmask & ((size_t)1 << i)) {
        bytes += gab_fvalinspect(stream, m->data[i * 2], depth - 1);
        bytes += fprintf(stream, " = ");
        bytes += gab_fvalinspect(stream, m->data[i * 2 + 1], depth - 1);

      } else {
        bytes += gab_fvalinspect(stream, m->data[i * 2], depth - 1);
      }

      if (i + 1 < len)
        bytes += fprintf(stream, ", ");
    }

    return bytes;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(map);
    size_t len = __builtin_popcountl(m->mask);

    if (len == 0)
      return fprintf(stream, "~ ");

    if (len > 8)
      return fprintf(stream, "... ");

    int32_t bytes = 0;

    for (uint64_t i = 0; i < len; i++) {
      if (m->vmask & ((size_t)1 << i)) {
        bytes += gab_fvalinspect(stream, m->data[i * 2], depth - 1);
        bytes += fprintf(stream, " = ");
        bytes += gab_fvalinspect(stream, m->data[i * 2 + 1], depth - 1);

      } else {
        bytes += gab_fvalinspect(stream, m->data[i * 2], depth - 1);
      }

      if (i + 1 < len)
        bytes += fprintf(stream, ", ");
    }

    return bytes;
    break;
  }
  default:
    assert(false && "NOT A REC");
  }
}

int gab_fvalinspect(FILE *stream, gab_value self, int depth) {
  if (depth < 0)
    depth = 1;

  switch (gab_valkind(self)) {
  case kGAB_PRIMITIVE:
    return fprintf(stream, "<gab.primitive %s>",
                   gab_opcode_names[gab_valtop(self)]);
  case kGAB_NUMBER:
    return fprintf(stream, "%g", gab_valton(self));
  case kGAB_SIGIL:
    return fprintf(stream, ".%s", gab_strdata(&self));
  case kGAB_STRING:
    return fprintf(stream, "%s", gab_strdata(&self));
  case kGAB_MESSAGE: {
    return fprintf(stream, "\\%s", gab_strdata(&self));
  }
  case kGAB_SHAPE: {
    return fprintf(stream, "<gab.shape ") +
           shape_dump_keys(stream, self, depth) + fprintf(stream, ">");
  }
  case kGAB_RECORD: {
    return fprintf(stream, "{") + map_dump_properties(stream, self, depth) +
           fprintf(stream, "}");
  }
  case kGAB_RECORDNODE: {
    return map_dump_properties(stream, self, depth);
  }
  case kGAB_BOX: {
    struct gab_obj_box *con = GAB_VAL_TO_BOX(self);
    return fprintf(stream, "<") + gab_fvalinspect(stream, con->type, depth) +
           fprintf(stream, " %p>", con->data);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);
    return fprintf(stream, "<gab.block ") +
           gab_fvalinspect(stream, gab_srcname(p->src), depth) +
           fprintf(stream, ">");
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(self);
    return fprintf(stream, "<gab.native ") +
           gab_fvalinspect(stream, n->name, depth) + fprintf(stream, ">");
  }
  case kGAB_PROTOTYPE: {
    struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(self);
    return fprintf(stream, "<gab.prototype ") +
           gab_fvalinspect(stream, gab_srcname(proto->src), depth) +
           fprintf(stream, ">");
  }
  default: {
    assert(false && "NOT AN OBJECT");
  }
  }
}

void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *self) {
  GAB_OBJ_FREED(self);

  switch (self->kind) {
  case kGAB_SHAPE: {
    struct gab_obj_shape *shp = (struct gab_obj_shape *)self;
    v_gab_value_destroy(&shp->transitions);
    break;
  }
  case kGAB_BOX: {
    struct gab_obj_box *box = (struct gab_obj_box *)self;
    if (box->do_destroy)
      box->do_destroy(box->len, box->data);
    break;
  }
  case kGAB_STRING:
    d_strings_remove(&gab->strings, (struct gab_obj_string *)self);
    break;
  default:
    break;
  }
}

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
  assert(gab_valkind(_a) == kGAB_STRING || gab_valkind(_a) == kGAB_SIGIL);
  assert(gab_valkind(_b) == kGAB_STRING || gab_valkind(_b) == kGAB_SIGIL);

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

  d_strings_insert(&gab.eg->strings, self, 0);

  return __gab_obj(self);
};

/*
  Given two strings, create a third which is the concatenation a+b
*/
gab_value gab_strcat(struct gab_triple gab, gab_value _a, gab_value _b) {
  assert(gab_valkind(_a) == kGAB_STRING || gab_valkind(_a) == kGAB_SIGIL);
  assert(gab_valkind(_b) == kGAB_STRING || gab_valkind(_b) == kGAB_SIGIL);

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

gab_value gab_prototype(struct gab_triple gab, struct gab_src *src,
                        size_t offset, size_t len,
                        struct gab_prototype_argt args) {

  struct gab_obj_prototype *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_prototype, uint8_t, args.nupvalues, kGAB_PROTOTYPE);

  self->src = src;
  self->offset = offset;
  self->len = args.nupvalues;
  self->len = len;
  self->nslots = args.nslots;
  self->nlocals = args.nlocals;
  self->nupvalues = args.nupvalues;
  self->narguments = args.narguments;

  if (args.nupvalues > 0) {
    if (args.data) {
      memcpy(self->data, args.data, args.nupvalues * 2 * sizeof(uint8_t));
    } else if (args.flags && args.indexes) {
      for (uint8_t i = 0; i < args.nupvalues; i++) {
        bool is_local = args.flags[i] & fLOCAL_LOCAL;
        self->data[i] = (args.indexes[i] << 1) | is_local;
      }
    } else {
      assert(0 && "Invalid arguments to gab_bprototype");
    }
  }

  return __gab_obj(self);
}

gab_value gab_native(struct gab_triple gab, gab_value name, gab_native_f f) {
  assert(gab_valkind(name) == kGAB_STRING || gab_valkind(name) == kGAB_SIGIL);

  struct gab_obj_native *self = GAB_CREATE_OBJ(gab_obj_native, kGAB_NATIVE);

  self->name = name;
  self->function = f;

  return __gab_obj(self);
}

gab_value gab_snative(struct gab_triple gab, const char *name, gab_native_f f) {
  return gab_native(gab, gab_string(gab, name), f);
}

gab_value gab_block(struct gab_triple gab, gab_value prototype) {
  assert(gab_valkind(prototype) == kGAB_PROTOTYPE);
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(prototype);

  struct gab_obj_block *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_block, gab_value, p->nupvalues, kGAB_BLOCK);

  self->p = prototype;
  self->nupvalues = p->nupvalues;

  for (uint8_t i = 0; i < self->nupvalues; i++) {
    self->upvalues[i] = gab_undefined;
  }

  return __gab_obj(self);
}

gab_value gab_symbol(struct gab_triple gab) {
  struct gab_obj_box *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_box, unsigned char, 0, kGAB_BOX);

  self->type = __gab_obj(self);
  self->do_destroy = nullptr;
  self->do_visit = nullptr;
  self->len = 0;

  return __gab_obj(self);
}

gab_value gab_box(struct gab_triple gab, struct gab_box_argt args) {
  struct gab_obj_box *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_box, unsigned char, args.size, kGAB_BOX);

  self->do_destroy = args.destructor;
  self->do_visit = args.visitor;
  self->do_copy = args.copier;
  self->type = args.type;
  self->len = args.size;

  if (args.data) {
    memcpy(self->data, args.data, args.size);
  } else {
    memset(self->data, 0, args.size);
  }

  return __gab_obj(self);
}

gab_value gab_record(struct gab_triple gab, size_t stride, size_t len,
                     gab_value keys[static len], gab_value vals[static len]) {
  struct gab_obj_rec *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_rec, gab_value, 0, kGAB_RECORD);

  self->shape = gab_shape(gab, stride, len, keys);
  self->mask = 0;
  self->vmask = 0;

  gab_value res = __gab_obj(self);

  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    res = gab_recput(gab, res, keys[i * stride], vals[i * stride]);
  }

  gab_gcunlock(gab.gc);

  return res;
}

gab_value __gab_record(struct gab_triple gab, size_t len, size_t space,
                       gab_value *data) {
  struct gab_obj_rec *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_rec, gab_value, (space + len) * 2, kGAB_RECORD);

  self->mask = 0;
  self->vmask = 0;
  self->shape = gab_undefined;
  memcpy(self->data, data, sizeof(gab_value) * len * 2);

  return __gab_obj(self);
}

gab_value __gab_recordnode(struct gab_triple gab, size_t len, size_t space,
                           gab_value *data) {
  struct gab_obj_recnode *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_recnode, gab_value, (space + len) * 2, kGAB_RECORDNODE);

  self->mask = 0;
  self->vmask = 0;
  memcpy(self->data, data, sizeof(gab_value) * len * 2);

  return __gab_obj(self);
}

gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[static len]) {
  gab_value shp = gab.eg->shapes;

  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++)
    shp = gab_shpwith(gab, shp, keys[i * stride]);

  gab_gcunlock(gab.gc);

  return shp;
}

gab_value __gab_shape(struct gab_triple gab, size_t len) {
  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPE);

  self->len = len;

  v_gab_value_create(&self->transitions, 16);

  return __gab_obj(self);
}

gab_value gab_shpwith(struct gab_triple gab, gab_value shp, gab_value key) {
  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);

  size_t idx = gab_shpfind(shp, key);
  if (idx != -1)
    return shp;

  idx = gab_shptfind(shp, key);
  if (idx != -1)
    return v_gab_value_val_at(&s->transitions, idx * 2 + 1);

  gab_value new_shape = __gab_shape(gab, s->len + 1);
  struct gab_obj_shape *self = GAB_VAL_TO_SHAPE(new_shape);

  // Set the keys on the new shape
  memcpy(self->keys, s->keys, sizeof(gab_value) * s->len);
  self->keys[s->len] = key;

  // Push transition into parent shape
  v_gab_value_push(&s->transitions, key);
  v_gab_value_push(&s->transitions, new_shape);

  return new_shape;
}

gab_value gab_shpwithout(struct gab_triple gab, gab_value shp, gab_value key);

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
