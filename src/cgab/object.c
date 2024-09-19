#include "engine.h"
#include "gab.h"
#include "lexer.h"
#include <threads.h>

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

  struct gab_jb *wk = gab.eg->jobs + gab.wkid;
  if (wk->locked) {
    v_gab_obj_push(&wk->lock_keep, self);
#if cGAB_LOG_GC
    printf("QLOCK\t%p\n", (void *)self);
#endif
  }

  gab_dref(gab, __gab_obj(self));

  return self;
}

size_t gab_obj_size(struct gab_obj *obj) {
  switch (obj->kind) {
  case kGAB_CHANNEL: {
    struct gab_obj_channel *o = (struct gab_obj_channel *)obj;
    return sizeof(struct gab_obj_channel) + o->len * sizeof(gab_value);
  }
  case kGAB_BOX: {
    struct gab_obj_box *o = (struct gab_obj_box *)obj;
    return sizeof(struct gab_obj_box) + o->len * sizeof(char);
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *o = (struct gab_obj_recnode *)obj;
    return sizeof(struct gab_obj_recnode) + o->len * sizeof(gab_value);
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *o = (struct gab_obj_rec *)obj;
    return sizeof(struct gab_obj_rec) + o->len * sizeof(gab_value);
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
  case kGAB_FIBER:
    return sizeof(struct gab_obj_fiber);
  case kGAB_NATIVE:
    return sizeof(struct gab_obj_native);
  default:
    break;
  }
  assert(false && "unreachable");
  return 0;
}

int shape_dump_keys(FILE *stream, gab_value shape, int depth) {
  struct gab_obj_shape *shp = GAB_VAL_TO_SHAPE(shape);
  size_t len = shp->len;

  if (len == 0)
    return 0;

  if (len > 8 && depth >= 0)
    return fprintf(stream, "... ");

  int bytes = 0;

  for (size_t i = 0; i < len; i++) {
    bytes += gab_fvalinspect(stream, shp->keys[i], depth - 1);

    if (i + 1 < len)
      bytes += fprintf(stream, " ");
  }

  return bytes;
}

int rec_dump_properties(FILE *stream, gab_value rec, int depth) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    size_t len = gab_reclen(rec);

    if (len == 0)
      return 0;

    if (len > 8 && depth >= 0)
      return fprintf(stream, " ... ");

    int32_t bytes = 0;

    for (uint64_t i = 0; i < len; i++) {
      bytes += gab_fvalinspect(stream, gab_ukrecat(rec, i), depth - 1);
      bytes += fprintf(stream, ":");
      bytes += gab_fvalinspect(stream, gab_uvrecat(rec, i), depth - 1);

      if (i + 1 < len)
        bytes += fprintf(stream, ",");
    }

    return bytes;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);
    size_t len = m->len;

    if (len == 0)
      return fprintf(stream, "~ ");

    if (len > 8)
      return fprintf(stream, "... ");

    int32_t bytes = 0;

    for (uint64_t i = 0; i < len; i++) {
      bytes += gab_fvalinspect(stream, m->data[i], depth - 1);

      if (i + 1 < len)
        bytes += fprintf(stream, ", ");
    }

    return bytes;
    break;
  }
  default:
    break;
  }
  assert(false && "NOT A REC");
  return 0;
}

int gab_fvalinspect(FILE *stream, gab_value self, int depth) {
  switch (gab_valkind(self)) {
  case kGAB_PRIMITIVE:
    return fprintf(stream, "<gab.primitive %s>",
                   gab_opcode_names[gab_valtop(self)]);
  case kGAB_UNDEFINED:
    return fprintf(stream, "undefined");
  case kGAB_NUMBER:
    return fprintf(stream, "%lg", gab_valton(self));
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
  case kGAB_CHANNEL:
  case kGAB_CHANNELCLOSED:
  case kGAB_CHANNELBUFFERED: {
    return fprintf(stream, "<gab.channel %p>", GAB_VAL_TO_CHANNEL(self));
  }
  case kGAB_FIBER: {
    return fprintf(stream, "<gab.fiber %p>", GAB_VAL_TO_FIBER(self));
  }
  case kGAB_RECORD: {
    return fprintf(stream, "{") + rec_dump_properties(stream, self, depth) +
           fprintf(stream, "}");
  }
  case kGAB_RECORDNODE: {
    return fprintf(stream, "[") + rec_dump_properties(stream, self, depth) +
           fprintf(stream, "]");
  }
  case kGAB_BOX: {
    struct gab_obj_box *con = GAB_VAL_TO_BOX(self);
    return fprintf(stream, "<") + gab_fvalinspect(stream, con->type, depth) +
           fprintf(stream, " %p>", con->data);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(self);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);
    size_t line = gab_srcline(p->src, p->offset + 2);
    return fprintf(stream, "<gab.block ") +
           gab_fvalinspect(stream, gab_srcname(p->src), depth) +
           fprintf(stream, ":%lu>", line);
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
  default:
    break;
  }
  assert(false && "NOT AN OBJECT");
  return 0;
}

