#ifndef BLUF_OBJECT_H
#define BLUF_OBJECT_H

#include "../../common/common.h"
#include "nanbox.h"

static inline f64 value_to_f64(gab_value value) { return *(f64 *)(&value); }
static inline gab_value f64_to_value(f64 value) {
  return *(gab_value *)(&value);
}

/* Because of NaN tagging, identical bit-representations are guaranteed
  to be identical values.
*/
static inline boolean gab_val_equal(gab_value self, gab_value other) {
  return self == other;
}

static inline u64 gab_val_hash(gab_value value) { return value; }

void gab_val_dump(gab_value self);

typedef struct gab_gc gab_gc;

/*
  An enumeration of the heap-allocated value types in gab.
*/
typedef enum gab_obj_kind {
  OBJECT_STRING,
  OBJECT_FUNCTION,
  OBJECT_BUILTIN,
  OBJECT_CLOSURE,
  OBJECT_UPVALUE,
  OBJECT_OBJECT,
  OBJECT_SHAPE,
} gab_obj_kind;

/*
  This header appears at the front of all gab_obj_kind structs.

  This allows a kind of polymorphism, casting between a
  gab_obj* and the kind of pointer denoted by gab_obj->kind

  I need some sort of prototype. A pointer to a dict that
  objects can share.
*/
typedef struct gab_obj gab_obj;
struct gab_obj {
  gab_obj_kind kind;
  u8 flags;
  i32 references;
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

#if GAB_CONCURRENT_GC
#define GAB_OBJ_FLAG_RED 64
#define GAB_OBJ_FLAG_ORANGE 128
#endif

#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_IS_BLACK(obj) ((obj)->flags & GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_IS_GRAY(obj) ((obj)->flags & GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_IS_WHITE(obj) ((obj)->flags & GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_IS_PURPLE(obj) ((obj)->flags & GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_IS_GREEN(obj) ((obj)->flags & GAB_OBJ_FLAG_GREEN)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_BLACK(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_GRAY(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_WHITE(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_PURPLE(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_GREEN(obj) ((obj)->flags = 0 | GAB_OBJ_FLAG_GREEN)

#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_BUFFERED)
#define GAB_OBJ_NOT_BLACK(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_BLACK)
#define GAB_OBJ_NOT_GRAY(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_GRAY)
#define GAB_OBJ_NOT_WHITE(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_WHITE)
#define GAB_OBJ_NOT_PURPLE(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_PURPLE)
#define GAB_OBJ_NOT_GREEN(obj) ((obj)->flags &= ~GAB_OBJ_FLAG_GREEN)

typedef struct gab_engine gab_engine;

/*
  'Generic' functions which handle all the different kinds of gab objects.
*/
void gab_obj_destroy(gab_obj *self, gab_engine *eng);

/*
  Defined in common/log.c
*/
void gab_obj_dump(gab_value value);

static inline boolean gab_val_is_obj_kind(gab_value self, gab_obj_kind k) {
  return GAB_VAL_IS_OBJ(self) && GAB_VAL_TO_OBJ(self)->kind == k;
}

/*
  ------------- OBJ_STRING -------------
  A sequence of chars. Interned by the module.
*/
typedef struct gab_obj_string gab_obj_string;
struct gab_obj_string {
  gab_obj header;

  /*
    The pre-computed hash of the string.
  */
  u64 hash;
  u64 size;
  u8 data[FLEXIBLE_ARRAY];
};
#define GAB_VAL_IS_STRING(value) (gab_val_is_obj_kind(value, OBJECT_STRING))
#define GAB_VAL_TO_STRING(value) ((gab_obj_string *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_STRING(value) ((gab_obj_string *)value)

gab_obj_string *gab_obj_string_create(gab_engine *eng, s_u8_ref data);
gab_obj_string *gab_obj_string_concat(gab_engine *eng, gab_obj_string *a,
                                      gab_obj_string *b);
s_u8_ref gab_obj_string_get_ref(gab_obj_string *self);
gab_obj_string *gab_obj_to_obj_string(gab_obj *self, gab_engine *engine);
gab_obj_string *gab_val_to_obj_string(gab_value self, gab_engine *eng);

/*
  ------------- OBJ_BUILTIN-------------
  A function pointer. to a native c function.
*/
typedef gab_value (*gab_builtin)(u8 argc, gab_value *argv, gab_engine *eng,
                                 char **err);
typedef struct gab_obj_builtin gab_obj_builtin;
struct gab_obj_builtin {
  gab_obj header;
  gab_builtin function;
  s_u8_ref name;
};

#define GAB_VAL_IS_BUILTIN(value) (gab_val_is_obj_kind(value, OBJECT_BUILTIN))
#define GAB_VAL_TO_BUILTIN(value) ((gab_obj_builtin *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BUILTIN(value) ((gab_obj_builtin *)value)
gab_obj_builtin *gab_obj_builtin_create(gab_engine *eng, gab_builtin function,
                                        const char *name);

/*
  ------------- OBJ_FUNCTION -------------
  A function. Not visible at runtime - always wrapped by an OBJ_CLOSURE
*/
typedef struct gab_obj_function gab_obj_function;
struct gab_obj_function {
  gab_obj header;
  s_u8_ref name;

  /*
    The number of arguments the function takes.
  */
  u8 narguments;

  /*
    The number of upvalues the function captures.
  */
  u8 nupvalues;

  /*
    The number of local variables in the function.
  */
  u8 nlocals;

