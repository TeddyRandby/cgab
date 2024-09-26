#include "core.h"
#include "engine.h"
#include "gab.h"
#include "colors.h"
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
  case kGAB_SHAPE:
  case kGAB_SHAPELIST: {
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

int rec_dump_values(FILE *stream, gab_value rec, int depth) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  size_t len = gab_reclen(rec);

  if (len == 0)
    return 0;

  if (len > 8 && depth >= 0)
    return fprintf(stream, " ... ");

  int32_t bytes = 0;

  for (uint64_t i = 0; i < len; i++) {
    bytes += gab_fvalinspect(stream, gab_uvrecat(rec, i), depth - 1);

    if (i + 1 < len)
      bytes += fprintf(stream, ", ");
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
      bytes += fprintf(stream, ": ");
      bytes += gab_fvalinspect(stream, gab_uvrecat(rec, i), depth - 1);

      if (i + 1 < len)
        bytes += fprintf(stream, ", ");
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
  case kGAB_SYMBOL:
  case kGAB_STRING:
    return fprintf(stream, "%s", gab_strdata(&self));
  case kGAB_MESSAGE: {
    return fprintf(stream, "\\%s", gab_strdata(&self));
  }
  case kGAB_SHAPE:
  case kGAB_SHAPELIST: {
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
    if (gab_valkind(gab_recshp(self)) == kGAB_SHAPELIST)
      return fprintf(stream, "[") + rec_dump_values(stream, self, depth) +
             fprintf(stream, "]");
    else
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
  case kGAB_SHAPE:
  case kGAB_SHAPELIST: {
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
      memcpy(self->data, args.data, args.nupvalues * sizeof(uint8_t));
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

gab_value gab_nlstpush(struct gab_triple gab, gab_value list, size_t len,
                       gab_value values[static len]) {
  assert(gab_valkind(list) == kGAB_RECORD);

  size_t start = gab_reclen(list);

  gab_gclock(gab);
  for (size_t i = 0; i < len; i++) {
    gab_value key = gab_number(start + i);
    gab_value val = values[i];
    list = gab_recput(gab, list, key, val);
  }
  gab_gcunlock(gab);

  return list;
}

gab_value gab_lstpop(struct gab_triple gab, gab_value list, gab_value *popped) {
  assert(gab_valkind(list) == kGAB_RECORD);

  gab_value shp =
      gab_shpwithout(gab, gab_recshp(list), gab_number(gab_reclen(list) - 1));

  //TODO: Actually pop the value from the record

  return recsetshp(reccpy(gab, list, 0), shp);
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

gab_value gab_shptorec(struct gab_triple gab, gab_value shp) {
  assert(gab_valkind(shp) == kGAB_SHAPE || gab_valkind(shp) == kGAB_SHAPELIST);

  size_t len = gab_shplen(shp);

  gab_gclock(gab);

  size_t shift = getshift(len);

  size_t rootlen = getlen(len, shift);

  struct gab_obj_rec *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_rec, gab_value, rootlen, kGAB_RECORD);

  self->shape = shp;
  self->shift = shift;
  self->len = rootlen;

  gab_value res = __gab_obj(self);

  if (len) {
    recfillchildren(gab, res, shift, len, rootlen);

    for (size_t i = 0; i < len; i++) {
      massoc(gab, res, gab_nil, i);
    }
  }

  gab_gcunlock(gab);
  return res;
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

gab_value nth_amongst(size_t n, size_t len, gab_value records[static len]) {
  assert(len > 0);

  size_t r = 0;
  size_t i = 0;

  while (r < len && n >= i + gab_reclen(records[r]))
    i += gab_reclen(records[r++]);

  return gab_uvrecat(records[r], n - i);
}

gab_value gab_nlstcat(struct gab_triple gab, size_t len,
                      gab_value records[static len]) {
  size_t total_len = 0;

  for (size_t i = 0; i < len; i++)
    total_len += gab_reclen(records[i]);

  gab_value total_keys[total_len];
  for (size_t i = 0; i < total_len; i++)
    total_keys[i] = gab_number(i);

  gab_gclock(gab);

  size_t shift = getshift(total_len);

  size_t rootlen = getlen(total_len, shift);

  struct gab_obj_rec *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_rec, gab_value, rootlen, kGAB_RECORD);

  self->shape = gab_shape(gab, 1, total_len, total_keys);
  self->shift = shift;
  self->len = rootlen;

  gab_value res = __gab_obj(self);

  if (total_len) {
    recfillchildren(gab, res, shift, total_len, rootlen);

    assert(total_len == gab_shplen(self->shape));

    for (size_t i = 0; i < total_len; i++) {
      massoc(gab, res, nth_amongst(i, total_len, records), i);
    }
  }

  gab_gcunlock(gab);

  return res;
}

gab_value gab_list(struct gab_triple gab, uint64_t size,
                   gab_value values[size]) {
  gab_gclock(gab);

  gab_value keys[size];
  for (size_t i = 0; i < size; i++) {
    keys[i] = gab_number(i);
  }

  gab_value v = gab_record(gab, 1, size, keys, values);
  gab_gcunlock(gab);
  return v;
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

gab_value gab_nshpcat(struct gab_triple gab, size_t len,
                      gab_value shapes[static len]) {
  assert(len > 0);
  gab_value shp = shapes[0];

  for (size_t i = 1; i < len; i++)
    for (size_t k = 0; k < gab_shplen(shapes[i]); k++)
      shp = gab_shpwith(gab, shp, gab_ushpat(shp, k));

  return shp;
}

gab_value __gab_shape(struct gab_triple gab, size_t len) {
  struct gab_obj_shape *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_shape, gab_value, len, kGAB_SHAPELIST);

  self->len = len;

  v_gab_value_create(&self->transitions, 16);

  return __gab_obj(self);
}

gab_value gab_shpwithout(struct gab_triple gab, gab_value shape, gab_value key) {
  gab_value shp = gab.eg->shapes;

  gab_gclock(gab);

  size_t len = gab_shplen(shape);

  for (size_t i = 0; i < len; i++) {
    gab_value thiskey = gab_ushpat(shape, i);
    if (key == thiskey)
      continue;

    shp = gab_shpwith(gab, shp, thiskey);
  }

  gab_gcunlock(gab);

  assert(len - 1 == gab_shplen(shp));
  return shp;
}

gab_value gab_shpwith(struct gab_triple gab, gab_value shp, gab_value key) {
  mtx_lock(&gab.eg->shapes_mtx);

  assert(gab_valkind(shp) == kGAB_SHAPE || gab_valkind(shp) == kGAB_SHAPELIST);
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

  if (gab_valkind(shp) != kGAB_SHAPELIST || key != gab_number(s->len))
    self->header.kind = kGAB_SHAPE;

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

gab_value gab_fiber(struct gab_triple gab, gab_value receiver,
                    gab_value message, size_t argc, gab_value argv[argc]) {
  assert(gab_valkind(message) == kGAB_MESSAGE);

  struct gab_obj_fiber *self =
      GAB_CREATE_FLEX_OBJ(gab_obj_fiber, gab_value, argc + 2, kGAB_FIBER);

  if (gab_thisfiber(gab) == gab_undefined) {
    self->messages = gab.eg->messages;
    self->macros = gab.eg->macros;
  } else {
    struct gab_obj_fiber *parent = GAB_VAL_TO_FIBER(gab_thisfiber(gab));
    self->messages = parent->messages;
    self->macros = parent->macros;
  }

  self->len = argc + 2;
  memcpy(self->data + 2, argv, argc * sizeof(gab_value));
  self->data[0] = message;
  self->data[1] = receiver;

  self->res = nullptr;
  self->status = kGAB_FIBER_WAITING;

  self->vm.fp = self->vm.sb + 3;
  self->vm.sp = self->vm.sb + 3;

  // Setup main and args
  *self->vm.sp++ = receiver;
  for (uint8_t i = 0; i < argc; i++)
    *self->vm.sp++ = argv[i];

  *self->vm.sp = argc + 1;

  // Setup the return frame
  self->vm.fp[-1] = (uintptr_t) nullptr;
  self->vm.fp[-2] = 0;
  self->vm.fp[-3] = (uintptr_t) nullptr;

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

static const struct timespec t = {.tv_nsec = GAB_CHANNEL_STEP_NS};

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
  return gab_tchntake(gab, c, -1);
}

gab_value gab_tchntake(struct gab_triple gab, gab_value c, size_t nms) {
  assert(gab_valkind(c) == kGAB_CHANNEL ||
         gab_valkind(c) == kGAB_CHANNELCLOSED ||
         gab_valkind(c) == kGAB_CHANNELBUFFERED);

  struct gab_obj_channel *channel = GAB_VAL_TO_CHANNEL(c);

  size_t timeout = 0;

  switch (channel->header.kind) {
  case kGAB_CHANNEL: {
    mtx_lock(&channel->mtx);

    while (gab_chnisempty(c)) {
      gab_yield(gab);

      if (cnd_timedwait(&channel->p_cnd, &channel->mtx, &t) == thrd_timedout)
        timeout += cGAB_CHANNEL_STEP_MS;

      if (gab_chnisclosed(c)) {
        mtx_unlock(&channel->mtx);
        return gab_undefined;
      }

      if (timeout > nms) {
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


static uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset);

static uint64_t dumpSimpleInstruction(FILE *stream,
                                      struct gab_obj_prototype *self,
                                      uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

static uint64_t dumpSendInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

  uint16_t constant =
      ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)) << 8 |
      v_uint8_t_val_at(&self->src->bytecode, offset + 2);

  gab_value msg = v_gab_value_val_at(&self->src->constants, constant);

  uint8_t have = v_uint8_t_val_at(&self->src->bytecode, offset + 3);

  uint8_t var = have & fHAVE_VAR;
  uint8_t tail = have & fHAVE_TAIL;
  have = have >> 2;

  fprintf(stream, "%-25s" GAB_BLUE, name);
  gab_fvalinspect(stream, msg, 0);
  fprintf(stream, GAB_RESET " (%d%s)%s\n", have, var ? " & more" : "",
          tail ? " [TAILCALL]" : "");

  return offset + 4;
}

static uint64_t dumpByteInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

static uint64_t dumpTrimInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t wantbyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, wantbyte);
  return offset + 2;
}

static uint64_t dumpReturnInstruction(FILE *stream,
                                      struct gab_obj_prototype *self,
                                      uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s%hhx%s\n", "RETURN", have,
          havebyte & fHAVE_VAR ? " & more" : "");
  return offset + 2;
}

static uint64_t dumpPackInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t operandA = v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  uint8_t operandB = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s(%hhx%s) -> %hhx %hhx\n", name, have,
          havebyte & fHAVE_VAR ? " & more" : "", operandA, operandB);
  return offset + 4;
}

static uint64_t dumpConstantInstruction(FILE *stream,
                                        struct gab_obj_prototype *self,
                                        uint64_t offset) {
  uint16_t constant =
      ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)) << 8 |
      v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s", name);
  gab_fvalinspect(stdout, v_gab_value_val_at(&self->src->constants, constant),
                  0);
  fprintf(stream, "\n");
  return offset + 3;
}

