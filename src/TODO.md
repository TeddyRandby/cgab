# TODO List
- Improve inspect and to-c-string implementations
    - to-c-string should convert a gab-value into a string, which can be evaulated to arrive at the original value
        - But how would user-defined box types (like Map or List, for example) define this?
        - This is basically serialization. Maybe work on the gab-binary-object-format (gbof)
        - User types would need a function cb for doing this as well.
    - inspect should be useful for printf debugging