  /*
    The offset into the module where the function's instructions begin.

    This can't be a pointer because modules can be reallocated as their
    vector grows - and this would leave functions with dangling pointers.
  */
  u64 offset;

  /*
    The module where the function is defined.
  */
  gab_module *module;
};

#define GAB_VAL_IS_FUNCTION(value) (gab_val_is_obj_kind(value, OBJECT_FUNCTION))
#define GAB_VAL_TO_FUNCTION(value) ((gab_obj_function *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_FUNCTION(value) ((gab_obj_function *)value)
gab_obj_function *gab_obj_function_create(gab_engine *eng, s_u8_ref name);

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

#define GAB_VAL_IS_UPVALUE(value) (gab_val_is_obj_kind(value, OBJECT_UPVALUE))
#define GAB_VAL_TO_UPVALUE(value) ((gab_obj_upvalue *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_UPVALUE(value) ((gab_obj_upvalue *)value)
gab_obj_upvalue *gab_obj_upvalue_create(gab_engine *eng, gab_value *slot);

/*
  ------------- OBJ_CLOSURE-------------
  The wrapper to OBJ_FUNCTION, which is actually called at runtime.
*/
typedef struct gab_obj_closure gab_obj_closure;
struct gab_obj_closure {
  gab_obj header;
  /*
    The function being wrapped.
  */
  gab_obj_function *func;
  /*
    An array of upvalue pointers - since multiple closures can capture
    the same upvalue.
  */
  gab_obj_upvalue *upvalues[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_CLOSURE(value) (gab_val_is_obj_kind(value, OBJECT_CLOSURE))
#define GAB_VAL_TO_CLOSURE(value) ((gab_obj_closure *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_CLOSURE(value) ((gab_obj_closure *)value)
gab_obj_closure *gab_obj_closure_create(gab_engine *eng, gab_obj_function *fn,
                                        gab_obj_upvalue *upvs[]);

/*
  ------------- OBJ_SHAPE-------------
  A javascript object, or a python dictionary, or a lua table.
  Known by many names.
*/
typedef struct gab_obj_shape gab_obj_shape;
struct gab_obj_shape {
  gab_obj header;

  u64 hash;

  d_u64 properties;

  gab_value keys[FLEXIBLE_ARRAY];
};
#define GAB_VAL_IS_SHAPE(value) (gab_val_is_obj_kind(value, OBJECT_SHAPE))
#define GAB_VAL_TO_SHAPE(value) ((gab_obj_shape *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_SHAPE(value) ((gab_obj_shape *)value)

gab_obj_shape *gab_obj_shape_create(gab_engine *eng, gab_value values[],
                                    u64 size, u64 stride);

gab_obj_shape *gab_obj_shape_extend(gab_obj_shape *self, gab_engine *eng,
                                    gab_value property);
static inline i64 gab_obj_shape_find(gab_obj_shape *self, gab_value key) {
  u64 offset = d_u64_read(&self->properties, gab_val_hash(key), key);

  if (GAB_VAL_IS_NULL(offset)) {
    return -1;
  }

  return offset;
};

/*
  ------------- OBJ_OBJECT-------------
  A javascript object, or a python dictionary, or a lua table.
  Known by many names.
*/
typedef struct gab_obj_object gab_obj_object;
struct gab_obj_object {
  gab_obj header;
  /*
    The shape of this object.
  */
  gab_obj_shape *shape;

  boolean is_dynamic;
  /*
    The number of properties.
  */
  u8 static_size;

  v_u64 dynamic_values;
  /*
    The object's properties.
  */
  gab_value static_values[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_OBJECT(value) (gab_val_is_obj_kind(value, OBJECT_OBJECT))
#define GAB_VAL_TO_OBJECT(value) ((gab_obj_object *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_OBJECT(value) ((gab_obj_object *)value)
gab_obj_object *gab_obj_object_create(gab_engine *eng, gab_obj_shape *shape,
                                      gab_value values[], u64 size, u64 stride);

i16 gab_obj_object_extend(gab_obj_object *self, gab_engine *eng,
                          gab_obj_shape *new_shape, gab_value value);

static inline void gab_obj_object_set(gab_obj_object *self, i16 offset,
                                      gab_value value) {

  if (self->is_dynamic)
    v_u64_set(&self->dynamic_values, offset, value);

  else
    self->static_values[offset] = value;
}

static inline gab_value gab_obj_object_get(gab_obj_object *self, i16 offset) {
  if (offset < 0)
    return GAB_VAL_NULL();

  else if (self->is_dynamic)
    return v_u64_val_at(&self->dynamic_values, offset);

  else
    return self->static_values[offset];
}

/*
  The exception here is when interning needs to be done.

  This means that the hash depends on the actual value of the object.

  For example, strings hash based on their content.

  This allows the rest of hashing and comparison to be based on the
  bit-representation.
*/
static inline u64 gab_val_intern_hash(gab_value value) {
  if (GAB_VAL_IS_STRING(value))
    return GAB_VAL_TO_STRING(value)->hash;

  if (GAB_VAL_IS_SHAPE(value))
    return GAB_VAL_TO_SHAPE(value)->hash;

  return value;
}

/*
  A value is false if it is null or the boolean false.
*/
static inline boolean gab_val_falsey(gab_value self) {
  return GAB_VAL_IS_NULL(self) || GAB_VAL_IS_FALSE(self);
}
#endif
