#include "include/gab.h"
#include <uriparser/Uri.h>
#include <uriparser/UriBase.h>

void gab_lib_stouri(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                    size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_vmpush(vm, gab_string(gab, "invalid_arguments"));
    return;
  }

  struct gab_obj_string *uri = GAB_VAL_TO_STRING(argv[0]);

  gab_value r_values[] = {
      gab_string(gab, "ok"),
      gab_nil,
      gab_nil,
  };

  const char *errorPos = NULL;
  UriUriA parsed_uri;

  const char *start = (char *)uri->data;
  const char *after_end = start + uri->len;

  int result = uriParseSingleUriExA(&parsed_uri, start, after_end, &errorPos);

  if (result != URI_SUCCESS) {
    gab_vmpush(vm, gab_string(gab, "parse_failed"));
    return;
  }

  UriPathSegmentA *path = parsed_uri.pathHead;

  if (path) {
    uint64_t path_count = 1;

    while (path->next) {
      path = path->next;
      path_count++;
    }

    path = parsed_uri.pathHead;

    gab_value values[path_count];

    uint64_t index = 0;
    while (index < path_count) {
      values[index] = gab_nstring(gab, path->text.afterLast - path->text.first,
                                  path->text.first);
      path = path->next;
      index++;
    }

    r_values[1] = gab_tuple(gab, path_count, values);
  }

  UriQueryListA *query = NULL;
  int item_count = 0;

  result = uriDissectQueryMallocA(&query, &item_count, parsed_uri.query.first,
                                  parsed_uri.query.afterLast);

  if (result != URI_SUCCESS) {
    gab_vmpush(vm, gab_string(gab, "parse_failed"));
    return;
  }

  UriQueryListA *walker = query;
  gab_value values[item_count];
  const char *keys[item_count];
  uint8_t index = 0;

  while (index < item_count) {
    const char *key = walker->key;
    gab_value value = gab_string(gab, walker->value);

    keys[index] = key;
    values[index] = value;

    walker = walker->next;
    index++;
  }

  r_values[2] = gab_srecord(gab, item_count, keys, values);

  uriFreeQueryListA(query);

  uriFreeUriMembersA(&parsed_uri);

  gab_nvmpush(vm, sizeof(r_values) / sizeof(r_values[0]), r_values);
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "to_uri",
  };

  gab_value receivers[] = {
      gab_typ(gab, kGAB_STRING),
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "to_uri", gab_lib_stouri),
  };

  for (int32_t i = 0; i < sizeof(specs) / sizeof(gab_value); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return a_gab_value_one(gab_nil);
}
