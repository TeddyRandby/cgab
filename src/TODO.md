# TODO List
- Investigate more optimizations for GC.
    - Objects are allocated alive, with a decrement. We could instead allocate them dead, and change them to alive at their first increment.
    - This leads to a problem where there are basically no increments or decrements ever pushed, so the GC's buffers never fill up and we never collect.
    - At this point, we basically become a mark-sweep collector. When we collect, we increment all the roots, and everytning else gets freed.
    - WIth the current imlementation, the decrements that are pushed when we allocate objects fill up the GC's mod buffers and eventually trigger GC's.
- Improve inspect and to-c-string implementations
    - to-c-string should convert a gab-value into a string, which can be evaulated to arrive at the original value
        - But how would user-defined box types (like Map or List, for example) define this?
        - This is basically serialization. Maybe work on the gab-binary-object-format (gbof)
        - User types would need a function cb for doing this as well.
    - inspect should be useful for printf debugging
- C-API improvements and documentation
    - First improviement idea - pass buffers into functions that evaluate gab code, and gab can trim-return values in and return a status code.

- Is it possible to remove some language features and implement them simply in a std lib?
    My thoughts are `then`, `else`, `and`, `or`, `match`, and `for in`
    ```gab
        def then[.Boolean](cb); self and cb(); self end
    ```

    I can't define messages on 'True', only on the .Boolean type.

    I can't implement (:and) because it does short-circuiting. 

    ```gab
    [some collection]:for(&:each) do (v)
        .pass
    end

    :for([some collection]:each) do (v)
        .pass
    end

    def for[](msg, cb)
        *args, suspense = self:(msg)

        loop
            cb(*args)            

            ?suspense == .Suspense or break

            *args, suspense = suspense()
        end
    end
    ```
