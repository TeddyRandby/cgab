#include "include/gab.h"

#define K gab_value
#define V gab_value
#define DEF_V gab_undefined
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

void map_destroy(void *data) {
  d_gab_value *self = data;
  d_gab_value_destroy(self);
  DESTROY(self);
}

void map_visit(struct gab_triple gab, gab_gcvisit_f v, void *data) {
  d_gab_value *map = data;
  for (uint64_t i = 0; i < map->cap; i++) {
    if (d_gab_value_iexists(map, i)) {
      gab_value key = d_gab_value_ikey(map, i);
      gab_value val = d_gab_value_ival(map, i);
      if (gab_valiso(key))
        v(gab, gab_valtoo(key));
      if (gab_valiso(val))
        v(gab, gab_valtoo(val));
    }
  }
}

gab_value map_at(gab_value self, gab_value key) {
  return d_gab_value_read(gab_boxdata(self), key);
}

bool map_has(gab_value self, gab_value key) {
  return d_gab_value_exists(gab_boxdata(self), key);
}

gab_value map_put(gab_value self, gab_value key, gab_value value) {
  d_gab_value_insert(gab_boxdata(self), key, value);
  return value;
}

gab_value map_create(struct gab_triple gab, size_t len, size_t stride,
                     gab_value keys[len], gab_value values[len]) {
  d_gab_value *data = NEW(d_gab_value);
  d_gab_value_create(data, 8);

  gab_value self = gab_box(gab, (struct gab_box_argt){
                                       .type = gab_string(gab, "map"),
                                       .destructor = map_destroy,
                                       .visitor = map_visit,
                                       .data = data,
                                   });

  for (size_t i = 0; i < len; i++) {
    d_gab_value_insert(data, keys[i * stride], values[i * stride]);
  }

  return self;
}
