#ifndef GAB_OBJECT_H
#define GAB_OBJECT_H

#include "value.h"

typedef struct gab_obj gab_obj;
typedef struct gab_obj_string gab_obj_string;
typedef struct gab_obj_block_proto gab_obj_block_proto;
typedef struct gab_obj_builtin gab_obj_builtin;
typedef struct gab_obj_block gab_obj_block;
typedef struct gab_obj_message gab_obj_message;
typedef struct gab_obj_shape gab_obj_shape;
typedef struct gab_obj_record gab_obj_record;
typedef struct gab_obj_container gab_obj_container;
typedef struct gab_obj_suspense gab_obj_suspense;

typedef struct gab_module gab_module;
typedef struct gab_engine gab_engine;
typedef struct gab_vm gab_vm;
typedef struct gab_gc gab_gc;

typedef void (*gab_gc_visitor)(gab_gc *gc, gab_obj *obj);

#define T gab_value
#include "array.h"

typedef enum gab_kind {
  kGAB_SUSPENSE,
  kGAB_STRING,
  kGAB_MESSAGE,
  kGAB_BLOCK_PROTO,
  kGAB_BUILTIN,
  kGAB_BLOCK,
  kGAB_CONTAINER,
  kGAB_RECORD,
  kGAB_SHAPE,
  kGAB_NIL,
  kGAB_UNDEFINED,
  kGAB_NUMBER,
  kGAB_BOOLEAN,
  kGAB_PRIMITIVE,
  kGAB_NKINDS,
} gab_kind;

/*
  This header appears at the front of all gab_obj_kind structs.

  This allows a kind of polymorphism, casting between a
  gab_obj* and the kind of pointer denoted by gab_obj->kind
*/
struct gab_obj {
  gab_obj *next;
  i32 references;
  gab_kind kind;
  u8 flags;
};

/*
  These macros are used to check-for and toggle flags in objects for RC garbage
  collection.

  Gab uses a purely RC garbage collection approach.

  The algorithm is described in this paper:
  https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon03Pure.pdf
*/
#define fGAB_OBJ_BUFFERED (1 << 0)
#define fGAB_OBJ_BLACK (1 << 1)
#define fGAB_OBJ_GRAY (1 << 2)
#define fGAB_OBJ_WHITE (1 << 3)
#define fGAB_OBJ_PURPLE (1 << 4)
#define fGAB_OBJ_GREEN (1 << 5)
#define fGAB_OBJ_GARBAGE (1 << 6)

#define GAB_OBJ_IS_BUFFERED(obj) ((obj)->flags & fGAB_OBJ_BUFFERED)
#define GAB_OBJ_IS_BLACK(obj) ((obj)->flags & fGAB_OBJ_BLACK)
#define GAB_OBJ_IS_GRAY(obj) ((obj)->flags & fGAB_OBJ_GRAY)
#define GAB_OBJ_IS_WHITE(obj) ((obj)->flags & fGAB_OBJ_WHITE)
#define GAB_OBJ_IS_PURPLE(obj) ((obj)->flags & fGAB_OBJ_PURPLE)
#define GAB_OBJ_IS_GREEN(obj) ((obj)->flags & fGAB_OBJ_GREEN)
#define GAB_OBJ_IS_GARBAGE(obj) ((obj)->flags & fGAB_OBJ_GARBAGE)

#define GAB_OBJ_BUFFERED(obj) ((obj)->flags |= fGAB_OBJ_BUFFERED)
#define GAB_OBJ_NOT_BUFFERED(obj) ((obj)->flags &= ~fGAB_OBJ_BUFFERED)

#define GAB_OBJ_GARBAGE(obj) ((obj)->flags |= fGAB_OBJ_GARBAGE)

#define GAB_OBJ_GREEN(obj)                                                     \
  ((obj)->flags = ((obj)->flags & fGAB_OBJ_BUFFERED) | fGAB_OBJ_GREEN)
