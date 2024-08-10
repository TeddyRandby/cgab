#include "core.h"
#include "gab.h"
#include "hash.h"

/*
 * Gab recs are persistent HAMTs. These are a tried-and-true immutable data
 * structure used in multiple other languages - Elixir, Gleam, and Clojure to
 * name a few.
 *
 * This implemetnation is *HEAVILY* inspired by
 * https://github.com/mkirchner/hamt
 */
enum rec_result {
  kSEARCH_SUCCESS = 0,
  kSEARCH_KEYMISMATCH,
  kSEARCH_NOTFOUND,
};

static inline size_t hash_index(size_t hash, size_t shift) {
  return (hash >> shift) & GAB_HAMT_MASK;
}

static inline size_t shift_next(size_t shift) {
  shift += GAB_HAMT_BITS;

  if (shift > GAB_HAMT_SIZE - GAB_HAMT_BITS)
    shift = shift - (GAB_HAMT_SIZE - GAB_HAMT_BITS);

  return shift;
}

static inline bool rec_hasindex(gab_value rec, size_t idx) {
  assert(idx < GAB_HAMT_SIZE);

  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return GAB_VAL_TO_REC(rec)->mask & ((size_t)1 << idx);
  case kGAB_RECORDNODE:
    return GAB_VAL_TO_RECNODE(rec)->mask & ((size_t)1 << idx);
  default:
    assert(0 && "Only rec and recnode can have indexes");
  }
}

static inline size_t rec_mask(gab_value rec) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return GAB_VAL_TO_REC(rec)->mask;
  case kGAB_RECORDNODE:
    return GAB_VAL_TO_RECNODE(rec)->mask;
  default:
    assert(0 && "Only rec and recnode can have indexes");
  }
}

enum node_k {
  kBRANCH = 0,
  kLEAF = 1,
};

enum {
  kLEAF_KEY = 0,
  kLEAF_VALUE = 1,
};

static inline enum node_k rec_nodekat(gab_value rec, size_t pos) {
  assert(pos < GAB_HAMT_SIZE);

  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return (GAB_VAL_TO_REC(rec)->vmask & ((size_t)1 << pos)) ? kLEAF : kBRANCH;
  case kGAB_RECORDNODE:
    return (GAB_VAL_TO_RECNODE(rec)->vmask & ((size_t)1 << pos)) ? kLEAF
                                                                 : kBRANCH;
  default:
    assert(0 && "INVALID NODE K");
  }
}

#define popcount(n) __builtin_popcountl(n)

static inline size_t rec_posat(gab_value rec, size_t index) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return popcount(GAB_VAL_TO_REC(rec)->mask & (((size_t)1 << index) - 1));
  case kGAB_RECORDNODE:
    return popcount(GAB_VAL_TO_RECNODE(rec)->mask & (((size_t)1 << index) - 1));
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline int rec_len(gab_value rec) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return popcount(GAB_VAL_TO_REC(rec)->mask);
  case kGAB_RECORDNODE:
    return popcount(GAB_VAL_TO_RECNODE(rec)->mask);
  default:
    assert(0 && "Only rec and recnodebranch can have len");
  }
}

static inline int rec_vlen(gab_value rec) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return popcount(GAB_VAL_TO_REC(rec)->vmask);
  case kGAB_RECORDNODE:
    return popcount(GAB_VAL_TO_RECNODE(rec)->vmask);
  default:
    assert(0 && "Only rec and recnodebranch can have len");
  }
}

