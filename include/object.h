#ifndef GAB_OBJECT_H
#define GAB_OBJECT_H

#include "value.h"
#include <stdio.h>

typedef struct gab_module gab_module;
typedef struct gab_gc gab_gc;
typedef struct gab_vm gab_vm;
typedef struct gab_engine gab_engine;

typedef enum gab_kind {
  GAB_KIND_EFFECT,
  GAB_KIND_STRING,
  GAB_KIND_MESSAGE,
  GAB_KIND_PROTOTYPE,
  GAB_KIND_BUILTIN,
  GAB_KIND_BLOCK,
  GAB_KIND_UPVALUE,
  GAB_KIND_SYMBOL,
  GAB_KIND_CONTAINER,
  GAB_KIND_RECORD,
  GAB_KIND_SHAPE,
  GAB_KIND_LIST,
  GAB_KIND_MAP,
  GAB_KIND_NIL,
  GAB_KIND_UNDEFINED,
  GAB_KIND_NUMBER,
  GAB_KIND_BOOLEAN,
  GAB_KIND_NKINDS,
} gab_kind;

static const char *gab_kind_names[] = {
    "effect",  "string", "message",   "prototype", "builtin", "block",
    "upvalue", "value",  "container", "record",    "shape",   "list",
    "map",     "nil",    "undefined", "number",    "boolean", "nkinds",
};

/*
  This header appears at the front of all gab_obj_kind structs.

  This allows a kind of polymorphism, casting between a
  gab_obj* and the kind of pointer denoted by gab_obj->kind

  I need some sort of prototype. A pointer to a dict that
  objects can share.
*/
typedef struct gab_obj gab_obj;
struct gab_obj {
  gab_obj *next;
  i32 references;
  gab_kind kind;
  u8 flags;
};

/*
  These macros are used to toggle flags in objects for RC garbage collection.

  Sol uses a pure RC garbage collection approach.

  The algorithm is described in this paper:
  https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon03Pure.pdf
*/
#define GAB_OBJ_FLAG_BUFFERED 1
#define GAB_OBJ_FLAG_BLACK 2
#define GAB_OBJ_FLAG_GRAY 4
#define GAB_OBJ_FLAG_WHITE 8
#define GAB_OBJ_FLAG_PURPLE 16
#define GAB_OBJ_FLAG_GREEN 32

#define GAB_OBJ_FLAG_GARBAGE 64

#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_IS_BLACK(obj) ((obj)->flags & GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_IS_GRAY(obj) ((obj)->flags & GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_IS_WHITE(obj) ((obj)->flags & GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_IS_PURPLE(obj) ((obj)->flags & GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_IS_GREEN(obj) ((obj)->flags & GAB_OBJ_FLAG_GREEN)
#define GAB_OBJ_IS_GARBAGE(obj) ((obj)->flags & GAB_OBJ_FLAG_GARBAGE)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_BLACK(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_GRAY(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_WHITE(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_PURPLE(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_GREEN(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_GREEN)
#define GAB_OBJ_GARBAGE(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_GARBAGE)

#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_NOT_BLACK(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_NOT_GRAY(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_NOT_WHITE(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_NOT_PURPLE(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_NOT_GREEN(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_GREEN)

/*
  'Generic' functions which handle all the different kinds of gab objects.
*/
void gab_obj_destroy(gab_engine *gab, gab_vm *vm, gab_obj *self);
u64 gab_obj_size(gab_obj *self);

static inline boolean gab_val_is_obj_kind(gab_value self, gab_kind k) {
  return GAB_VAL_IS_OBJ(self) && GAB_VAL_TO_OBJ(self)->kind == k;
}

/*
  ------------- OBJ_STRING -------------
  A sequence of chars. Interned by the module.
*/
typedef struct gab_obj_string gab_obj_string;
struct gab_obj_string {
  gab_obj header;

  u64 hash;

  u64 len;

  i8 data[FLEXIBLE_ARRAY];
};
#define GAB_VAL_IS_STRING(value) (gab_val_is_obj_kind(value, GAB_KIND_STRING))
#define GAB_VAL_TO_STRING(value) ((gab_obj_string *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_STRING(value) ((gab_obj_string *)value)

gab_obj_string *gab_obj_string_create(gab_engine *gab, s_i8 data);

gab_obj_string *gab_obj_string_concat(gab_engine *gab, gab_obj_string *a,
                                      gab_obj_string *b);

s_i8 gab_obj_string_ref(gab_obj_string *self);