void gab_obj_destroy(struct gab_eg *gab, struct gab_obj *self) {
  switch (self->kind) {
  case kGAB_CHANNELBUFFERED:
  case kGAB_CHANNELCLOSED:
  case kGAB_CHANNEL: {
    struct gab_obj_channel *chn = (struct gab_obj_channel *)self;
    mtx_destroy(&chn->mtx);
    cnd_destroy(&chn->p_cnd);
    cnd_destroy(&chn->t_cnd);
    break;
  }
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

  mtx_lock(&gab.eg->strings_mtx);

  s_char str = s_char_create(data, len);

  uint64_t hash = s_char_hash(str, gab.eg->hash_seed);

  struct gab_obj_string *interned = gab_egstrfind(gab.eg, str, hash);

  if (interned)
    return mtx_unlock(&gab.eg->strings_mtx), __gab_obj(interned);

  struct gab_obj_string *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_string, char, str.len + 1, kGAB_STRING);

  memcpy(self->data, str.data, str.len);
  self->len = str.len;
  self->hash = hash;
  self->data[str.len] = '\0';

  d_strings_insert(&gab.eg->strings, self, 0);

  return mtx_unlock(&gab.eg->strings_mtx), __gab_obj(self);
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
  self->type = args.type;
  self->len = args.size;

  if (args.data) {
    memcpy(self->data, args.data, args.size);
  } else {
    memset(self->data, 0, args.size);
  }

  return __gab_obj(self);
}

gab_value __gab_record(struct gab_triple gab, size_t len, size_t space,
                       gab_value *data) {
  struct gab_obj_rec *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_rec, gab_value, space + len, kGAB_RECORD);

  self->len = len + space;
  self->shift = 0;
  self->shape = gab_undefined;
  memcpy(self->data, data, sizeof(gab_value) * len);

  for (size_t i = len; i < self->len; i++)
    self->data[i] = gab_undefined;

  return __gab_obj(self);
}

gab_value __gab_recordnode(struct gab_triple gab, size_t len, size_t space,
                           gab_value *data) {
  assert(space + len > 0);
  struct gab_obj_recnode *self = GAB_CREATE_FLEX_OBJ(
      gab_obj_recnode, gab_value, space + len, kGAB_RECORDNODE);

  self->len = len + space;
  memcpy(self->data, data, sizeof(gab_value) * len);

  for (size_t i = len; i < self->len; i++)
    self->data[i] = gab_undefined;

  return __gab_obj(self);
}

gab_value reccpy(struct gab_triple gab, gab_value r, size_t space) {
  switch (gab_valkind(r)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(r);

    struct gab_obj_rec *nm =
        GAB_VAL_TO_REC(__gab_record(gab, n->len, space, n->data));

    nm->shift = n->shift;
    nm->shape = n->shape;

    return __gab_obj(nm);
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(r);

    return __gab_recordnode(gab, n->len, space, n->data);
  }
  case kGAB_UNDEFINED: {
    return __gab_recordnode(gab, 0, 1, nullptr);
  }
  default:
    break;
  }

  assert(0 && "Only rec and recnodebranch cpy");
  return gab_undefined;
}

void recassoc(gab_value rec, gab_value v, size_t i) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *r = GAB_VAL_TO_RECNODE(rec);
    assert(i < r->len);
    r->data[i] = v;
    return;
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
    assert(i < r->len);
    r->data[i] = v;
    return;
  }
  default:
    break;
  }
  assert(false && "UNREACHABLE");
}

gab_value recnth(gab_value rec, size_t n) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *r = GAB_VAL_TO_RECNODE(rec);
    assert(n < r->len);
    return r->data[n];
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
    assert(n < r->len);
    return r->data[n];
  }
  default:
    break;
  }

  assert(false && "UNREACHABLE");
  return gab_undefined;
}

