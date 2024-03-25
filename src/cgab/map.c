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
  return (hash >> shift) & GAB_HAMT_IDXMASK;
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

enum node_k {
  kBRANCH = 0,
  kLEAF = 1,
};

static inline enum node_k map_nodekat(gab_value map, size_t idx) {
  assert(idx < GAB_HAMT_SIZE);

  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->vmask & (1 << idx);
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->vmask & (1 << idx);
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
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline gab_value *map_nodeleafat(gab_value map, size_t p) {
  int idx = 2 * p;
  assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->data + idx;
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->data + idx;
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void mapnode_pput(gab_value map, size_t p, gab_value v) {
  int idx = 2 * p + 1;
  assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    GAB_VAL_TO_MAP(map)->data[idx] = v;
    break;
  case kGAB_MAPNODE:
    GAB_VAL_TO_MAPNODE(map)->data[idx] = v;
    break;
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline void mapnode_pleafput(gab_value map, size_t p, gab_value v) {
  int idx = 2 * p + 1;
  assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    GAB_VAL_TO_MAP(map)->data[idx] = v;
    break;
  case kGAB_MAPNODE:
    GAB_VAL_TO_MAPNODE(map)->data[idx] = v;
    break;
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

static inline gab_value map_nodebranchat(gab_value map, int p) {
  int idx = 2 * p;
  assert(idx < map_len(map));
  switch (gab_valkind(map)) {
  case kGAB_MAP:
    return GAB_VAL_TO_MAP(map)->data[idx];
  case kGAB_MAPNODE:
    return GAB_VAL_TO_MAPNODE(map)->data[idx];
  default:
    assert(0 && "Only map and mapnodebranch can have indexes");
  }
}

gab_value map_set(gab_value m, gab_value k, gab_value v) {}

gab_value mapnode_cpy(struct gab_triple gab, gab_value m, size_t space) {
  assert(gab_valkind(m) == kGAB_MAPNODE);
  struct gab_obj_mapnode *n = GAB_VAL_TO_MAPNODE(m);
  int size = popcount(n->mask);
  return __gab_mapnode(gab, size, space, n->data);
}

gab_value mapcpy(struct gab_triple gab, gab_value m) {
  assert(gab_valkind(m) == kGAB_MAP);
  struct gab_obj_map *n = GAB_VAL_TO_MAP(m);
  int size = popcount(n->mask);
  return __gab_mapnode(gab, 1, size, n->data);
}

gab_value gab_matput(struct gab_triple gab, gab_value map, gab_value key,
                     gab_value val) {
  size_t shift = 0, path_pos = 0;
  gab_value root = mapcpy(gab, map);
  gab_value path = gab_undefined;

  for (;;) {
    size_t idx = hash_index(key, shift);

    size_t pos = map_posat(map, idx);

    if (!map_hasindex(map, idx)) {
      // TODO: Patch the path along the way
      gab_value n = mapnode_cpy(gab, map, 1);

      mapnode_pleafput(n, pos, val);
      mapnode_pput(path, path_pos, n);

      return root;
    }

    switch (map_nodekat(map, pos)) {
    case kLEAF: {
      gab_value *kv = map_nodeleafat(map, pos);

      if (kv[0] == key) {
        // TODO: Patch the path along the way
        gab_value n = mapnode_cpy(gab, map, 0);

        mapnode_pleafput(n, pos, val);
        mapnode_pput(path, path_pos, n);

        return root;
      }

      // TODO: The keys did not match, so split this leaf node out into a branch
      // node.
      gab_value cpy = mapnode_cpy(gab, map, 0);
      gab_value n = __gab_mapnode(gab, 0, 2, nullptr);

      mapnode_pput(cpy, n, pos);
      mapnode_pput(path, path_pos, cpy);

      size_t idx = hash_index(key, shift);
      size_t pos = map_posat(n, idx);
      gab_value *nkv = map_nodeleafat(n, pos);
      nkv[0] = key;
      nkv[1] = val;

      idx = hash_index(kv[0], shift);
      pos = map_posat(n, idx);
      nkv = map_nodeleafat(n, pos);
      nkv[0] = kv[0];
      nkv[1] = kv[1];

      return root;
    }
    case kBRANCH:
      // Continue the search down the branch.
      // Copy the path we take, patching it up
      // in the new root
      shift += GAB_HAMT_BITS;

      map = mapnode_cpy(gab, map_nodebranchat(map, pos), 0);

      if (path != gab_undefined)
        mapnode_pput(path, map, pos);

      path = map;
      path_pos = pos;

      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }

  return root;
}

gab_value gab_mapat(gab_value map, gab_value key) {
  size_t hash = key, shift = 0;
  for (;;) {
    size_t idx = hash_index(hash, shift);

    if (!map_hasindex(map, idx))
      return gab_undefined;

    size_t pos = map_posat(map, idx);

    switch (map_nodekat(map, pos)) {
    case kLEAF: {
      gab_value *kv = map_nodeleafat(map, pos);

      return kv[0] == key ? kv[1] : gab_undefined;
    }
    case kBRANCH:
      shift += GAB_HAMT_BITS;

      map = map_nodebranchat(map, pos);

      assert(shift + GAB_HAMT_BITS <= GAB_HAMT_SIZE && "UH OH REHASH here");
      break;
    default:
      assert(false && "invalid nodek");
    }
  }
}

gab_value gab_mapput(struct gab_triple gab, gab_value vec, gab_value key,
                     gab_value val);

void gab_mmapput(struct gab_triple gab, gab_value vec, gab_value key,
                 gab_value val);
