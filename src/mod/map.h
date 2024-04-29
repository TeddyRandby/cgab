#include "gab.h"

#define K gab_value
#define V gab_value
#define DEF_V gab_undefined
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

void map_destroy(size_t len, char data[static len]) {
  d_gab_value_destroy((void *)data);
}

void map_visit(struct gab_triple gab, gab_gcvisit_f v, size_t len,
               char data[static len]) {
  d_gab_value *map = (void *)data;
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

size_t map_len(gab_value self) {
  return ((d_gab_value *)gab_boxdata(self))->len;
}

gab_value map_put(struct gab_triple gab, gab_value self, gab_value key,
                  gab_value value) {
  d_gab_value *data = gab_boxdata(self);

  if (!gab_valisnew(self)) {
    if (d_gab_value_exists(data, key)) {
      gab_dref(gab, d_gab_value_read(data, key));
    } else {
      gab_dref(gab, key);
    }

    gab_iref(gab, value);
  }

  d_gab_value_insert(data, key, value);
  return value;
}

gab_value map_create(struct gab_triple gab, size_t len, size_t stride,
                     gab_value keys[len], gab_value values[len]) {
  d_gab_value d = {0};

  for (size_t i = 0; i < len; i++) {
    gab_niref(gab, stride, len, keys);
    gab_niref(gab, stride, len, values);

    d_gab_value_insert(&d, keys[i * stride], values[i * stride]);
  }

  gab_value self = gab_box(gab, (struct gab_box_argt){
                                    .type = gab_string(gab, "gab.map"),
                                    .destructor = map_destroy,
                                    .visitor = map_visit,
                                    .data = &d,
                                    .size = sizeof(d),
                                });

  return self;
}