#define GAB_OBJ_BLACK(obj)                                                     \
  ((obj)->flags = ((obj)->flags & fGAB_OBJ_BUFFERED) | fGAB_OBJ_BLACK)
#define GAB_OBJ_GRAY(obj)                                                      \
  ((obj)->flags = ((obj)->flags & fGAB_OBJ_BUFFERED) | fGAB_OBJ_GRAY)
#define GAB_OBJ_WHITE(obj)                                                     \
  ((obj)->flags = ((obj)->flags & fGAB_OBJ_BUFFERED) | fGAB_OBJ_WHITE)
#define GAB_OBJ_PURPLE(obj)                                                    \
  ((obj)->flags = ((obj)->flags & fGAB_OBJ_BUFFERED) | fGAB_OBJ_PURPLE)

/**
 * An under-the-hood allocator for objects.
 *
 * @param gab The gab engine.
 *
 * @param loc The object whose memory is being reallocated, or NULL.
 *
 * @param size The size to allocate. When size is 0, the object is freed.
 *
 * @return The allocated memory, or NULL.
 */
void *gab_obj_alloc(gab_engine *gab, gab_obj *loc, u64 size);

/**
 * Free memory owned by the object, but not the object itself.
 *
 * @param self The object whose memory should be freed.
 */
void gab_obj_destroy(gab_obj *obj);

/**
 * Get the size of the object in bytes.
 *
 * @param obj The object to calculate the size of.
 *
 * @return The size of the object in bytes.
 */
u64 gab_obj_size(gab_obj *obj);

/**
 * Check if a value is of a particular kind.
 *
 * @param val The value to check.
 *
 * @param k The kind to check for.
 *
 * @return True if the value is of the given kind, false otherwise.
 */
static inline boolean gab_val_is_obj_kind(gab_value val, gab_kind k) {
  return GAB_VAL_IS_OBJ(val) && GAB_VAL_TO_OBJ(val)->kind == k;
}

/**
 * A sequence of chars.
 */
struct gab_obj_string {
  gab_obj header;

  u64 hash;

  u64 len;

  i8 data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_STRING(value) (gab_val_is_obj_kind(value, kGAB_STRING))
#define GAB_VAL_TO_STRING(value) ((gab_obj_string *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_STRING(value) ((gab_obj_string *)value)

/**
 * Create a new string object.
 *
 * @param gab The gab engine.
 *
 * @param data The value of the string.
 *
 * @return The new string object.
 */
gab_obj_string *gab_obj_string_create(gab_engine *gab, s_i8 data);

/**
 * Concatenate two strings.
 *
 * @param gab The gab engine.
 *
 * @param a The first string.
 *
 * @param b The second string.
 *
 * @return The concatenated string.
 */
gab_obj_string *gab_obj_string_concat(gab_engine *gab, gab_obj_string *a,
                                      gab_obj_string *b);
/**
 * Get a slice referencing a string's data.
 *
 * @param self The string get a reference to.
 *
 * @param The reference.
 */
s_i8 gab_obj_string_ref(gab_obj_string *self);

typedef void (*gab_builtin)(gab_engine *gab, gab_vm *vm, u8 argc,
                            gab_value argv[argc]);
/**
 * A native c function.
 */
struct gab_obj_builtin {
  gab_obj header;

  gab_builtin function;

  gab_value name;
};

#define GAB_VAL_IS_BUILTIN(value) (gab_val_is_obj_kind(value, kGAB_BUILTIN))
#define GAB_VAL_TO_BUILTIN(value) ((gab_obj_builtin *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BUILTIN(value) ((gab_obj_builtin *)value)

/**
 * Create a new builtin object.
 *
 * @param gab The gab engine.
 *
 * @param function The native function
 *
 * @param name The name of the function.
 */
gab_obj_builtin *gab_obj_builtin_create(gab_engine *gab, gab_builtin function,
                                        gab_value name);

/**
 * The prototype of a function, used to stamp out blocks.
 */
struct gab_obj_block_proto {
  gab_obj header;

  u8 narguments, nupvalues, nslots, nlocals, var;

  gab_module *mod;

