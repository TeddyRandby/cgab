#ifndef gab_mod_H
#define gab_mod_H
#include "include/gab.h"
#include "lexer.h"

#define NAME gab_constant
#define T gab_value
#include "include/vector.h"

struct gab_mod {
  struct gab_mod *next;

  struct gab_src *source;

  /*
   * The name of the module
   **/
  gab_value name;

  /*
    The constant table.
  */
  v_gab_constant constants;

  /*
    The instructions, a contiguous vector of single-byte op-codes and args.
  */
  v_uint8_t bytecode;

  /*
    A sister vector to 'bytecode'.
    This vector relates each instruction to a token offset.
  */
  v_uint64_t bytecode_toks;

  size_t previous_basic_block;
  size_t basic_block;
  uint8_t previous_compiled_op;
};

/*
  Creating and destroying modules, from nothing and from a base module.
*/
struct gab_mod *gab_mod(struct gab_eg *gab, gab_value name,
                        struct gab_src *src);

void gab_moddestroy(struct gab_triple gab, struct gab_mod *mod);

struct gab_mod *gab_modcpy(struct gab_triple gab, struct gab_mod *mod);

/* Push an op into the module's bytecode */
void gab_modop(struct gab_mod *mod, gab_opcode o, size_t t);

/* Push a byte into the module's bytecode */
void gab_modbyte(struct gab_mod *mod, uint8_t b, size_t t);

/* Push a short into the module's bytecode */
void gab_modshort(struct gab_mod *mod, uint16_t h, size_t t);

/* Push a load_constant op into the module's bytecode */
void gab_modloadk(struct gab_mod *mod, gab_value k, size_t t);

/* Push a load_local op into the module's bytecode */
void gab_modloadl(struct gab_mod *mod, uint8_t local, size_t t);

/* Push a store_local op into the module's bytecode */
void gab_modstorel(struct gab_mod *mod, uint8_t local, size_t t);

/* Push a load_upvalue op into the module's bytecode */
void gab_modloadu(struct gab_mod *mod, uint8_t upvalue, size_t t);

/* Push a return op into the module's bytecode */
void gab_modret(struct gab_mod *mod, uint8_t have, bool mv, size_t t);

/* Push a tuple op into the module's bytecode */
void gab_modtup(struct gab_mod *mod, uint8_t have, bool mv, size_t t);

/* Push a yield op into the module's bytecode */
void gab_modyield(struct gab_mod *mod, uint16_t proto, uint8_t have, bool mv,
                  size_t t);

/* Push a pack op into the module's bytecode */
void gab_modpack(struct gab_mod *mod, uint8_t below, uint8_t above, size_t t);

/* Push a send op into the module's bytecode */
void gab_modsend(struct gab_mod *mod, uint8_t have, uint16_t message, bool mv,
                  size_t t);

/* Push a shift op into the module's bytecode */
void gab_modshift(struct gab_mod *mod, uint8_t n, size_t t);

/* Push a pop op into the module's bytecode */
void gab_modpop(struct gab_mod *mod, uint8_t n, size_t t);

/* Push an inter instruction */
uint64_t gab_moditer(struct gab_mod *mod, uint8_t start, uint8_t want,
                           size_t t);

/* Push a next instruction */
void gab_modnext(struct gab_mod *mod, uint8_t local, size_t t);

/* Push a loop instruction */
uint64_t gab_modloop(struct gab_mod *mod);

/* Push a jump instruction */
uint64_t gab_modjump(struct gab_mod *mod, uint8_t jump_op, size_t t);

/* Push a loop instruction */
void gab_modloopp(struct gab_mod *mod, uint64_t label, size_t t);

/* Push a jump instruction */
void gab_modjumpp(struct gab_mod *mod, uint64_t label);

/* Try to patch the last emitted instruction to want a different amt of values
 */
bool gab_modmvp(struct gab_mod *mod, uint8_t want);

/* Add a constant to the module's constant table */
uint16_t gab_mod_add_constant(struct gab_mod *mod, gab_value k);
#endif