size_t reclen(gab_value rec) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *r = GAB_VAL_TO_RECNODE(rec);
    return r->len;
  }
  case kGAB_RECORD: {
    struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
    return r->len;
  }
  case kGAB_UNDEFINED: {
    return 0;
  }
  default:
    break;
  }

  assert(false && "UNREACHABLE");
  return 0;
}

gab_value gab_uvrecat(gab_value rec, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);

  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  gab_value node = rec;

  for (size_t level = r->shift; level > 0; level -= GAB_PVEC_BITS) {
    size_t idx = (i >> level) & GAB_PVEC_MASK;
    gab_value next_node = recnth(rec, idx);
    assert(gab_valkind(next_node) == kGAB_RECORDNODE ||
           gab_valkind(next_node) == kGAB_RECORD);
    node = next_node;
  }

  return recnth(node, i & GAB_PVEC_MASK);
}

bool recneedsspace(gab_value rec, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
  size_t idx = (i >> r->shift) & GAB_PVEC_MASK;
  return idx >= r->len;
}

gab_value recsetshp(gab_value rec, gab_value shp) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
  r->shape = shp;
  return rec;
}

gab_value assoc(struct gab_triple gab, gab_value rec, gab_value v, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  gab_value node = rec;
  gab_value root = node;
  gab_value path = root;

  for (int64_t level = r->shift; level > 0; level -= GAB_PVEC_BITS) {
    size_t idx = (i >> level) & GAB_PVEC_MASK;

    size_t nidx = (i >> (level - GAB_PVEC_BITS)) & GAB_PVEC_MASK;

    if (idx < reclen(node))
      node = reccpy(gab, recnth(node, idx), nidx >= reclen(recnth(node, idx)));
    else
      node = __gab_recordnode(gab, 0, 1, nullptr);

    recassoc(path, node, idx);
    path = node;
  }

  assert(node != gab_undefined);
  recassoc(node, v, i & GAB_PVEC_MASK);
  return root;
}

void massoc(struct gab_triple gab, gab_value rec, gab_value v, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  assert(i < gab_reclen(rec));

  gab_value node = rec;

  for (int64_t level = r->shift; level > 0; level -= GAB_PVEC_BITS) {
    size_t idx = (i >> level) & GAB_PVEC_MASK;

    assert(idx < reclen(node));
    node = recnth(node, idx);
  }

  assert(node != gab_undefined);
  recassoc(node, v, i & GAB_PVEC_MASK);

  return;
}

gab_value cons(struct gab_triple gab, gab_value rec, gab_value v,
               gab_value shp) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  size_t i = gab_reclen(rec);
  gab_value new_root;

  // overflow root
  if ((i >> GAB_PVEC_BITS) >= ((size_t)1 << r->shift)) {
    new_root = __gab_record(gab, 1, 1, &rec);
    struct gab_obj_rec *new_r = GAB_VAL_TO_REC(new_root);
    new_r->shape = shp;
    new_r->shift = r->shift + 5;
    new_root = assoc(gab, new_root, v, i);
  } else {
    new_root = recsetshp(
        assoc(gab, reccpy(gab, rec, recneedsspace(rec, i)), v, i), shp);
  }

  return new_root;
}

gab_value gab_recput(struct gab_triple gab, gab_value rec, gab_value key,
                     gab_value val) {
  assert(gab_valkind(rec) == kGAB_RECORD);

  size_t idx = gab_recfind(rec, key);

  if (idx == -1)
    return cons(gab, rec, val, gab_shpwith(gab, gab_recshp(rec), key));
  else
    return assoc(gab, reccpy(gab, rec, recneedsspace(rec, idx)), val, idx);
}

gab_value gab_urecput(struct gab_triple gab, gab_value rec, size_t i,
                      gab_value v) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  assert(i < gab_reclen(rec));

  return assoc(gab, reccpy(gab, rec, 0), v, i);
}

gab_value gab_recdel(struct gab_triple gab, gab_value rec, gab_value key) {
  return rec;
}

size_t getlen(size_t n, size_t shift) {
  if (n)
    n--;

  return ((n >> shift) & GAB_PVEC_MASK) + 1;
}