/*
  ------------- OBJ_BUILTIN-------------
  A function pointer. to a native c function.
*/
typedef gab_value (*gab_builtin)(gab_engine *gab, gab_vm *vm, u8 argc,
                                 gab_value argv[argc]);
typedef struct gab_obj_builtin gab_obj_builtin;
struct gab_obj_builtin {
  gab_obj header;

  gab_builtin function;

  s_i8 name;
};

#define GAB_VAL_IS_BUILTIN(value) (gab_val_is_obj_kind(value, GAB_KIND_BUILTIN))
#define GAB_VAL_TO_BUILTIN(value) ((gab_obj_builtin *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BUILTIN(value) ((gab_obj_builtin *)value)

gab_obj_builtin *gab_obj_builtin_create(gab_engine *gab, gab_builtin function,
                                        s_i8 name);

/*
  ------------- OBJ_UPVALUE -------------
  A local variable, captured by a closure.
  When the local goes out of scope, its data is moved into the upvalue.
*/
typedef struct gab_obj_upvalue gab_obj_upvalue;
struct gab_obj_upvalue {
  gab_obj header;

  /*
    The pointer to the upvalues data.
    While the upvalue is open, this points into the stack at the local's slot.
    While the upvalue is closed, this points at it's own closed value.
  */
  gab_value *data;

  /*
    The value of the upvalue once it is closed.
  */
  gab_value closed;

  /*
    An intrusive list of upvalues, used by the vm to close upvalues while
    leaving scopes and popping call frames.
  */
  gab_obj_upvalue *next;
};

#define GAB_VAL_IS_UPVALUE(value) (gab_val_is_obj_kind(value, GAB_KIND_UPVALUE))
#define GAB_VAL_TO_UPVALUE(value) ((gab_obj_upvalue *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_UPVALUE(value) ((gab_obj_upvalue *)value)

gab_obj_upvalue *gab_obj_upvalue_create(gab_engine *gab, gab_value *slot);

/*
  ------------- OBJ_PROTOTYPE -------------
*/
typedef struct gab_obj_prototype gab_obj_prototype;
struct gab_obj_prototype {
  gab_obj header;

  s_i8 name;

  /*
   * The number of arguments the function takes.
   */
  u8 narguments;

  /*
   * The number of upvalues the function captures.
   */
  u8 nupvalues;

  /*
   * The number of slots the proto needs.
   */
  u16 nslots;

  /*
   * The number of locals
   */
  u8 nlocals;

  /*
   */
  u8 var;

  /*
   * The offset into the module where the function's instructions begin.
   * This can't be a pointer because modules can be reallocated as their
   * vector grows - and this would leave functions with dangling pointers.
   */
  u64 offset;
  u64 len;

  /*
   * The module owning the function.
   */
  gab_module *mod;
};

#define GAB_VAL_IS_PROTOTYPE(value)                                            \
  (gab_val_is_obj_kind(value, GAB_KIND_PROTOTYPE))
#define GAB_VAL_TO_PROTOTYPE(value) ((gab_obj_prototype *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_PROTOTYPE(value) ((gab_obj_prototype *)value)

gab_obj_prototype *gab_obj_prototype_create(gab_engine *gab, gab_module *mod,
                                            s_i8 name, u8 narguments,
                                            u16 nslots, u8 nupvalues,
                                            u8 nlocals, u64 offset, u64 len,
                                            boolean var);

/*
  ------------- OBJ_CLOSURE-------------
  The wrapper to OBJ_FUNCTION, which is actually called at runtime.
*/
typedef struct gab_obj_block gab_obj_block;
struct gab_obj_block {
  gab_obj header;

  u8 nupvalues;

  gab_obj_prototype *p;