static inline gab_value *rec_nodeleafat(gab_value rec, size_t p) {
  size_t idx = 2 * p;
  // assert(idx < rec_len(rec));
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return GAB_VAL_TO_REC(rec)->data + idx;
  case kGAB_RECORDNODE:
    return GAB_VAL_TO_RECNODE(rec)->data + idx;
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_unsetindices(gab_value rec) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

    m->mask = 0;
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);

    m->mask = 0;
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_setindex(gab_value rec, size_t idx) {
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

    m->mask |= ((size_t)1 << idx);
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);

    m->mask |= ((size_t)1 << idx);
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_setbranch(gab_value rec, size_t idx, size_t pos,
                                 gab_value v) {
  size_t offset = 2 * pos;
  // assert(idx < rec_len(rec));
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

    m->data[offset + kLEAF_KEY] = v;
    m->data[offset + kLEAF_VALUE] = gab_undefined;
    m->mask |= ((size_t)1 << idx);
    m->vmask &= ~((size_t)1 << pos);
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);

    m->data[offset + kLEAF_KEY] = v;
    m->data[offset + kLEAF_VALUE] = gab_undefined;
    m->mask |= ((size_t)1 << idx);
    m->vmask &= ~((size_t)1 << pos);
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

// This does unshifting and cleans up the mask.
static inline void rec_delleaf(gab_value rec, size_t idx, size_t pos) {
  size_t offset = 2 * pos;
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset), m->data + (offset) + 2,
            sizeof(gab_value) * (2 * (nrows - pos - 1)));

    size_t above_vmask = m->vmask >> 1 & ~(pos - 1);
    size_t below_vmask = (m->vmask & (pos - 1));
    m->vmask = above_vmask | below_vmask;
    m->mask &= ~((size_t)1 << idx);

    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);

    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset), m->data + (offset) + 2,
            sizeof(gab_value) * (2 * (nrows - pos - 1)));

    size_t above_vmask = m->vmask >> 1 & (pos - 1);
    size_t below_vmask = (m->vmask & (pos - 2));
    m->vmask = above_vmask | below_vmask;
    m->mask &= ~((size_t)1 << idx);

    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_setleaf(gab_value rec, size_t pos, gab_value v) {
  size_t offset = 2 * pos + 1;
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);
    m->data[offset] = v;
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);
    m->data[offset] = v;
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_shiftvalues(gab_value rec, size_t pos) {
  size_t offset = 2 * pos;
  size_t ones_below = ((size_t)1 << pos) - 1;
  size_t ones_above = ~ones_below << 1;
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);
    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset) + 2, m->data + (offset),
            sizeof(gab_value) * (2 * (nrows - pos)));

    size_t above_vmask = m->vmask << 1 & ones_above;
    size_t below_vmask = m->vmask & ones_below;
    m->vmask = above_vmask | below_vmask;
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);
    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset) + 2, m->data + (offset),
            sizeof(gab_value) * (2 * (nrows - pos)));

    size_t above_vmask = m->vmask << 1 & ones_above;
    size_t below_vmask = m->vmask & ones_below;
    m->vmask = above_vmask | below_vmask;
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline void rec_setshpwith(struct gab_triple gab, gab_value rec, gab_value key) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec* r = GAB_VAL_TO_REC(rec);
  r->shape = gab_shpwith(gab, r->shape, key);
}

static inline void rec_insertleaf(gab_value rec, size_t idx, size_t pos,
                                  gab_value k, gab_value v) {
  size_t offset = 2 * pos;
  // assert(idx < rec_len(rec) * 2);
  switch (gab_valkind(rec)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);
    assert(!(m->mask & ((size_t)1 << idx)));
    assert(!(m->vmask & ((size_t)1 << pos)));

    m->data[offset] = k;
    m->data[offset + 1] = v;
    m->mask |= ((size_t)1 << idx);
    m->vmask |= ((size_t)1 << pos);
    break;
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *m = GAB_VAL_TO_RECNODE(rec);
    assert(!(m->mask & ((size_t)1 << idx)));
    assert(!(m->vmask & ((size_t)1 << pos)));

    m->data[offset] = k;
    m->data[offset + 1] = v;
    m->mask |= ((size_t)1 << idx);
    m->vmask |= ((size_t)1 << pos);
    break;
  }
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

static inline gab_value rec_nodebranchat(gab_value rec, int pos) {
  int offset = 2 * pos;
  assert(offset < (rec_len(rec) * 2));
  switch (gab_valkind(rec)) {
  case kGAB_RECORD:
    return GAB_VAL_TO_REC(rec)->data[offset];
  case kGAB_RECORDNODE:
    return GAB_VAL_TO_RECNODE(rec)->data[offset];
  default:
    assert(0 && "Only rec and recnodebranch can have indexes");
  }
}

