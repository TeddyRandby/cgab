#include "include/gab.h"
#include "include/engine.h"
#include "include/object.h"

gab_value gab_bundle(gab_engine *gab, u64 size, s_i8 keys[size],
                     gab_value values[size]) {
  gab_value value_keys[size];

  for (u64 i = 0; i < size; i++) {
    value_keys[i] = GAB_VAL_OBJ(gab_obj_string_create(gab, keys[i]));
  }

  gab_obj_shape *bundle_shape = gab_obj_shape_create(gab, value_keys, size, 1);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_object_create(gab, bundle_shape, values, size, 1));

  return bundle;
}

gab_value gab_bundle_array(gab_engine *gab, u64 size, gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_array(gab, size);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_object_create(gab, bundle_shape, values, size, 1));

  return bundle;
}

void gab_bind(gab_engine *gab, u64 size, s_i8 keys[size],
              gab_value values[size]) {

  gab_dref(gab, gab->std);

  gab->std = gab_bundle(gab, size, keys, values);
}

gab_result gab_run(gab_engine *gab, s_i8 module_name, s_i8 source, u8 flags) {

  gab_result compile_result = gab_bc_compile(gab, module_name, source, flags);

  if (!gab_result_ok(compile_result)) {
    return compile_result;
  }

  gab_result run_result = gab_vm_run(gab, gab_result_value(compile_result));

  return run_result;
}