  /*
   * The array of captured upvalues
   */
  gab_value upvalues[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_BLOCK(value) (gab_val_is_obj_kind(value, GAB_KIND_BLOCK))
#define GAB_VAL_TO_BLOCK(value) ((gab_obj_block *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BLOCK(value) ((gab_obj_block *)value)

gab_obj_block *gab_obj_block_create(gab_engine *gab, gab_obj_prototype *p);

/*
  ------------- OBJ_FUNCTION -------------
  A function. Not visible at runtime - always wrapped by an OBJ_CLOSURE
*/
typedef struct gab_obj_message gab_obj_message;

#define NAME specs
#define K gab_value
#define V gab_value
#define DEF_V GAB_VAL_NIL()
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

struct gab_obj_message {
  gab_obj header;

  u8 version;

  /*
   * The name of the function
   */
  s_i8 name;
  u64 hash;

  /*
   * The map of specializations defined for this function.
   */
  d_specs specs;
};

#define GAB_VAL_IS_MESSAGE(value) (gab_val_is_obj_kind(value, GAB_KIND_MESSAGE))
#define GAB_VAL_TO_MESSAGE(value) ((gab_obj_message *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_MESSAGE(value) ((gab_obj_message *)value)

gab_obj_message *gab_obj_message_create(gab_engine *gab, s_i8 name);

static inline u64 gab_obj_message_find(gab_obj_message *self,
                                       gab_value receiver) {
  if (!d_specs_exists(&self->specs, receiver))
    return UINT16_MAX;

  return d_specs_index_of(&self->specs, receiver);
}

static inline void gab_obj_message_set(gab_obj_message *self, u16 offset,
                                       gab_value rec, gab_value spec) {
  d_specs_iset_key(&self->specs, offset, rec);
  d_specs_iset_val(&self->specs, offset, spec);
  self->version++;
}

static inline gab_value gab_obj_message_get(gab_obj_message *self, u64 offset) {
  return d_specs_ival(&self->specs, offset);
}

static inline void gab_obj_message_insert(gab_obj_message *self,
                                          gab_value receiver,
                                          gab_value specialization) {
  d_specs_insert(&self->specs, receiver, specialization);
  self->version++;
}

static inline gab_value gab_obj_message_read(gab_obj_message *self,
                                             gab_value receiver) {
  return d_specs_read(&self->specs, receiver);
}

/*
  ------------- OBJ_SHAPE-------------
  A javascript object, or a python dictionary, or a lua table.
  Known by many names.
*/
typedef struct gab_obj_shape gab_obj_shape;
struct gab_obj_shape {
  gab_obj header;

  s_i8 name;

  u64 hash;

  u64 len;

  gab_value data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_SHAPE(value) (gab_val_is_obj_kind(value, GAB_KIND_SHAPE))
#define GAB_VAL_TO_SHAPE(value) ((gab_obj_shape *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_SHAPE(value) ((gab_obj_shape *)value)

gab_obj_shape *gab_obj_shape_create(gab_engine *gab, gab_vm *vm, u64 len,
                                    u64 stride, gab_value key[len]);

gab_obj_shape *gab_obj_shape_create_tuple(gab_engine *gab, gab_vm *vm, u64 len);

gab_obj_shape *gab_obj_shape_grow(gab_engine *gab, gab_vm *vm,
                                  gab_obj_shape *self, gab_value property);

static inline u16 gab_obj_shape_find(gab_obj_shape *self, gab_value key) {

  for (u64 i = 0; i < self->len; i++) {
    assert(i < UINT16_MAX);

    if (self->data[i] == key)
      return i;
  }

  return UINT16_MAX;
};

/*
 *------------- OBJ_RECORD -------------
 */
typedef struct gab_obj_record gab_obj_record;
struct gab_obj_record {
  gab_obj header;
  /*
    The shape of this object.
  */
  gab_obj_shape *shape;

  u64 len;

  gab_value data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_RECORD(value) (gab_val_is_obj_kind(value, GAB_KIND_RECORD))
#define GAB_VAL_TO_RECORD(value) ((gab_obj_record *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_RECORD(value) ((gab_obj_record *)value)

gab_obj_record *gab_obj_record_create(gab_engine *gab, gab_obj_shape *shape,
                                      u64 size, u64 stride,
                                      gab_value values[size]);

static inline void gab_obj_record_set(gab_obj_record *self, u16 offset,
                                      gab_value value) {
  assert(offset < self->len);
  self->data[offset] = value;
}

static inline gab_value gab_obj_record_get(gab_obj_record *self, u16 offset) {
  assert(offset < self->len);
  return self->data[offset];
}

static inline boolean gab_obj_record_put(gab_obj_record *self, gab_value key,
                                         gab_value value) {
  u16 prop_offset = gab_obj_shape_find(self->shape, key);

  if (prop_offset == UINT16_MAX) {
    return false;
  }

  gab_obj_record_set(self, prop_offset, value);

  return true;
}

static inline gab_value gab_obj_record_at(gab_obj_record *self,
                                          gab_value prop) {
  u16 prop_offset = gab_obj_shape_find(self->shape, prop);

  if (prop_offset >= self->len)
    return GAB_VAL_NIL();

  return gab_obj_record_get(self, prop_offset);
}

/*
 *------------- OBJ_LIST -------------
 */
#define T gab_value
#include "vector.h"

typedef struct gab_obj_list gab_obj_list;
struct gab_obj_list {
  gab_obj header;

  v_gab_value data;
};

#define GAB_VAL_IS_LIST(value) (gab_val_is_obj_kind(value, GAB_KIND_LIST))
#define GAB_VAL_TO_LIST(value) ((gab_obj_list *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_LIST(value) ((gab_obj_list *)value)

gab_obj_list *gab_obj_list_create(gab_engine *gab, u64 size, u64 stride,
                                  gab_value values[size]);
gab_obj_list *gab_obj_list_create_empty(gab_engine *gab, u64 size);

static inline gab_value gab_obj_list_put(gab_obj_list *self, u64 offset,
                                         gab_value value) {
  if (offset >= self->data.len) {
    u64 nils = offset - self->data.len;

    while (nils-- > 0) {
      v_gab_value_push(&self->data, GAB_VAL_NIL());
    }

    v_gab_value_push(&self->data, value);

    return value;
  }

  v_gab_value_set(&self->data, offset, value);

  return value;
}

static inline gab_value gab_obj_list_at(gab_obj_list *self, u64 offset) {
  if (offset >= self->data.len)
    return GAB_VAL_NIL();

  return v_gab_value_val_at(&self->data, offset);
}

/*
 *------------- OBJ_MAP -------------
 */

#define K gab_value
#define V gab_value
#define DEF_V GAB_VAL_NIL()
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "dict.h"

typedef struct gab_obj_map gab_obj_map;
struct gab_obj_map {
  gab_obj header;

  d_gab_value data;
};

#define GAB_VAL_IS_MAP(value) (gab_val_is_obj_kind(value, GAB_KIND_MAP))
#define GAB_VAL_TO_MAP(value) ((gab_obj_map *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_MAP(value) ((gab_obj_map *)value)

gab_obj_map *gab_obj_map_create(gab_engine *gab, u64 len, u64 stride,
                                gab_value keys[len], gab_value values[len]);

static inline gab_value gab_obj_map_put(gab_obj_map *self, gab_value key,
                                        gab_value value) {

  d_gab_value_insert(&self->data, key, value);

  return value;
}

static inline gab_value gab_obj_map_at(gab_obj_map *self, gab_value key) {
  return d_gab_value_read(&self->data, key);
}

/*
  ------------- OBJ_CONTAINER-------------
  A container to some unknown data.
*/
typedef struct gab_obj_container gab_obj_container;
typedef void (*gab_obj_container_cb)(gab_engine *gab, gab_vm *vm, void *data);
struct gab_obj_container {
  gab_obj header;

  /* The pointer owned by this object */
  void *data;

  /* A callback to clean up the owned data */
  gab_obj_container_cb destructor;
};

#define GAB_VAL_IS_CONTAINER(value)                                            \
  (gab_val_is_obj_kind(value, GAB_KIND_CONTAINER))
#define GAB_VAL_TO_CONTAINER(value) ((gab_obj_container *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_CONTAINER(value) ((gab_obj_container *)value)

gab_obj_container *gab_obj_container_create(gab_engine *gab,
                                            gab_obj_container_cb destructor,
                                            void *data);

/*
  ------------- OBJ_SYMBOL-------------
  A unique symbol.
*/
typedef struct gab_obj_symbol gab_obj_symbol;
struct gab_obj_symbol {
  gab_obj header;
  s_i8 name;
};

#define GAB_VAL_IS_SYMBOL(value) (gab_val_is_obj_kind(value, GAB_KIND_SYMBOL))
#define GAB_VAL_TO_SYMBOL(value) ((gab_obj_symbol *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_SYMBOL(value) ((gab_obj_symbol *)value)

gab_obj_symbol *gab_obj_symbol_create(gab_engine *gab, s_i8 name);

/*
  ------------- OBJ_EFFECT -------------
  A suspended call that can be handled.
*/
typedef struct gab_obj_effect gab_obj_effect;
struct gab_obj_effect {
  gab_obj header;

  // Closure
  gab_obj_block *c;

  // Instruction Pointer
  u64 offset;

  // The number of arguments yielded
  u8 have;

  // The number of values wanted
  u8 want;

  // Size of the stack frame
  u8 len;

  // Stack frame
  gab_value frame[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_EFFECT(value) (gab_val_is_obj_kind(value, GAB_KIND_EFFECT))
#define GAB_VAL_TO_EFFECT(value) ((gab_obj_effect *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_EFFECT(value) ((gab_obj_effect *)value)

gab_obj_effect *gab_obj_effect_create(gab_engine *gab, gab_obj_block *c,
                                      u64 offset, u8 arity, u8 want, u8 len,
                                      gab_value frame[len]);

#endif
