# TODO List
- Improve and to-c-string implementations
    - to-c-string should convert a gab-value into a string, which can be evaulated to arrive at the original value
        - This is basically serialization. Maybe work on the gab-binary-object-format (gbof)
        - User types would need a function cb for doing this as well.

improve gab cli utility, reflect c-api more

gab build [build_opts] <file>
    - dump bytecode
    - silent

gab run [run_opts] <file>
    - dump bytecode
    - stream input
    - delimit input
    - silent

gab exec [build_opts run_opts] <program>
    - dump bytecode
    - stream input
    - delimit input
    - silent

gab repl [build_opts run_opts repl_opts]

Potentially building in fibers/channels into the library more, introducing a sort of greenthread idea?

This would require a scheduler, with yielding points in the VM. Which data should be global, and which should be thread-specific?

```c
    struct gab_eg {
        size_t hash_seed;

        gab_value types[kGAB_NKINDS];

        /* Compiled source files, and cached modules */
        d_gab_src sources;
        d_gab_modules modules;

        /* Interned, unique values stored here */
        d_strings strings;
        d_shapes shapes;
        d_messages messages;

        v_gab_value scratch;

        void *(*os_dynopen)(const char *path);

        void *(*os_dynsymbol)(void *os_dynhandle, const char *path);
    }
```
