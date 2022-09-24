#include "../gab/gab.h"
#include "src/gab/value.h"

#include <uriparser/Uri.h>
#include <llhttp.h>


gab_value gab_lib_parse(gab_engine* eng, gab_value* argv, u8 argc) {
    if (argc != 1 || !GAB_VAL_IS_STRING(argv[0])) {
        return GAB_VAL_NULL();
    }

    gab_obj_string *request = GAB_VAL_TO_OBJ(argv[0]);

    static llhttp_t parser;
    static llhttp_settings_t settings;

    llhttp_init(&parser, HTTP_BOTH, &settings);

    i32 err = llhttp_execute(&parser, (const char*) request->data, request->size);

    if (err != HPE_OK) {
        return GAB_VAL_NULL();
    }


}

gab_value gab_mod(gab_engine *gab) {
  gab_lib_kvp url_kvps[] = {};
  return gab_bundle_kvps(gab, GAB_KVP_BUNDLESIZE(url_kvps), url_kvps);
}
