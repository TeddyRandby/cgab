#include "core.h"
#include "gab.h"

/*
 * Gab Maps are persistent HAMTs. These are a tried-and-true immutable data
 * structure used in multiple other languages - Elixir, Gleam, and Clojure to
 * name a few.
 *
 * This implemetnation is *HEAVILY* inspired by
 * https://github.com/mkirchner/hamt
 */
enum map_result {
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

static inline bool map_hasindex(gab_value map, size_t idx) {
  assert(idx < GAB_HAMT_SIZE);

  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->mask & (1 << idx);
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->mask & (1 << idx);
  default:
    assert(0 && "Only map and mapnode can have indexes");
  }
}

static inline size_t map_mask(gab_value map) {
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->mask;
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->mask;
  default:
    assert(0 && "Only map and mapnode can have indexes");
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

static inline enum node_k map_nodekat(gab_value map, size_t pos) {
  assert(pos < GAB_HAMT_SIZE);

  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return (GAB_VAL_TO_MAP(map)->vmask & (1 << pos)) ? kLEAF : kBRANCH;
  case kGAB_MAPNODE:
    return (GAB_VAL_TO_MAPNODE(map)->vmask & (1 << pos)) ? kLEAF : kBRANCH;
  default:
    assert(0 && "INVALID NODE K");
  }
}

#define popcount(n) __builtin_popcount(n)

static inline size_t map_posat(gab_value map, size_t index) {
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return popcount(GAB_VAL_TO_MAP(map)->mask & ((1 << index) - 1));
  case kGAB_MAPNODE:
    return popcount(GAB_VAL_TO_MAPNODE(map)->mask & ((1 << index) - 1));
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline int map_len(gab_value map) {
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return popcount(GAB_VAL_TO_MAP(map)->mask);
  case kGAB_MAPNODE:
    return popcount(GAB_VAL_TO_MAPNODE(map)->mask);
  default:
    assert(0 && "Only map and mapnodebranch can have len");
  }
}

static inline int map_vlen(gab_value map) {
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return popcount(GAB_VAL_TO_MAP(map)->vmask);
  case kGAB_MAPNODE:
    return popcount(GAB_VAL_TO_MAPNODE(map)->vmask);
  default:
    assert(0 && "Only map and mapnodebranch can have len");
  }
}

static inline gab_value *map_nodeleafat(gab_value map, size_t p) {
  size_t idx = 2 * p;
  // assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->data + idx;
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->data + idx;
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_unsetindices(gab_value map) {
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

    m->mask = 0;
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);

    m->mask = 0;
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_setindex(gab_value map, size_t idx) {
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

    m->mask |= (1 << idx);
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);

    m->mask |= (1 << idx);
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_setbranch(gab_value map, size_t idx, size_t pos,
                                 gab_value v) {
  size_t offset = 2 * pos;
  // assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

    m->data[offset + kLEAF_KEY] = v;
    m->data[offset + kLEAF_VALUE] = gab_undefined;
    m->mask |= (1 << idx);
    m->vmask &= ~(1 << pos);
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);

    m->data[offset + kLEAF_KEY] = v;
    m->data[offset + kLEAF_VALUE] = gab_undefined;
    m->mask |= (1 << idx);
    m->vmask &= ~(1 << pos);
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

// This does unshifting and cleans up the mask.
static inline void map_delleaf(gab_value map, size_t idx, size_t pos) {
  size_t offset = 2 * pos;
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset), m->data + (offset) + 2,
            sizeof(gab_value) * (2 * (nrows - pos - 1)));

    size_t above_vmask = m->vmask >> 1 & ~(pos - 1);
    size_t below_vmask = (m->vmask & (pos - 1));
    m->vmask = above_vmask | below_vmask;
    m->mask &= ~(1 << idx);

    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);

    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset), m->data + (offset) + 2,
            sizeof(gab_value) * (2 * (nrows - pos - 1)));

    size_t above_vmask = m->vmask >> 1 & (pos - 1);
    size_t below_vmask = (m->vmask & (pos - 2));
    m->vmask = above_vmask | below_vmask;
    m->mask &= ~(1 << idx);

    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_setleaf(gab_value map, size_t pos, gab_value v) {
  size_t offset = 2 * pos + 1;
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);
    m->data[offset] = v;
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);
    m->data[offset] = v;
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_shiftvalues(gab_value map, size_t pos) {
  size_t offset = 2 * pos;
  size_t ones_below = (1 << pos) - 1;
  size_t ones_above = ~ones_below << 1;
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);
    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset) + 2, m->data + (offset),
            sizeof(gab_value) * (2 * (nrows - pos)));

    size_t above_vmask = m->vmask << 1 & ones_above;
    size_t below_vmask = m->vmask & ones_below;
    m->vmask = above_vmask | below_vmask;
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);
    size_t nrows = popcount(m->mask);

    memmove(m->data + (offset) + 2, m->data + (offset),
            sizeof(gab_value) * (2 * (nrows - pos)));

    size_t above_vmask = m->vmask << 1 & ones_above;
    size_t below_vmask = m->vmask & ones_below;
    m->vmask = above_vmask | below_vmask;
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void map_incrementlen(gab_value map) {
  assert(gab_valkind(map) == kGAB_MAP);
  struct gab_obj_map *m = GAB_VAL_TO_MAP(map);
  m->len++;
}

