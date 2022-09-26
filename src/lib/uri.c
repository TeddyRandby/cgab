#include "../gab/gab.h"
#include "src/core/core.h"
#include "src/gab/object.h"
#include "src/gab/value.h"

#include <uriparser/Uri.h>
#include <llhttp.h>


gab_value gab_lib_parse(gab_engine* eng, gab_value* argv, u8 argc) {
    if (argc != 1 || !GAB_VAL_IS_STRING(argv[0])) {
        return GAB_VAL_NULL();
    }

    gab_obj_string *request = GAB_VAL_TO_STRING(argv[0]);

    static llhttp_t parser;
    static llhttp_settings_t settings;

    llhttp_init(&parser, HTTP_BOTH, &settings);

    i32 err = llhttp_execute(&parser, (const char*) request->data, request->size);

    if (err != HPE_OK) {
        return GAB_VAL_NULL();
    }

}

gab_value gab_mod(gab_engine *gab) {
    s_i8 keys[] = {
        s_i8_cstr("parse"),
    };

    gab_value values[] = {
        GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_parse, "parse", 1)),
    };

    return gab_bundle(gab, 1, keys, values);
}