void recfillchildren(struct gab_triple gab, gab_value rec, size_t shift,
                     size_t n, size_t len) {
  assert(len > 0);

  if (shift == 0)
    return;

  for (size_t l = 0; l < len - 1; l++) {
    gab_value lhs_child = __gab_recordnode(gab, 0, GAB_PVEC_SIZE, nullptr);

    recfillchildren(gab, lhs_child, shift - 5, n, 32);

    recassoc(rec, lhs_child, l);
  }

  size_t rhs_childlen = getlen(n, shift - 5);

  gab_value rhs_child = __gab_recordnode(gab, 0, rhs_childlen, nullptr);

  recfillchildren(gab, rhs_child, shift - 5, n, rhs_childlen);

  recassoc(rec, rhs_child, len - 1);
}

size_t getshift(size_t n) {
  size_t shift = 0;

  if (n)
    n--;

  while ((n >> GAB_PVEC_BITS) >= (1 << shift)) {
    shift += 5;
  }

  return shift;
}

gab_value gab_record(struct gab_triple gab, size_t stride, size_t len,
                     gab_value keys[static len], gab_value vals[static len]) {
  gab_gclock(gab);

  size_t shift = getshift(len);

  size_t rootlen = getlen(len, shift);

  struct gab_obj_rec *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_rec, gab_value, rootlen, kGAB_RECORD);

  self->shape = gab_shape(gab, stride, len, keys);
  self->shift = shift;
  self->len = rootlen;

  gab_value res = __gab_obj(self);


  if (len) {
    recfillchildren(gab, res, shift, len, rootlen);

    assert(len == gab_shplen(self->shape));

    for (size_t i = 0; i < len; i++) {
      massoc(gab, res, vals[i * stride], i);
    }
  }

  gab_gcunlock(gab);

  return res;
}

gab_value gab_shape(struct gab_triple gab, size_t stride, size_t len,
                    gab_value keys[static len]) {
  gab_value shp = gab.eg->shapes;

  gab_gclock(gab);

  for (size_t i = 0; i < len; i++)
    shp = gab_shpwith(gab, shp, keys[i * stride]);

  gab_gcunlock(gab);

  assert(len == gab_shplen(shp));
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
  mtx_lock(&gab.eg->shapes_mtx);

  assert(gab_valkind(shp) == kGAB_SHAPE);
  struct gab_obj_shape *s = GAB_VAL_TO_SHAPE(shp);

  size_t idx = gab_shpfind(shp, key);
  if (idx != -1) {
    mtx_unlock(&gab.eg->shapes_mtx);
    return shp;
  }

  idx = gab_shptfind(shp, key);
  if (idx != -1) {
    mtx_unlock(&gab.eg->shapes_mtx);
    return v_gab_value_val_at(&s->transitions, idx * 2 + 1);
  }

  gab_value new_shape = __gab_shape(gab, s->len + 1);
  struct gab_obj_shape *self = GAB_VAL_TO_SHAPE(new_shape);

  // Set the keys on the new shape
  memcpy(self->keys, s->keys, sizeof(gab_value) * s->len);
  self->keys[s->len] = key;

  if (!GAB_OBJ_IS_NEW((struct gab_obj *)s)) {
    gab_iref(gab, new_shape);
    gab_iref(gab, key);
  }

  // Push transition into parent shape
  v_gab_value_push(&s->transitions, key);
  v_gab_value_push(&s->transitions, new_shape);

  mtx_unlock(&gab.eg->shapes_mtx);
  return new_shape;
}

gab_value gab_shpwithout(struct gab_triple gab, gab_value shp, gab_value key);

gab_value gab_fiber(struct gab_triple gab, gab_value main, size_t argc,
                    gab_value argv[argc]) {
  assert(gab_valkind(main) == kGAB_BLOCK);

  struct gab_obj_fiber *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_fiber, gab_value, argc + 1, kGAB_FIBER);

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(main);

  if (gab_thisfiber(gab) == gab_undefined) {
    self->messages = gab.eg->messages;
  } else {
    self->messages = gab_fibmsg(gab_thisfiber(gab));
  }

  self->len = argc + 1;
  memcpy(self->data, argv, argc * sizeof(gab_value));
  self->data[argc] = main;

  self->res = nullptr;
  self->status = kGAB_FIBER_WAITING;

  self->vm.fp = self->vm.sb + 3;
  self->vm.sp = self->vm.sb + 3;

  // Setup main and args
  *self->vm.sp++ = main;
  for (uint8_t i = 0; i < argc; i++)
    *self->vm.sp++ = argv[i];

  *self->vm.sp = argc;

  // Setup the return frame
  self->vm.fp[-1] = (uintptr_t)self->vm.sb - 1;
  self->vm.fp[-2] = 0;
  self->vm.fp[-3] = (uintptr_t)b;

  self->vm.ip = nullptr;

  return __gab_obj(self);
}

