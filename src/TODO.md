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

We want blocks and messages to have the same dispatch
syntax so that they can be used interchangeably. I don't like using the traditional
function-call syntax because this *is not* the focus of the language.
```gab
    sum = do a, b; a + b end

    sum(1,2)
    \+(1, 2)
```

```gab
    sum = do a, b; a + b end

    1 (sum, 2) :do
```

```gab
    sum = [a, b: a:b]

    .td:filter \pos?
    .td:filter [a: a:pos?]
    .td:filter sum
```
