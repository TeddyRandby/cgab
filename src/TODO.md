# TODO List
- Improve and to-c-string implementations
    - to-c-string should convert a gab-value into a string, which can be evaulated to arrive at the original value
        - This is basically serialization. Maybe work on the gab-binary-object-format (gbof)
        - User types would need a function cb for doing this as well.

- Improve string/binary utils
  - Conveniences for _building_ up 

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
