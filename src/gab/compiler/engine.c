#include "engine.h"
#include "../vm/vm.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

gab_engine *gab_create() {
  gab_engine *self = NEW(gab_engine);

  self->constants = NEW(d_gab_value);
  d_gab_value_create(self->constants, MODULE_CONSTANTS_MAX);

  self->imports = NEW(d_s_i8);
  d_s_i8_create(self->imports, MODULE_CONSTANTS_MAX);

  self->modules = NULL;
  self->std = GAB_VAL_NULL();

  gab_bc_create(&self->bc);
  gab_vm_create(&self->vm);
  gab_gc_create(&self->gc);

  self->owning = true;

  pthread_mutexattr_t attr;
  pthread_mutex_init(&self->lock, &attr);

  return self;
}

gab_engine *gab_create_fork(gab_engine *parent) {
  gab_engine *self = NEW(gab_engine);

  self->constants = parent->constants;
  self->modules = parent->modules;
  self->imports = parent->imports;
  self->std = parent->std;

  self->gc = (gab_gc){0};
  d_u64_create(&self->gc.roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->gc.queue, MODULE_CONSTANTS_MAX);

  self->vm = (gab_vm){0};

  self->owning = false;

  pthread_mutexattr_t attr;
  pthread_mutex_init(&self->lock, &attr);
  return self;
};

void gab_destroy(gab_engine *self) {
  if (self->owning) {
    // Walk the linked list of modules and clean them up.
    while (self->modules != NULL) {
      gab_module *next = self->modules->next;
      gab_module_destroy(self->modules);
      self->modules = next;
    }

    for (i32 i = 0; i < self->imports->cap; i++) {
      if (d_s_i8_iexists(self->imports, i)) {
        gab_import *import = (gab_import *)d_s_i8_ival(self->imports, i);
        gab_import_destroy(import);
      }
    }

    for (u64 i = 0; i < self->constants->cap; i++) {
      if (d_gab_value_iexists(self->constants, i)) {
        gab_value v = d_gab_value_ikey(self->constants, i);
        gab_engine_val_dref(self, v);
      }
    }

    gab_engine_val_dref(self, self->std);
  }

  gab_engine_collect(self);

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);

  pthread_mutex_destroy(&self->lock);

  if (self->owning) {
    d_gab_value_destroy(self->constants);
    d_s_i8_destroy(self->imports);
  }

  DESTROY(self);
}

gab_import *gab_import_shared(void *shared, gab_value result) {
  gab_import *self = NEW(gab_import);

  self->k = IMPORT_SHARED;
  self->shared = shared;
  self->cache = result;
  return self;
}

gab_import *gab_import_source(a_i8 *source, gab_value result) {
  gab_import *self = NEW(gab_import);

  self->k = IMPORT_SOURCE;
  self->source = source;
  self->cache = result;
  return self;
}

void gab_import_destroy(gab_import *self) {
  switch (self->k) {
  case IMPORT_SHARED:
    dlclose(self->shared);
    break;
  case IMPORT_SOURCE:
    a_i8_destroy(self->source);
    break;
  }
  DESTROY(self);
  return;
}

gab_module *gab_engine_add_module(gab_engine *self, s_i8 name, s_i8 source) {

  gab_module *module = NEW(gab_module);

  gab_module_create(module, name, source);

  module->engine = self;
  module->next = self->modules;
  self->modules = module;

  return module;
}

void gab_engine_add_import(gab_engine *self, gab_import *import, s_i8 module) {
  d_s_i8_insert(self->imports, module, import);
}

u16 gab_engine_add_constant(gab_engine *self, gab_value value) {
  if (self->constants->cap > MODULE_CONSTANTS_MAX) {
    fprintf(stderr, "Too many constants\n");
    exit(1);
  }

  d_gab_value_insert(self->constants, value, 0);

  u64 val = d_gab_value_index_of(self->constants, value);

  return val;
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_value_ikey(self->constants, index);
    d_status status = d_gab_value_istatus(self->constants, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_STRING(key)) {
      gab_obj_string *str_key = GAB_VAL_TO_STRING(key);
      if (str_key->hash == hash &&
          s_i8_match(str, gab_obj_string_ref(str_key))) {
        return str_key;
      }
    }

    index = (index + 1) & (self->constants->cap - 1);
  }
}

static inline boolean shape_matches_keys(gab_obj_shape *self,
                                         gab_value values[], u64 size,
                                         u64 stride) {

  if (self->properties.len != size) {
    return false;
  }

  for (u64 i = 0; i < size; i++) {
    gab_value key = values[i * stride];
    if (!d_u64_exists(&self->properties, key)) {
      return false;
    }
  }

  return true;
}

gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size,
                                     gab_value values[size], u64 stride,
                                     u64 hash) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_value_ikey(self->constants, index);
    d_status status = d_gab_value_istatus(self->constants, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_SHAPE(key)) {
      gab_obj_shape *shape_key = GAB_VAL_TO_SHAPE(key);
      if (shape_key->hash == hash &&
          shape_matches_keys(shape_key, values, size, stride)) {
        return shape_key;
      }
    }

    index = (index + 1) & (self->constants->cap - 1);
  }
}

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

