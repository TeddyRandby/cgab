#include "include/gab.h"
#include <gab/gab.h>
#include <llhttp.h>
#include <stdio.h>

typedef struct {
  s_char url;
  s_char body;
  v_s_char header_fields;
  v_s_char header_values;
} userdata;

int handle_on_url(llhttp_t *parser, const char *data, size_t len) {
  userdata *ud = parser->data;
  ud->url = (s_char){.len = len, .data = data};
  return HPE_OK;
}

int handle_on_body(llhttp_t *parser, const char *data, size_t len) {
  userdata *ud = parser->data;
  ud->body = (s_char){.len = len, .data = data};
  return HPE_OK;
}

int handle_on_header_field(llhttp_t *parser, const char *data, size_t len) {
  userdata *ud = parser->data;
  v_s_char_push(&ud->header_fields, (s_char){
                                        .len = len,
                                        .data = data,
                                    });
  return HPE_OK;
}

int handle_on_header_value(llhttp_t *parser, const char *data, size_t len) {
  userdata *ud = parser->data;
  v_s_char_push(&ud->header_values, (s_char){
                                        .len = len,
                                        .data = data,
                                    });
  return HPE_OK;
}

void gab_lib_parse(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_vmpush(vm, gab_string(gab, "invalid_arguments"));
    return;
  }

  struct gab_obj_string *req = GAB_VAL_TO_STRING(argv[0]);

  llhttp_t parser;
  llhttp_settings_t settings;

  userdata ud = {0};

  /*Initialize user callbacks and settings */

  llhttp_settings_init(&settings);

  settings.on_url = handle_on_url;
  settings.on_body = handle_on_body;
  settings.on_header_field = handle_on_header_field;
  settings.on_header_value = handle_on_header_value;

  llhttp_init(&parser, HTTP_BOTH, &settings);

  parser.data = &ud;

  enum llhttp_errno err = llhttp_execute(&parser, (char *)req->data, req->len);

  if (err == HPE_OK) {
    size_t header_count = ud.header_fields.len;
    gab_value header_fields[header_count];
    gab_value header_values[header_count];

    for (size_t i = 0; i < header_count; i++) {
      header_fields[i] = gab_nstring(gab, ud.header_fields.data[i].len,
                                     (char *)ud.header_fields.data[i].data);

      header_values[i] = gab_nstring(gab, ud.header_values.data[i].len,
                                     (char *)ud.header_values.data[i].data);
    }

    gab_value result[] = {
        gab_string(gab, "ok"),
        gab_string(gab, llhttp_method_name(parser.method)),
        gab_nstring(gab, ud.url.len, (char *)ud.url.data),
        gab_record(gab, header_count, header_fields, header_values),
        ud.body.len > 0 ? gab_nstring(gab, ud.body.len, (char *)ud.body.data)
                        : gab_nil,
    };

    gab_nvmpush(vm, sizeof(result) / sizeof(result[0]), result);
  } else {
    gab_vmpush(vm, gab_string(gab, llhttp_errno_name(err)));
  }
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {

  const char *names[] = {
      "to_http",
  };

  gab_value types[] = {
      gab_typ(gab, kGAB_STRING),
  };

  gab_value values[] = {
      gab_builtin(gab, "to_http", gab_lib_parse),
  };

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = types[i],
                      .specialization = values[i],
                  });
  }

  gab_ngciref(gab, gc, vm, 1, sizeof(types) / sizeof(types[0]), types);

  return a_gab_value_one(gab_nil);
}
