#include "gab.h"
#include "compiler/object.h"

gab_engine *gab_create() { return gab_engine_create(); }

void gab_destroy(gab_engine *gab) { gab_engine_destroy(gab); }

gab_value gab_bundle(gab_engine *gab, u64 size, gab_lib_kvp kvps[size]) {
  gab_value keys[size], values[size];

  for (u64 i = 0; i < size; i++) {
    keys[i] = GAB_VAL_OBJ(
        gab_obj_string_create(gab, s_u8_ref_create_cstr(kvps[i].key)));
    values[i] = kvps[i].value;
  }

  gab_obj_shape *bundle_shape = gab_obj_shape_create(gab, keys, size, 1);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_object_create(gab, bundle_shape, values, size, 1));

  return bundle;
}

void gab_bind_library(gab_engine *gab, u64 size, gab_lib_kvp kvps[size]) {

  if (GAB_VAL_IS_OBJ(gab->std)) {
    gab_obj_destroy(GAB_VAL_TO_OBJ(gab->std), gab);
  }

  gab_value std = gab_bundle(gab, size, kvps);

  gab->std = std;
}

gab_result *gab_run_source(gab_engine *gab, const char *source,
                           const char *name, u8 flags) {

  gab_result *compile_result = gab_engine_compile(
      gab, s_u8_create_cstr(source), s_u8_ref_create_cstr(name), flags);

  if (gab_result_has_error(compile_result)) {
    return compile_result;
  }

  gab_vm vm;

  gab_result *run_result = gab_engine_run(gab, &vm, compile_result->as.main);

  gab_result_destroy(compile_result);

  return run_result;
}
