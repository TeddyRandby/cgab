# TODO List
- Improve inspect and to-c-string implementations
    - to-c-string should convert a gab-value into a string, which can be evaulated to arrive at the original value
        - But how would user-defined box types (like Map or List, for example) define this?
        - This is basically serialization. Maybe work on the gab-binary-object-format (gbof)
        - User types would need a function cb for doing this as well.
    - inspect should be useful for printf debugging
- HAMT to implement shapes *and* records

## Persistent HAMT for shapes and records
Requirements for shapes:
- List of keys
- Store hash value
- Store shared keys
Requirements for records:
- List of values
- Store shape value
- Store shared values

### Hypothetical hamt node object
```c
struct gab_obj_hamt {
    struct gab_obj header;

    uint32_t mask;

    union {
        struct { size_t hash; } shape;
        struct { gab_value shape; } record;
    };

    size_t len; // Cached size of the collection

    gab_value nodes[64];
}
```

Shapes could be a implemented with a single trie - but leaf nodes wouldn't actually have values, only keys.
Records would have a value for their shape, but they would also need a trie - of only values.
I could mirror the path followed in the shape in the record, but I lose the advantage of caching the lookup from the shape.
Unless I use the kSPEC cache to somehow cache the _path_ through the hamt, or I don't implement records as hamts at all.

Moving to immutable data structures *would* allow me to remove _all_ the cycle-detection code in GC.

Implementing Records as straight HAMTS (and abandoning caching lookup) would lose the structural-typing semantics of the language.
a. Is this even a bad thing?

Alternative implementations of immutable data structures for just our records?

```c
gab_value gab_recput(struct gab_triple gab, gab_value rec, gab_value key, gab_value val) {

};
```