gab_value reccpy(struct gab_triple gab, gab_value m, size_t space) {
  switch (gab_valkind(m)) {
  case kGAB_RECORD: {
    struct gab_obj_rec *n = GAB_VAL_TO_REC(m);
    size_t size = popcount(n->mask);

    struct gab_obj_rec *nm =
        GAB_VAL_TO_REC(__gab_record(gab, size, space, n->data));

    nm->mask = n->mask;
    nm->vmask = n->vmask;
    nm->shape = n->shape;

    return __gab_obj(nm);
  }
  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *n = GAB_VAL_TO_RECNODE(m);
    size_t size = popcount(n->mask);

    struct gab_obj_recnode *nm =
        GAB_VAL_TO_RECNODE(__gab_recordnode(gab, size, space, n->data));

    nm->mask = n->mask;
    nm->vmask = n->vmask;

    return __gab_obj(nm);
  }
  default:
    assert(0 && "Only rec and recnodebranch cpy");
  }
}

gab_value gab_recput(struct gab_triple gab, gab_value rec, gab_value key,
                     gab_value val) {
  size_t path_pos, path_idx, shift = 0;

  size_t idx = hash_index(key, shift);

  gab_value root = reccpy(gab, rec, !rec_hasindex(rec, idx));

  gab_value path = gab_undefined;

  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = rec_posat(rec, idx);

    if (!rec_hasindex(rec, idx)) {
      if (path == gab_undefined) {
        rec_shiftvalues(root, pos);
        rec_insertleaf(root, idx, pos, key, val);
      } else {
        gab_value n = reccpy(gab, rec, 1);
        rec_shiftvalues(n, pos);
        rec_insertleaf(n, idx, pos, key, val);
        rec_setbranch(path, path_idx, path_pos, n);
      }

      rec_setshpwith(gab, root, key);
      return root;
    }

    switch (rec_nodekat(rec, pos)) {
    case kLEAF: {
      gab_value *kv = rec_nodeleafat(rec, pos);

      if (kv[kLEAF_KEY] == key) {
        if (path == gab_undefined) {
          rec_setleaf(root, pos, val);
        } else {
          gab_value n = reccpy(gab, rec, 0);
          rec_setleaf(n, pos, val);
          rec_setbranch(path, path_idx, path_pos, n);
        }

        return root;
      }

      gab_value cpy = path == gab_undefined ? root : reccpy(gab, rec, 0);
      if (path != gab_undefined) {
        rec_setbranch(path, path_idx, path_pos, cpy);
      }

      path = cpy;
      path_idx = idx;
      path_pos = pos;

      shift += GAB_HAMT_BITS;
      size_t hash_idx_existing = hash_index(kv[kLEAF_KEY], shift),
             hash_idx_new = hash_index(key, shift);

      while (hash_idx_existing == hash_idx_new) {

        gab_value intermediate = __gab_recordnode(gab, 0, 1, nullptr);
        rec_setbranch(path, path_idx, path_pos, intermediate);

        path = intermediate;
        path_idx = hash_idx_new;
        path_pos = 0;

        shift = shift_next(shift);
        assert((shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE) && "UH OH REHASH here");

        hash_idx_existing = hash_index(kv[kLEAF_KEY], shift);
        hash_idx_new = hash_index(key, shift);
      }

      assert(hash_idx_new != hash_idx_existing);

      gab_value n = __gab_recordnode(gab, 0, 2, nullptr);
      rec_setbranch(path, path_idx, path_pos, n);

      // Manually set indices before getting positions
      rec_setindex(n, hash_idx_existing);
      rec_setindex(n, hash_idx_new);
      size_t pos_new = rec_posat(n, hash_idx_new);
      size_t pos_existing = rec_posat(n, hash_idx_existing);

      assert(pos_new != pos_existing);
      rec_unsetindices(n);

      rec_insertleaf(n, hash_idx_new, pos_new, key, val);
      rec_insertleaf(n, hash_idx_existing, pos_existing, kv[kLEAF_KEY],
                     kv[kLEAF_VALUE]);

      rec_setshpwith(gab, root, key);
      return root;
    }
    case kBRANCH:
      if (path == gab_undefined) {
        path = root;
        rec = reccpy(gab, rec_nodebranchat(rec, pos), 0);
      } else {
        assert(rec_mask(path) & ((size_t)1 << path_idx));
        gab_value nm = reccpy(gab, rec_nodebranchat(rec, pos), 0);
        rec_setbranch(path, path_idx, path_pos, rec);
        path = rec;
        rec = nm;
      }

      path_idx = idx;
      path_pos = pos;

      shift = shift_next(shift);
      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }

  assert(false && "unreachable");
  return root;
}

