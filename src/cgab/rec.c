#include "gab.h"

/*
 * Gab recs are persistent HAMTs. These are a tried-and-true immutable data
 * structure used in multiple other languages - Elixir, Gleam, and Clojure to
 * name a few.
 *
 * This implemetnation is *HEAVILY* inspired by
 * https://github.com/mkirchner/hamt
 */

#define BITS (5)
#define WIDTH (1 << BITS)
#define MASK (WIDTH - 1)

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

  if (i > gab_reclen(rec))
    return gab_undefined;

  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  gab_value node = rec;

  for (size_t level = r->shift; level > 0; level -= BITS) {
    size_t idx = (i >> level) & MASK;
    gab_value next_node = recnth(rec, idx);
    assert(gab_valkind(next_node) == kGAB_RECORDNODE ||
           gab_valkind(next_node) == kGAB_RECORD);
    node = next_node;
  }

  return recnth(node, i & MASK);
}

bool recneedsspace(gab_value rec, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);
  size_t idx = (i >> r->shift) & MASK;
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

  if (i > gab_reclen(rec))
    return gab_undefined;

  gab_value node = rec;
  gab_value root = node;
  gab_value path = root;

  for (int64_t level = r->shift; level > 0; level -= BITS) {
    size_t idx = (i >> level) & MASK;

    size_t nidx = (i >> (level - BITS)) & MASK;

    if (idx < reclen(node))
      node = reccpy(gab, recnth(node, idx), nidx >= reclen(recnth(node, idx)));
    else
      node = __gab_recordnode(gab, 0, 1, nullptr);

    recassoc(path, node, idx);
    path = node;
  }

  assert(node != gab_undefined);
  recassoc(node, v, i & MASK);
  return root;
}

gab_value cons(struct gab_triple gab, gab_value rec, gab_value v,
               gab_value shp) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *r = GAB_VAL_TO_REC(rec);

  size_t i = gab_reclen(rec);
  gab_value new_root;

  // overflow root
  if ((i >> BITS) >= ((size_t)1 << r->shift)) {
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
  assert(i < reclen(rec));

  return assoc(gab, reccpy(gab, rec, 0), v, i);
}

gab_value gab_recdel(struct gab_triple gab, gab_value rec, gab_value key) {
  return rec;
}