static inline void map_insertleaf(gab_value map, size_t idx, size_t pos,
                                  gab_value k, gab_value v) {
  size_t offset = 2 * pos;
  // assert(idx < map_len(map) * 2);
  switch (gab_valkind(map)) {
  case kGAB_MAP: {
    struct gab_obj_map *m = GAB_VAL_TO_MAP(map);
    assert(!(m->mask & (1 << idx)));
    assert(!(m->vmask & (1 << pos)));

    m->data[offset] = k;
    m->data[offset + 1] = v;
    m->mask |= (1 << idx);
    m->vmask |= (1 << pos);
    break;
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *m = GAB_VAL_TO_MAPNODE(map);
    assert(!(m->mask & (1 << idx)));
    assert(!(m->vmask & (1 << pos)));

    m->data[offset] = k;
    m->data[offset + 1] = v;
    m->mask |= (1 << idx);
    m->vmask |= (1 << pos);
    break;
  }
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline gab_value map_nodebranchat(gab_value map, int pos) {
  int offset = 2 * pos;
  assert(offset < (map_len(map) * 2));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->data[offset];
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->data[offset];
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline int map_vsublen(gab_value map) {
  size_t sum = map_vlen(map);

  for (size_t i = 0; i < map_len(map); i++) {
    if (map_nodekat(map, i) == kBRANCH) {
      gab_value submap = map_nodebranchat(map, i);
      sum += map_vsublen(submap);
    }
  }

  return sum;
}

gab_value mapcpy(struct gab_triple gab, gab_value m, size_t space) {
  switch (gab_valkind(m)) {
  case kGAB_MAP: {
    struct gab_obj_map *n = GAB_VAL_TO_MAP(m);
    size_t size = popcount(n->mask);

    struct gab_obj_map *nm =
        GAB_VAL_TO_MAP(__gab_map(gab, size, space, n->data));

    nm->mask = n->mask;
    nm->vmask = n->vmask;
    nm->len = n->len;
    nm->hash = n->hash;

    return __gab_obj(nm);
  }
  case kGAB_MAPNODE: {
    struct gab_obj_mapnode *n = GAB_VAL_TO_MAPNODE(m);
    size_t size = popcount(n->mask);

    struct gab_obj_mapnode *nm =
        GAB_VAL_TO_MAPNODE(__gab_mapnode(gab, size, space, n->data));

    nm->mask = n->mask;
    nm->vmask = n->vmask;

    return __gab_obj(nm);
  }
  default:
    assert(0 && "Only map and mapnodebranch cpy");
  }
}

gab_value gab_mapput(struct gab_triple gab, gab_value map, gab_value key,
                     gab_value val) {
  size_t path_pos, path_idx, shift = 0;

  size_t idx = hash_index(key, shift);

  gab_value root = mapcpy(gab, map, !map_hasindex(map, idx));

  gab_value path = gab_undefined;

  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = map_posat(map, idx);

    if (!map_hasindex(map, idx)) {
      if (path == gab_undefined) {
        map_shiftvalues(root, pos);
        map_insertleaf(root, idx, pos, key, val);
      } else {
        gab_value n = mapcpy(gab, map, 1);
        map_shiftvalues(n, pos);
        map_insertleaf(n, idx, pos, key, val);
        map_setbranch(path, path_idx, path_pos, n);
      }

      map_incrementlen(root);
      return root;
    }

    switch (map_nodekat(map, pos)) {
    case kLEAF: {
      gab_value *kv = map_nodeleafat(map, pos);

      if (kv[kLEAF_KEY] == key) {
        if (path == gab_undefined) {
          map_setleaf(root, pos, val);
        } else {
          gab_value n = mapcpy(gab, map, 0);
          map_setleaf(n, pos, val);
          map_setbranch(path, path_idx, path_pos, n);
        }

        return root;
      }

      gab_value cpy = path == gab_undefined ? root : mapcpy(gab, map, 0);
      if (path != gab_undefined) {
        map_setbranch(path, path_idx, path_pos, cpy);
      }

      path = cpy;
      path_idx = idx;
      path_pos = pos;

      shift += GAB_HAMT_BITS;
      size_t hash_idx_existing = hash_index(kv[kLEAF_KEY], shift),
             hash_idx_new = hash_index(key, shift);

      while (hash_idx_existing == hash_idx_new) {

        gab_value intermediate = __gab_mapnode(gab, 0, 1, nullptr);
        map_setbranch(path, path_idx, path_pos, intermediate);

        path = intermediate;
        path_idx = hash_idx_new;
        path_pos = 0;

        shift = shift_next(shift);
        assert((shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE) && "UH OH REHASH here");

        hash_idx_existing = hash_index(kv[kLEAF_KEY], shift);
        hash_idx_new = hash_index(key, shift);
      }

      gab_value n = __gab_mapnode(gab, 0, 2, nullptr);
      map_setbranch(path, path_idx, path_pos, n);

      // Manually set indices before getting positions
      map_setindex(n, hash_idx_existing);
      map_setindex(n, hash_idx_new);
      size_t pos_new = map_posat(n, hash_idx_new);
      size_t pos_existing = map_posat(n, hash_idx_existing);

      assert(hash_idx_new != hash_idx_existing);
      assert(pos_new != pos_existing);
      map_unsetindices(n);

      map_insertleaf(n, hash_idx_new, pos_new, key, val);
      map_insertleaf(n, hash_idx_existing, pos_existing, kv[kLEAF_KEY],
                     kv[kLEAF_VALUE]);

      map_incrementlen(root);

      return root;
    }
    case kBRANCH:
      if (path == gab_undefined) {
        path = root;
        map = mapcpy(gab, map_nodebranchat(map, pos), 0);
      } else {
        assert(map_mask(path) & (1 << path_idx));
        gab_value nm = mapcpy(gab, map_nodebranchat(map, pos), 0);
        map_setbranch(path, path_idx, path_pos, map);
        path = map;
        map = nm;
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

gab_value gab_mapdel(struct gab_triple gab, gab_value map, gab_value key) {
  assert(gab_valkind(map) == kGAB_MAP);

  // There is nothing to remove - return the original map
  if (gab_mapat(map, key) == gab_undefined)
    return map;

  size_t path_pos, path_idx, shift = 0;

  size_t idx = hash_index(key, shift);

  gab_value root = mapcpy(gab, map, !map_hasindex(map, idx));

  gab_value path = gab_undefined;

  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = map_posat(map, idx);

    switch (map_nodekat(map, pos)) {
    case kLEAF: {
      gab_value *kv = map_nodeleafat(map, pos);

      assert(map != path);

      if (kv[kLEAF_KEY] == key) {
        if (path == gab_undefined) {
          map_delleaf(root, idx, pos);
        } else {
          gab_value n = mapcpy(gab, map, 0);
          map_delleaf(n, idx, pos);
          map_setbranch(path, path_idx, path_pos, n);
        }

        return root;
      }

      assert(false);
    }
    case kBRANCH:
      if (path == gab_undefined) {
        path = root;
        map = mapcpy(gab, map_nodebranchat(map, pos), 0);
      } else {
        assert(map_mask(path) & (1 << path_idx));
        gab_value nm = mapcpy(gab, map_nodebranchat(map, pos), 0);
        map_setbranch(path, path_idx, path_pos, map);
        path = map;
        map = nm;
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

gab_value gab_smapat(struct gab_triple gab, gab_value map, const char *key) {
  return gab_mapat(map, gab_string(gab, key));
}

gab_value gab_mapat(gab_value map, gab_value key) {
  assert(gab_valkind(map) == kGAB_MAP);

  size_t shift = 0;
  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = map_posat(map, idx);

    if (!map_hasindex(map, idx))
      return gab_undefined;

    switch (map_nodekat(map, pos)) {
    case kLEAF: {
      gab_value *kv = map_nodeleafat(map, pos);

      return kv[kLEAF_KEY] == key ? kv[kLEAF_VALUE] : gab_undefined;
    }
    case kBRANCH:
      shift = shift_next(shift);

      map = map_nodebranchat(map, pos);

      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }
}

gab_value *_umap_at(gab_value map, size_t i) {
  size_t remaining = i;
  size_t len = map_len(map);

  for (size_t p = 0; p < len; p++) {
    switch (map_nodekat(map, p)) {
    case kLEAF:
      if (remaining == 0)
        return map_nodeleafat(map, p);

      remaining -= 1;
      break;
    case kBRANCH: {
      gab_value branch = map_nodebranchat(map, p);
      size_t nvals = map_vsublen(branch);

      if (nvals > remaining)
        return _umap_at(branch, remaining);

      remaining -= nvals;
      break;
    }
    }
  }

  assert(false && "unreachable");
  return nullptr;
}

gab_value gab_ukmapat(gab_value map, size_t i) {
  assert(gab_valkind(map) == kGAB_MAP);
  assert(gab_umaphas(map, i));

  return _umap_at(map, i)[kLEAF_KEY];
}

gab_value gab_uvmapat(gab_value map, size_t i) {
  assert(gab_valkind(map) == kGAB_MAP);
  assert(gab_umaphas(map, i));

  return _umap_at(map, i)[kLEAF_VALUE];
}

bool gab_maphas(gab_value map, gab_value key) {
  return gab_mapat(map, key) != gab_undefined;
}

bool gab_umaphas(gab_value map, size_t i) {
  assert(gab_valkind(map) == kGAB_MAP);

  struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

  return i < m->len;
}

size_t gab_maplen(gab_value map) {
  assert(gab_valkind(map) == kGAB_MAP);
  struct gab_obj_map *m = GAB_VAL_TO_MAP(map);

  return m->len;
}