a_gab_value *gab_fibawait(struct gab_triple gab, gab_value f) {
  assert(gab_valkind(f) == kGAB_FIBER);
  struct gab_obj_fiber *fiber = GAB_VAL_TO_FIBER(f);

  while (fiber->status != kGAB_FIBER_DONE)
    gab_yield(gab);

  return fiber->res;
}

gab_value gab_channel(struct gab_triple gab, size_t len) {
  enum gab_kind k = len ? kGAB_CHANNELBUFFERED : kGAB_CHANNEL;
  len = len ? len : 1;
  struct gab_obj_channel *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_channel, gab_value, len, k);

  self->len = len;
  self->head = -1;
  self->tail = -1;

  mtx_init(&self->mtx, mtx_plain);
  cnd_init(&self->p_cnd);
  cnd_init(&self->t_cnd);

  return __gab_obj(self);
}

static const struct timespec t = {.tv_nsec = 500};

void gab_chnclose(gab_value c) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);

  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);
  mtx_lock(&channel->mtx);

  channel->header.kind = kGAB_CHANNELCLOSED;

  mtx_unlock(&channel->mtx);
}

bool gab_chnisclosed(gab_value c) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);

  return GAB_VAL_TO_CHANNEL(c)->header.kind == kGAB_CHANNELCLOSED;
};

bool gab_chnisempty(gab_value c) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);
  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);

  return channel->head == -1;
};

bool gab_chnisfull(gab_value c) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);
  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);

  if (channel->head == 0 && channel->tail == channel->len - 1)
    return true;

  if ((channel->tail + 1) % channel->len == channel->head)
    return true;

  return false;
};

void doput(struct gab_obj_channel *channel, gab_value value) {
  channel->tail++;

  if (channel->tail == channel->len)
    channel->tail = 0;

  if (channel->head == -1)
    channel->head = 0;

  channel->data[channel->tail] = value;
}

void gab_chnput(struct gab_triple gab, gab_value c, gab_value value) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);
  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);

  switch (channel->header.kind) {
  case kGAB_CHANNEL:
    mtx_lock(&channel->mtx);

    while (gab_chnisfull(c)) {
      gab_yield(gab);

      cnd_timedwait(&channel->t_cnd, &channel->mtx, &t);

      if (gab_chnisclosed(c)) {
        mtx_unlock(&channel->mtx);
        return;
      }
    }

    doput(channel, value);

    cnd_signal(&channel->p_cnd);

    mtx_unlock(&channel->mtx);

    break;
  default:
    break;
  }
}

gab_value dotake(struct gab_obj_channel *channel) {
  gab_value res = channel->data[channel->head];

  if (channel->head == channel->tail) {
    channel->head = -1;
    channel->tail = -1;
  } else {
    if (channel->head == channel->len)
      channel->head = 0;
    else
      channel->head++;
  }

  return res;
}

gab_value gab_chntake(struct gab_triple gab, gab_value c) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);

  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);

  switch (channel->header.kind) {
  case kGAB_CHANNEL: {
    mtx_lock(&channel->mtx);

    while (gab_chnisempty(c)) {
      gab_yield(gab);

      cnd_timedwait(&channel->p_cnd, &channel->mtx, &t);

      if (gab_chnisclosed(c)) {
        mtx_unlock(&channel->mtx);
        return gab_undefined;
      }
    }

    gab_value res = dotake(channel);

    cnd_signal(&channel->t_cnd);

    mtx_unlock(&channel->mtx);

    return res;
  }
  case kGAB_CHANNELCLOSED: {
    mtx_lock(&channel->mtx);

    if (gab_chnisempty(c)) {
      mtx_unlock(&channel->mtx);
      return gab_undefined;
    }

    gab_value res = dotake(channel);

    cnd_signal(&channel->t_cnd);

    mtx_unlock(&channel->mtx);

    return res;
  }
  default:
    break;
  }

  assert(false && "NOT A CHANNEL");
  return gab_undefined;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