void dump_compile_error(gab_bc *compiler, const char *msg) {
  gab_bc_frame *frame = &compiler->frames[compiler->frame_count - 1];

  s_i8 curr_token = compiler->lex.previous_token_src;

  u64 curr_src_index = compiler->line - 1;

  s_i8 curr_src;
  if (curr_src_index < compiler->lex.source_lines->len) {
    curr_src = v_s_i8_val_at(compiler->lex.source_lines, curr_src_index);
  } else {
    curr_src = s_i8_cstr("");
  }

  s_i8 func_name = frame->locals_name[0];

  a_i8 *curr_under = a_i8_empty(curr_src.len);

  i8 *tok_start, *tok_end;

  tok_start = curr_token.data;
  tok_end = curr_token.data + curr_token.len;

  const char *tok = gab_token_names[compiler->previous_token];

  for (u8 i = 0; i < curr_under->len; i++) {
    if (curr_src.data + i >= tok_start && curr_src.data + i < tok_end)
      curr_under->data[i] = '^';
    else
      curr_under->data[i] = ' ';
  }

  const char *prev_color = ANSI_COLOR_GREEN;
  const char *curr_color = ANSI_COLOR_RED;

  const char *curr_box = "\u256d";

  fprintf(stderr,
          "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET "] line " ANSI_COLOR_RED
          "%04lu" ANSI_COLOR_RESET ": Error near " ANSI_COLOR_BLUE
          "%s" ANSI_COLOR_RESET "\n\t%s%s %.4lu " ANSI_COLOR_RESET "%.*s"
          "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
          "\n\t\u2570\u2500> ",
          (i32)func_name.len, func_name.data, compiler->line, tok, curr_box,
          curr_color, curr_src_index + 1, (i32)curr_src.len, curr_src.data,
          (i32)curr_under->len, curr_under->data);

  a_i8_destroy(curr_under);

  fprintf(stderr, ANSI_COLOR_YELLOW "%s.\n" ANSI_COLOR_RESET, msg);
}

void dump_run_error(gab_vm *vm, const char *msg) {

  gab_call_frame *frame = vm->call_stack + 1;

  while (frame <= vm->frame) {
    gab_module *mod = frame->closure->func->module;

    s_i8 func_name = frame->closure->func->name;

    u64 offset = frame->ip - mod->bytecode.data - 1;

    u64 curr_row = v_u64_val_at(&mod->lines, offset);

    // Row numbers start at one, so the index is one less.
    s_i8 curr_src = v_s_i8_val_at(mod->source_lines, curr_row - 1);

    const char *tok = gab_token_names[v_u8_val_at(&mod->tokens, offset)];

    if (frame == vm->frame) {
      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET
              "] line: " ANSI_COLOR_RED "%04lu" ANSI_COLOR_RESET
              " Error near " ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET
              "\n\t\u256d " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
              "\n\t\u2502"
              "\n\t\u2570\u2500> ",
              (i32)func_name.len, func_name.data, curr_row, tok, curr_row,
              (i32)curr_src.len, curr_src.data);

      fprintf(stderr, ANSI_COLOR_YELLOW "%s.\n" ANSI_COLOR_RESET, msg);
    } else {
      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET
              "] line: " ANSI_COLOR_RED "%04lu" ANSI_COLOR_RESET " Called from"
              "\n\t  " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
              "\n\t"
              "\n",
              (i32)func_name.len, func_name.data, curr_row, curr_row,
              (i32)curr_src.len, curr_src.data);
    }

    frame++;
  }
}

boolean gab_result_has_error(gab_result *self) {
  return self->type == RESULT_COMPILE_FAIL || self->type == RESULT_RUN_FAIL;
};

void gab_result_dump_error(gab_result *self) {
  if (self->type == RESULT_COMPILE_FAIL) {
    dump_compile_error(self->as.compile_fail.compiler,
                       self->as.compile_fail.msg);
  } else if (self->type == RESULT_RUN_FAIL) {
    dump_run_error(self->as.run_fail.vm, self->as.run_fail.msg);
  }
};

gab_result *gab_compile_fail(gab_bc *compiler, const char *msg) {
  gab_result *self = NEW(gab_result);
  self->type = RESULT_COMPILE_FAIL;

  // Copy the compiler's state at the point of the error.
  self->as.compile_fail.compiler = NEW(gab_bc);
  memcpy(self->as.compile_fail.compiler, compiler, sizeof(gab_bc));

  self->as.compile_fail.msg = msg;

  return self;
};

gab_result *gab_compile_success(gab_obj_closure *main) {
  gab_result *self = NEW(gab_result);
  self->type = RESULT_COMPILE_SUCCESS;
  self->as.main = main;
  return self;
};

gab_result *gab_run_fail(gab_vm *vm, const char *msg) {
  gab_result *self = NEW(gab_result);
  self->type = RESULT_RUN_FAIL;

  self->as.run_fail.vm = NEW(gab_vm);

  memcpy(self->as.run_fail.vm, vm, sizeof(gab_vm));

  self->as.run_fail.vm->frame =
      self->as.run_fail.vm->call_stack + (vm->frame - vm->call_stack);

  self->as.run_fail.msg = msg;
  return self;
};

gab_result *gab_run_success(gab_value data) {
  gab_result *self = NEW(gab_result);

  self->type = RESULT_RUN_SUCCESS;
  self->as.result = data;
  return self;
};

void gab_result_destroy(gab_result *self) {
  switch (self->type) {
  case RESULT_RUN_FAIL:
    DESTROY(self->as.run_fail.vm);
    break;
  case RESULT_COMPILE_FAIL:
    DESTROY(self->as.compile_fail.compiler);
    break;
  default:
    break;
  }

  DESTROY(self);
};