  u8 upv_desc[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_BLOCK_PROTO(value)                                          \
  (gab_val_is_obj_kind(value, kGAB_BLOCK_PROTO))
#define GAB_VAL_TO_BLOCK_PROTO(value)                                          \
  ((gab_obj_block_proto *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BLOCK_PROTO(value) ((gab_obj_block_proto *)value)

/**
 * Create a new prototype object.
 *
 * @param gab The gab engine.
 *
 * @param mod The bytecode module that the prototype should wrap.
 *
 * @param narguments The number of arguments the function takes.
 *
 * @param nslots The number of slots the function uses.
 *
 * @param nlocals The number of locals the function uses.
 *
 * @param nupvalues The number of upvalues the function uses.
 *
 * @param var Whether the function is variadic.
 *
 * @param flags The flags of the upvalues.
 *
 * @param indexes The indexes of the upvalues.
 */
gab_obj_block_proto *gab_obj_prototype_create(gab_engine *gab, gab_module *mod,
                                              u8 narguments, u8 nslots,
                                              u8 nlocals, u8 nupvalues,
                                              boolean var,
                                              u8 flags[static nupvalues],
                                              u8 indexes[static nupvalues]);

/**
 * The block object, used to represent a closure.
 */
struct gab_obj_block {
  gab_obj header;

  u8 nupvalues;

  gab_obj_block_proto *p;

  gab_value upvalues[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_BLOCK(value) (gab_val_is_obj_kind(value, kGAB_BLOCK))
#define GAB_VAL_TO_BLOCK(value) ((gab_obj_block *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_BLOCK(value) ((gab_obj_block *)value)

/**
 * Create a new block object.
 *
 * @param gab The gab engine.
 *
 * @param p The prototype of the block.
 *
 * @return The new block object.
 */
gab_obj_block *gab_obj_block_create(gab_engine *gab, gab_obj_block_proto *p);

#define NAME specs
#define K gab_value
#define V gab_value
#define DEF_V GAB_VAL_UNDEFINED()
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

#define NAME helps
#define K gab_value
#define V s_i8
#define DEF_V ((s_i8){0})
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

/**
 * The message object, a collection of receivers and specializations, under a
 * name.
 */
struct gab_obj_message {
  gab_obj header;

  u8 version;

  gab_value name;

  u64 hash;

  d_specs specs;
};

#define GAB_VAL_IS_MESSAGE(value) (gab_val_is_obj_kind(value, kGAB_MESSAGE))
#define GAB_VAL_TO_MESSAGE(value) ((gab_obj_message *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_MESSAGE(value) ((gab_obj_message *)value)

/**
 * Create a new message object.
 *
 * @param gab The gab engine.
 *
 * @param name The name of the message.
 *
 * @return The new message object.
 */
gab_obj_message *gab_obj_message_create(gab_engine *gab, gab_value name);

/**
 * Find the index of a receiver's specializtion in the message.
 *
 * @param self The message object.
 *
 * @param receiver The receiver to look for.
 *
 * @return The index of the receiver's specialization, or UINT64_MAX if it
 * doesn't exist.
 */
static inline u64 gab_obj_message_find(gab_obj_message *obj,
                                       gab_value receiver) {
  if (!d_specs_exists(&obj->specs, receiver))
    return UINT64_MAX;

  return d_specs_index_of(&obj->specs, receiver);
}

/**
 * Set the receiver and specialization at the given offset in the message.
 * This is to be used internally by the vm.
 *
 * @param obj The message object.
 *
 * @param offset The offset in the message.
 *
 * @param rec The receiver.
 *
 * @param spec The specialization.
 */
static inline void gab_obj_message_set(gab_obj_message *obj, u64 offset,
                                       gab_value receiver,
                                       gab_value specialization) {
  d_specs_iset_key(&obj->specs, offset, receiver);
  d_specs_iset_val(&obj->specs, offset, specialization);
  obj->version++;
}

/**
 * Get the specialization at the given offset in the message.
 *
 * @param obj The message object.
 *
 * @param offset The offset in the message.
 */
static inline gab_value gab_obj_message_get(gab_obj_message *obj, u64 offset) {
  return d_specs_ival(&obj->specs, offset);
}

/**
 * Insert a specialization into the message for a given receiver.
 *
 * @param obj The message object.
 *
 * @param receiver The receiver.
 *
 * @param specialization The specialization.

 *
 */
static inline void gab_obj_message_insert(gab_obj_message *obj,
                                          gab_value receiver,
                                          gab_value specialization) {
  d_specs_insert(&obj->specs, receiver, specialization);
  obj->version++;
}

/**
 * Read the specialization for a given receiver.
 *
 * @param obj The message object.
 *
 * @param receiver The receiver.
 *
 * @return The specialization for the receiver, or GAB_VAL_UNDEFINED if it did
 * not exist.
 */
static inline gab_value gab_obj_message_read(gab_obj_message *obj,
                                             gab_value receiver) {
  return d_specs_read(&obj->specs, receiver);
}

/**
 * A shape object, used to define the layout of a record.
 */
struct gab_obj_shape {
  gab_obj header;

  u64 hash, len;

  gab_value data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_SHAPE(value) (gab_val_is_obj_kind(value, kGAB_SHAPE))
#define GAB_VAL_TO_SHAPE(value) ((gab_obj_shape *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_SHAPE(value) ((gab_obj_shape *)value)

/**
 * Create a new shape object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param len The length of the shape.
 *
 * @param stride The stride of the keys in they key array.
 *
 * @param keys The key array.
 */
gab_obj_shape *gab_obj_shape_create(gab_engine *gab, gab_vm *vm, u64 len,
                                    u64 stride, gab_value keys[static len]);

/**
 * Create a new shape for a tuple. The keys are monotonic increasing integers,
 * starting from 0.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param len The length of the tuple.
 */
gab_obj_shape *gab_obj_shape_create_tuple(gab_engine *gab, gab_vm *vm, u64 len);

/**
 * Find the offset of a key in the shape.
 *
 * @param obj The shape object.
 *
 * @param key The key to look for.
 *
 * @return The offset of the key, or UINT64_MAX if it doesn't exist.
 */
static inline u64 gab_obj_shape_find(gab_obj_shape *obj, gab_value key) {

  for (u64 i = 0; i < obj->len; i++) {
    assert(i < UINT64_MAX);

    if (obj->data[i] == key)
      return i;
  }

  return UINT64_MAX;
};

/**
 * Get the offset of the next key in the shape.
 *
 * @param obj The shape object.
 *
 * @param key The key to look for.
 *
 * @return The offset of the next key, 0 if the key is GAB_VAL_UNDEFINED(), or
 * UINT64_MAX if it doesn't exist.
 */
static inline u64 gab_obj_shape_next(gab_obj_shape *obj, gab_value key) {
  if (GAB_VAL_IS_UNDEFINED(key))
    return 0;

  u64 offset = gab_obj_shape_find(obj, key);

  if (offset == UINT64_MAX)
    return UINT64_MAX;

  return offset + 1;
};

/**
 * The counterpart to shape, which holds the data.
 */
struct gab_obj_record {
  gab_obj header;

  gab_obj_shape *shape;

  u64 len;

  gab_value data[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_RECORD(value) (gab_val_is_obj_kind(value, kGAB_RECORD))
#define GAB_VAL_TO_RECORD(value) ((gab_obj_record *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_RECORD(value) ((gab_obj_record *)value)

/**
 * Create a new record object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param shape The shape of the record.
 *
 * @param stride The stride of the values in the values array.
 *
 * @param values The values array. The length is inferred from shape.
 *
 * @return The new record object.
 */
gab_obj_record *gab_obj_record_create(gab_engine *gab, gab_vm *vm,
                                      gab_obj_shape *shape, u64 stride,
                                      gab_value values[static shape->len]);

/**
 * Create a new record object, with values initialized to nil.
 *
 * @param gab The gab engine.
 *
 * @param shape The shape of the record.
 *
 * @return The new record object.
 */
gab_obj_record *gab_obj_record_create_empty(gab_engine *gab,
                                            gab_obj_shape *shape);

/**
 * Set a value in the record. This function is not bounds checked. It should be
 * used only internally in the vm.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param obj The record object.
 */
void gab_obj_record_set(gab_vm *vm, gab_obj_record *obj, u64 offset,
                        gab_value value);

/**
 * Get a value at the given offset. This function is not bounds checked. It
 * should be used only internally in the vm.
 *
 * @param obj The record object.
 *
 * @param offset The offset.
 *
 * @return The value at the offset, or GAB_VAL_NIL.
 */
gab_value gab_obj_record_get(gab_obj_record *obj, u64 offset);

/**
 * Get the value corresponding to the given key.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return The value at the key, or GAB_VAL_NIL.
 */
gab_value gab_obj_record_at(gab_obj_record *obj, gab_value key);

/**
 * Put a value in the record at the given key.
 *
 * @param vm The vm.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @param value The value.
 *
 * @return true if the put was a success, false otherwise.
 */
boolean gab_obj_record_put(gab_vm *vm, gab_obj_record *obj, gab_value key,
                           gab_value value);

/**
 * Check if the record has a value at the given key.
 *
 * @param obj The record object.
 *
 * @param key The key.
 *
 * @return true if the key exists on the record, false otherwise.
 */
boolean gab_obj_record_has(gab_obj_record *obj, gab_value key);

typedef void (*gab_obj_container_destructor)(void *data);
typedef void (*gab_obj_container_visitor)(gab_gc *gc, gab_gc_visitor visitor,
                                          void *data);

/**
 * A container object, which holds arbitrary data.
 *
 * There are two callbacks:
 *  - one to do cleanup when the object is destroyed
 *  - one to visit children values when doing garbage collection.
 */
struct gab_obj_container {
  gab_obj header;

  gab_obj_container_destructor do_destroy;

  gab_obj_container_visitor do_visit;

  gab_value type;

  void *data;
};

#define GAB_VAL_IS_CONTAINER(value) (gab_val_is_obj_kind(value, kGAB_CONTAINER))
#define GAB_VAL_TO_CONTAINER(value) ((gab_obj_container *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_CONTAINER(value) ((gab_obj_container *)value)

/**
 * Create a new container object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param type The type of the container.
 *
 * @param destructor The destructor callback.
 *
 * @param visitor The visitor callback.
 *
 * @param data The data to store in the container.
 */
gab_obj_container *
gab_obj_container_create(gab_engine *gab, gab_vm *vm, gab_value type,
                         gab_obj_container_destructor destructor,
                         gab_obj_container_visitor visitor, void *data);

/**
 * A suspense object, which holds the state of a suspended coroutine.
 */
struct gab_obj_suspense {
  gab_obj header;

  u8 have, want, len;

  gab_obj_block *c;

  u64 offset;

  gab_value frame[FLEXIBLE_ARRAY];
};

#define GAB_VAL_IS_SUSPENSE(value) (gab_val_is_obj_kind(value, kGAB_SUSPENSE))
#define GAB_VAL_TO_SUSPENSE(value) ((gab_obj_suspense *)GAB_VAL_TO_OBJ(value))
#define GAB_OBJ_TO_SUSPENSE(value) ((gab_obj_suspense *)value)

/**
 * Create a new suspense object.
 *
 * @param gab The gab engine.
 *
 * @param vm The vm.
 *
 * @param c The paused block.
 *
 * @param offset The offset in the block.
 *
 * @param arity The arity of the block.
 *
 * @param want The number of values the block wants.
 *
 * @param len The length of the frame.
 *
 * @param frame The frame.
 */
gab_obj_suspense *gab_obj_suspense_create(gab_engine *gab, gab_vm *vm,
                                          gab_obj_block *block, u64 offset,
                                          u8 arity, u8 want, u8 len,
                                          gab_value frame[static len]);

#endif
