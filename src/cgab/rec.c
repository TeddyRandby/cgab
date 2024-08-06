#include "gab.h"

gab_value rec_cpy(struct gab_triple gab, gab_value rec, size_t space) {
  switch (gab_valkind(rec)) {
  case kGAB_REC: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(rec);

    struct gab_obj_rec *nr =
        GAB_VAL_TO_REC(__gab_rec(gab, n->shape, n->len, space, n->data));

    return __gab_obj(nr);
  }
  case kGAB_RECNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(rec);

    struct gab_obj_recnode *nm =
        GAB_VAL_TO_RECNODE(__gab_recnode(gab, n->len, space, n->data));

    return __gab_obj(nm);
  }
  default:
    assert(0 && "Only rec and recnodebranch cpy");
  }
}

gab_value rec_has(gab_value rec, size_t idx) {
  switch (gab_valkind(rec)) {
  case kGAB_REC: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(rec);
    return idx < n->len;
  }
  case kGAB_RECNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(rec);
    return idx < n->len;
  }
  default:
    assert(0 && "Only rec and recnode has");
  }
}

gab_value rec_get(gab_value rec, size_t idx) {
  switch (gab_valkind(rec)) {
  case kGAB_REC: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(rec);
    assert(idx < n->len);
    return n->data[idx];
  }
  case kGAB_RECNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(rec);
    assert(idx < n->len);
    return n->data[idx];
  }
  default:
    assert(0 && "Only rec and recnode get");
  }
}

void rec_set(gab_value rec, size_t idx, gab_value v) {
  switch (gab_valkind(rec)) {
  case kGAB_REC: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(rec);
    assert(idx < n->len);
    n->data[idx] = v;
  }
  case kGAB_RECNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(rec);
    assert(idx < n->len);
    n->data[idx] = v;
  }
  default:
    assert(0 && "Only rec and recnode get");
  }
}

gab_value gab_recat(struct gab_triple gab, gab_value rec, size_t idx) {
  assert(gab_valkind(rec) == kGAB_REC);
  struct gab_obj_rec *root = GAB_VAL_TO_REC(rec);

  gab_value *node = root->data;

  for (uint64_t level = root->shift; level > 0; level -= GAB_REC_BITS) {
    gab_value branch = node[(idx >> level) & GAB_REC_MASK];
    assert(branch);
    assert(gab_valkind(branch) == kGAB_RECNODE);
    node = GAB_VAL_TO_RECNODE(branch)->data;
  }

  return node[idx & GAB_REC_MASK];
}

gab_value gab_recput(struct gab_triple gab, gab_value rec, size_t idx,
                     gab_value val) {
  assert(gab_valkind(rec) == kGAB_REC);
  struct gab_obj_rec *root = GAB_VAL_TO_REC(rec);

  if (idx > root->size)
    return gab_undefined;

  gab_value node = rec_cpy(gab, rec, idx > root->len), rootnode = node;

  for (size_t level = root->shift; level > 0; level -= GAB_REC_BITS) {
    size_t subidx = (idx >> level) & GAB_REC_MASK;

    // Node is already a copy
    if (rec_has(node, subidx)) {
      gab_value branch = rec_get(node, subidx);
      assert(gab_valkind(branch) == kGAB_RECNODE);

      branch = rec_cpy(gab, branch, 0);
      rec_set(node, subidx, branch);
      node = branch;
    } else {
      gab_value branch = __gab_recnode(gab, 0, 1, nullptr);
      rec_set(node, subidx, branch);
      node = branch;
    }
  }

  rec_set(node, idx & GAB_REC_MASK, val);

  return rootnode;
}

gab_value gab_recpush(struct gab_triple gab, gab_value rec, gab_value val);

gab_value gab_recpop(struct gab_triple gab, gab_value rec, gab_value *valout);

void gab_mrecput(struct gab_triple gab, gab_value rec, size_t key,
                 gab_value val);

void gab_mrecpush(struct gab_triple gab, gab_value rec, gab_value val);

void gab_mrecpop(struct gab_triple gab, gab_value rec, gab_value *valout);
