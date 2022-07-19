#ifndef GAB_H
#define GAB_H

#include "compiler/engine.h"

/*
  Create a Gab Engine. If you want libraries included, build and bind them
  before running any code.

  @return The allocated Sol Engine.
*/
gab_engine *gab_create();

gab_engine *gab_create_fork(gab_engine *gab);

void gab_destroy_fork(gab_engine *fork);

/*
  Cleanup a Gab Engine. Also cleans up all the modules which have been compiled
  with this engine.

  @param gab: The engine to clean up.
 */
void gab_destroy(gab_engine *gab);

typedef struct gab_lib_kvp {
  const char *key;
  gab_value value;
} gab_lib_kvp;

/*
   Bundle a list of KVPS into a Gab object.
*/
gab_value gab_bundle(gab_engine *gab, u64 size, gab_lib_kvp kvps[size]);

/*
  Bind a library to the Gab engine.
*/
void gab_bind_library(gab_engine *gab, u64 size, gab_lib_kvp kvps[size]);

/*
  Compile and run a c-str of gab code.

  @param gab: The engine to run the code with.

  @param source: The source code.

  @param module_name: Give a name to the module that will be compiled from the
  source.

  @param flags: The set of flags to pass to the compiler and vm.

  @return A gab result object.
*/
gab_result *gab_run_source(gab_engine *gab, const char *module_name,
                           s_u8 *source, u8 flags);

#endif
