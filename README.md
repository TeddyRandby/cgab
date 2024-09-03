# Gab
Gab is a dynamic scripting language. It's goals are:
- be *simple* - in design and implementation. 
- be *fast*. Performance is a feature.
- be *embeddable*. The c-api should be stable, simple, and productive.
## Inspiration and zen
Gab is heavily inspired by [Clojure](https://clojure.org), [Self](https://selflanguage.org/), [Lua](https://www.lua.org/), and [Erlang](https://www.erlang.org/).
Principles:
1. data are data are values are values.
2. control flow via polymorphism via messages.
# TOC
- Language Tour
- Imports
- Modules
- Dependencies
- Installation
### Language Tour
Programs are data. LISPs (such as the aforementioned Clojure) take this literally - code is a *data structure* that can be manipulated by *other code*.
Smalltalk and Self are message-oriented. Code is composed of values and messages. Values receive messages, and behavior emerges. Gab takes inspiration from these two ideas. Here is some example code:
```gab
    'Hello world!' :print
```
This code resembles Smalltalk somewhat, and can be transcribed in English as:
```
    Send the message 'print' to the value 'Hello world!'
```
Here is an alternative, equally valid syntax:
```gab
    \print ('Hello world!')
```
This syntax might look more familiar to programmers in the c-family of languages (Besides the curious '\\'). Said programmers might transcribe this block as:
```
    Call the function 'print' with the argument 'Hello world!'
```
This interpretation isn't wrong, but this is __more__ accurate:
```
    Send the empty message to the value '\print' with the argument 'Hello world!'
```
This might explain the peculiar syntax `\print`. This is actually a *message literal*.
To make an analogy to traditional classes and OOP, think of this as a generic value-representation for a method.
Polymorphism works as you'd expect. \+ behaves differently depending on the receiver:
```gab
    1 + 1                       # => 2
    \+ (1, 1)                   # => 2

    'Hello ' + 'world!'         # => 'Hello world!'
    \+ ('Hello ', 'world!')     # => 'Hello world!'
```
To peel back another layer, lets define Gab's syntax a little more clearly:
#### Numbers
Numbers are represented as IEEE 64-bit floats.
```gab
    1
    -4
    5.9
```
#### Strings
Strings are just byte arrays. Single-quoted strings support interpolation, and both support escape sequences.
```gab
    'Hello'
    "world!"
    'My name is { 'Joe' }!'
    'Escape unicode: \u[2502]'
```
#### Blocks
Blocks are functions. They always include an implicit variable `self`.
Depending on the context that the block is called in, `self` will have different values.
```gab
    add = do a, b:
        a + b
    end

    add(1,2) # => 3
```
#### Records
Records are collections of key-value pairs. They are ordered, and structurally typed.
```gab
    a_record = { work = 'value' }

    a_record                    # => { work = 'value', more_complex_work = <gab.block ...> }

    a_record :work              # => 'value'

    a_tuple = [1, 2, 3]         # => A record as above, but the keys are ascending integers (starting from 0)

    a_tuple                     # => { 0 = 1, 1 = 2, 2 = 3 }
```
Records, like all values, are __immutable__. This means that setting values in records returns a *new record*.
```gab
    a_record = { work = 'value' }

    a_record = a_record :work 'another value'   # => When an argument is provided, this message serves as a 'set' instead of a 'get'.

    a_record :print                             # => { work = 'another value' }
```
Normally this would be incredibly expensive, copying entire datastructures just to make a single mutation.
Gab's records are implemented using Hash-Array-Mapped-Tries, which use structural sharing to reduce memory impact and reduce the cost of copying.
#### Sigils
Sigils are similar to strings (and interchangeable in some ways). However, they respond to messages differently.
```gab
    .hello                  # => .hello (as a sigil)

    'hello'                 # => hello (as a string)
    
    .hello == 'hello'       # => false

    # \? is the message for getting the type of a value
    .hello ?                # => .hello

    'hello' ?               # => gab.string
```
Because sigil's have a different type, you can define how an *individual sigil* responds to a message. Here is an example:
```gab
# Define the message 'then' for the .true and .false sigil.
\then :defcase! {
    .true = do callback:
        # In the true path, we call the callback
        callback()
    end,
    .fase = do:
        # In the false path, we return false
        self
    end,
}
```
#### Messages
Messages are the *only* mechanism for defining behavior or control flow in gab.
```gab
    \print                  # => \print

    1 :print                # => prints 1

    \print (1, 2, 3)        # => prints 1, 2, 3
```
The core library provides some messages for defining messages. This might feel a little lispy:
```gab
\say_hello :def!(['gab.string', 'gab.sigil'], do: 'Hello, {self}!' :print end)

'Joe' :say_hello    # => Hello, Joe!

.Rich :say_hello    # => Hello, Rich!

\meet :defcase! {
    .Joe    = do: 'Nice to meet you Joe!' :print end,
    .Rich   = do: 'Its a pleasure, Rich!' :print end, 
}

.Joe  :meet         # => Nice to meet you Joe!

.Rich :meet         # => Its a pleasure, Rich!
```
### Behavior
Behavior in Gab is dictated *exclusively* by polymorphic, infix messages. These infix messages always have one left-hand value and up to one right-hand value.
However, the tuple syntax (eg: `(3, 4)`) allows multiple values to be wrapped into one. Blocks can return multiple values in the same way.
```gab
    # Send \+ to  1 with an argument of 1
    1 + 1

    # Send \do_work to val with an argument of (2, 3, 4)
    val :do_work (2, 3, 4)

    ok, result = val :might_fail 'something'
```
### Concurrency???
New programming languages that don't consider concurrency are boring! Everyones walking around with 8+ cores on them at all times, might as well use em!
Gab's runtime uses something similar to goroutines or processes. A `gab.fiber` is a lightweight thread of execution, which are quick to create/destroy.
Currently, they are mapped on to 8 (arbitrarily chosen at compile time) os threads from a global run-queue. There are many opportunities for improvement on this model.
```gab
# Define a message here that will do a good amount of work. In this case, creating a big list in a very silly way.
\do_acc :defcase! {
  .true  = do acc: acc end,
  .false = do acc, n: [n, acc:acc(n - 1)**] end, 
}

\acc :def!(['gab.record'], do n:
  n :== 0 :do_acc (self, n)
end)

# Define the \go message for blocks! This launches a new gab.fiber with the block as its argument.
\go :def!(['gab.block'], do: .gab.fiber(self) end)

# Here is our block that does a lot of work
work = do: [] :acc 950 :len:print end

# Send off all these workers to do their work in parallel!
work :go
work :go
work :go
work :go
work :go
    # => will wait a couple seconds, then print 950 5 times right next to each other.
```
A neat feature built on top of the concurrency model is *concurrent* garbage collection. There is a secret extra thread dedicated to garbage collection, which when triggered
briefly interrupts each running thread to track its live objects. After the brief interruption, each running thread is returned to its work and the collecting thread finishes the collection.
Because the threads all share a single collector, and all gab values are immutable, this means that gab can implement message sends *without any serialization or copying*.
Other models, such as in both Go and Erlang, copy/serialize messages into/out of channels and mailboxes. Gab's model doesn't have this restriction - we can pass values between fibers freely.
Gab uses CSP as its model for communicating between fibers. This is the `go` or `clojure.core.async` model, using channels and operations like `put`, `take`, and `alt/select`.
Gab has an initial implementation of this, and actually uses a `gab.channel` of `gab.fibers` at the runtime level to queue fibers for running.
#### TODO:
    - Instead of malloc/free, write a custom per-job allocator. This can function like a bump allocator, and can enable further optimizations:
        - allocate-as-dead. Objects that *survive* their first collection are *moved* out of the bump allocator, and into long term storage.
            This can work because we are __guaranteed__ to visit all the roots that kept this object alive in that first collection.
    - Implement buffered channels.
        - Because channels are mutable and require locking, their ownership is a bit funky. To simplify, just *increment* all values that go into a channel, and decrement all that come out.
        - This means that when channels are destroyed that still have references to objects, we need to dref them.
    - Implement *yielding/descheduling*. When a fiber blocks (like on a channel put/take), that fiber should *yield* to the back of some queue, so that the OS thread can continue work on another fiber.
        - Because Gab doesn't do this currenty, gab code actually can't run on just one thread. Try `gab run -j 1 test`. It will just hang.
        - This can't be implemented without changing the main work-channel to be buffered. 
        - This requires finding a sound strategy for handling thread-migration in the gc.
    - Optimize shape datastructure.
        - Shapes are mutable because of their ugly transition vector
        - Building big shapes (like for tuples) is basically traversing a linked list in O(n) time (ugly)
    - Optimize string interning datastructure.
        - Hashmap works well enough, but copies a lot of data and makes concat/slice slow.
    - Of course, lots of library work can be done.
        - More iterators
        - Improve spec
        - Wrappers for c-libraries for data parsing, http, sockets, number stuff
# What about imports?
Gab defines several native messages. `:print` is one you should be familiar with by now - `:use` is another!
It is used like this:
```
'io' :use
```
The implementation searches for the following, in order:
 - `./(name).gab`
 - `./(name)/mod.gab`
 - `./lib(name).so`
 - `~/gab/(name)/.gab`
 - `~/gab/(name)/.gab`
 - `~/gab/(name)/mod.gab`
 - `~/gab/std/(name)/.gab`
 - `~/gab/lib//libcgab(name).so`
 Files ending in the `.gab` extension are evaluated, and the result of the last top-level expression is returned to the caller of `:use`. Files ending in the `.so` extension are opened dynamically, and searched for the symbol `gab_lib`. The result of this function is returned to the caller.

Most of the time, the return value of the `:use` call can be ignored. It is just called once to define the messages in the module. For example, the `io` module defines three messages:
 - `\open`, which returns a `<File>` handle
 - `\read`
 - `\write`
And thats it!
# Dependencies
libc is the only dependency for the interpreter. However, some libraries (such as http and term) depend on some c libraries. 
# Installation
This project is built with `Make`. The `Makefile` expects some environment variables to be present. Specifically:
  - `GAB_PREFIX`: A place for gab to install data without root. This is for packages and other kinds of things. Something like `/home/<user>`
  - `GAB_INSTALLPREFIX`: Where gab should install itself - the executable, header files, libcgab, etc. Something like `/usr/local`
  - `GAB_CCFLAGS`: Additional flags ot pass to the c compiler.

[Clide](https://github.com/TeddyRandby/clide) is a tool for managing shell scripts within projects. cgab uses it to manage useful scripts and build configuration. To configure and install with clide, run `clide install` and complete the prompts.

If clide is unavailable, it is simple to just use make. To install a release build:
  1. `GAB_PREFIX="<path>" GAB_INSTALL_PREFIX="<path>" GAB_CCFLAGS="-O3" make build`
  2. `sudo make install`
# C-API Documentation
The c-api is contained in the single header file `gab.h`. You can generate documentation with `clide docs`, or by just running `doxygen`.
# KNOWN ISSUE
Thread migration is currently an issue, as its not handled in the paper used to implement the GC. This includes migration *in and out* of the global run queue.