gab_value gab_recdel(struct gab_triple gab, gab_value rec, gab_value key) {
  assert(gab_valkind(rec) == kGAB_RECORD);

  // There is nothing to remove - return the original rec
  if (gab_recat(rec, key) == gab_undefined)
    return rec;

  size_t path_pos, path_idx, shift = 0;

  size_t idx = hash_index(key, shift);

  gab_value root = reccpy(gab, rec, !rec_hasindex(rec, idx));

  gab_value path = gab_undefined;

  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = rec_posat(rec, idx);

    switch (rec_nodekat(rec, pos)) {
    case kLEAF: {
      gab_value *kv = rec_nodeleafat(rec, pos);

      assert(rec != path);

      if (kv[kLEAF_KEY] == key) {
        if (path == gab_undefined) {
          rec_delleaf(root, idx, pos);
        } else {
          gab_value n = reccpy(gab, rec, 0);
          rec_delleaf(n, idx, pos);
          rec_setbranch(path, path_idx, path_pos, n);
        }

        return root;
      }

      assert(false);
    }
    case kBRANCH:
      if (path == gab_undefined) {
        path = root;
        rec = reccpy(gab, rec_nodebranchat(rec, pos), 0);
      } else {
        assert(rec_mask(path) & ((size_t)1 << path_idx));
        gab_value nm = reccpy(gab, rec_nodebranchat(rec, pos), 0);
        rec_setbranch(path, path_idx, path_pos, rec);
        path = rec;
        rec = nm;
      }

      path_idx = idx;
      path_pos = pos;

      shift = shift_next(shift);
      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }
}

gab_value gab_srecat(struct gab_triple gab, gab_value rec, const char *key) {
  return gab_recat(rec, gab_string(gab, key));
}

gab_value gab_recat(gab_value rec, gab_value key) {
  assert(gab_valkind(rec) == kGAB_RECORD);

  size_t shift = 0;
  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = rec_posat(rec, idx);

    if (!rec_hasindex(rec, idx))
      return gab_undefined;

    switch (rec_nodekat(rec, pos)) {
    case kLEAF: {
      gab_value *kv = rec_nodeleafat(rec, pos);

      return kv[kLEAF_KEY] == key ? kv[kLEAF_VALUE] : gab_undefined;
    }
    case kBRANCH:
      shift = shift_next(shift);

      rec = rec_nodebranchat(rec, pos);

      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }
}


gab_value gab_ukrecat(gab_value rec, size_t idx) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  assert(gab_urechas(rec, idx));

  return gab_ushpat(gab_recshp(rec), idx);
}

gab_value gab_uvrecat(gab_value rec, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  assert(gab_urechas(rec, i));

  return gab_recat(rec, gab_ushpat(gab_recshp(rec), i));
}

bool gab_rechas(gab_value rec, gab_value key) {
  return gab_recat(rec, key) != gab_undefined;
}

bool gab_urechas(gab_value rec, size_t i) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

  return i < gab_shplen(m->shape);
}

size_t gab_reclen(gab_value rec) {
  assert(gab_valkind(rec) == kGAB_RECORD);
  struct gab_obj_rec *m = GAB_VAL_TO_REC(rec);

  return gab_shplen(m->shape);
}