static uint64_t dumpNConstantInstruction(FILE *stream,
                                         struct gab_obj_prototype *self,
                                         uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s", name);

  uint8_t n = v_uint8_t_val_at(&self->src->bytecode, offset + 1);

  for (int i = 0; i < n; i++) {
    uint16_t constant =
        ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 2 + (2 * i)))
            << 8 |
        v_uint8_t_val_at(&self->src->bytecode, offset + 3 + (2 * i));

    gab_fvalinspect(stdout, v_gab_value_val_at(&self->src->constants, constant),
                    0);

    if (i < n - 1)
      fprintf(stream, ", ");
  }

  fprintf(stream, "\n");
  return offset + 2 + (2 * n);
}

static uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->src->bytecode, offset);
  switch (op) {
  case OP_POP:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpPackInstruction(stream, self, offset);
  case OP_NCONSTANT:
    return dumpNConstantInstruction(stream, self, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_SEND:
  case OP_SEND_BLOCK:
  case OP_SEND_NATIVE:
  case OP_SEND_PROPERTY:
  case OP_SEND_PRIMITIVE_CONCAT:
  case OP_SEND_PRIMITIVE_SPLAT:
  case OP_SEND_PRIMITIVE_ADD:
  case OP_SEND_PRIMITIVE_SUB:
  case OP_SEND_PRIMITIVE_MUL:
  case OP_SEND_PRIMITIVE_DIV:
  case OP_SEND_PRIMITIVE_MOD:
  case OP_SEND_PRIMITIVE_EQ:
  case OP_SEND_PRIMITIVE_LT:
  case OP_SEND_PRIMITIVE_LTE:
  case OP_SEND_PRIMITIVE_GT:
  case OP_SEND_PRIMITIVE_GTE:
  case OP_SEND_PRIMITIVE_CALL_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_NATIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_PRIMITIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_NATIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_CONSTANT:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_MESSAGE_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_PROPERTY:
  case OP_TAILSEND_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_BLOCK:
  case OP_LOCALSEND_BLOCK:
  case OP_LOCALTAILSEND_BLOCK:
  case OP_MATCHSEND_BLOCK:
  case OP_MATCHTAILSEND_BLOCK:
    return dumpSendInstruction(stream, self, offset);
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POPSTORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_LOAD_LOCAL:
    return dumpByteInstruction(stream, self, offset);
  case OP_NPOPSTORE_STORE_LOCAL:
  case OP_NPOPSTORE_LOCAL:
  case OP_NLOAD_UPVALUE:
  case OP_NLOAD_LOCAL: {
    const char *name =
        gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

    uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);

    fprintf(stream, "%-25s%hhx: ", name, operand);

    for (int i = 0; i < operand - 1; i++) {
      fprintf(stream, "%hhx, ",
              v_uint8_t_val_at(&self->src->bytecode, offset + 2 + i));
    }

    fprintf(stream, "%hhx\n",
            v_uint8_t_val_at(&self->src->bytecode, offset + 1 + operand));

    return offset + 2 + operand;
  }
  case OP_RETURN:
    return dumpReturnInstruction(stream, self, offset);
  case OP_BLOCK: {
    offset++;

    uint16_t proto_constant =
        (((uint16_t)self->src->bytecode.data[offset] << 8) |
         self->src->bytecode.data[offset + 1]);

    offset += 2;

    gab_value pval = v_gab_value_val_at(&self->src->constants, proto_constant);

    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    printf("%-25s" GAB_CYAN "%-20s\n" GAB_RESET, "OP_BLOCK",
           gab_strdata(&p->src->name));

    for (int j = 0; j < p->nupvalues; j++) {
      int isLocal = p->data[j] & fLOCAL_LOCAL;
      uint8_t index = p->data[j] >> 1;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_TRIM_UP1:
  case OP_TRIM_UP2:
  case OP_TRIM_UP3:
  case OP_TRIM_UP4:
  case OP_TRIM_UP5:
  case OP_TRIM_UP6:
  case OP_TRIM_UP7:
  case OP_TRIM_UP8:
  case OP_TRIM_UP9:
  case OP_TRIM_DOWN1:
  case OP_TRIM_DOWN2:
  case OP_TRIM_DOWN3:
  case OP_TRIM_DOWN4:
  case OP_TRIM_DOWN5:
  case OP_TRIM_DOWN6:
  case OP_TRIM_DOWN7:
  case OP_TRIM_DOWN8:
  case OP_TRIM_DOWN9:
  case OP_TRIM_EXACTLY0:
  case OP_TRIM_EXACTLY1:
  case OP_TRIM_EXACTLY2:
  case OP_TRIM_EXACTLY3:
  case OP_TRIM_EXACTLY4:
  case OP_TRIM_EXACTLY5:
  case OP_TRIM_EXACTLY6:
  case OP_TRIM_EXACTLY7:
  case OP_TRIM_EXACTLY8:
  case OP_TRIM_EXACTLY9:
  case OP_TRIM: {
    return dumpTrimInstruction(stream, self, offset);
  }
  default: {
    uint8_t code = v_uint8_t_val_at(&self->src->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

int gab_fmodinspect(FILE *stream, struct gab_obj_prototype *proto) {
  uint64_t offset = proto->offset;

  uint64_t end = proto->offset + proto->len;

  printf("     ");
  gab_fvalinspect(stream, proto->src->name, 0);
  printf("\n");

  while (offset < end) {
    fprintf(stream, GAB_YELLOW "%04lu " GAB_RESET, offset);
    offset = dumpInstruction(stream, proto, offset);
  }

  return 0;
}

#undef CREATE_GAB_FLEX_OBJ
#undef CREATE_GAB_OBJ
