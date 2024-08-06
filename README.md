# Gab
Gab is a dynamic scripting language. It's goals are:
- be *simple* - in design and implementation. 
- be *fast*. Performance is a feature.
- be *embeddable*. The c-api should be stable, simple, and productive.
## Inspiration and zen
Gab is heavily inspired by [Clojure](https://clojure.org), [Self](https://selflanguage.org/), and [Lua](https://www.lua.org/).
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
In the following section, we take a look at Gab's primitive values.
#### Numbers
Numbers are represented as IEEE 64-bit floats.
```gab
    1
    -4
    5.9
```
#### Strings
Strings are utf8-encoded byte arrays. Single-quoted strings support interpolation, and both support escape sequences.
```gab
    'Hello'
    "world!"
    'My name is { 'joe' }!'
    'Escape unicode: \u[2502]'
```
#### Blocks
Blocks are functions. They always include an implicit variable `self`.
Depending on the context that the block is called in, `self` will have different values.
```gab
    add = do a, b
        a + b
    end

    add(1,2) # => 3
```
#### Records
Records are collections of key-value pairs. They are ordered, and structurally typed.
```gab
    a_record = {
        work = 'value',
        more_complex_work = do
            'do some work':print
            42
        end,
    }

    a_record # => { work = 'value', more_complex_work = <gab.block ...> }

    a_record :work # => 'value'

    a_record :more_complex_work # => prints 'do some work' and returns 42

    a_tuple = [1, 2, 3] # => A record as above, but the keys are ascending integers (starting from 0)

    a_tuple # => { 0 = 1, 1 = 2, 2 = 3 }
```
#### Sigils
Sigils are similar to strings (and interchangeable in some ways). However, they respond to messages differently.
```gab
    .hello # => hello (as a sigil)

    'hello' # => hello (as a string)
    
    .hello == 'hello' # => false

    # \? is the message for getting the type of a value
    .hello ? # => hello

    'hello' ? # => gab.string
```
#### Messages
Messages are polymorphic behavior which can be dispatched statically as an infix expression with `:` or via application with a message literal.
```gab
    \print # => \print

    1 :print # => prints 1

    \print (1, 2, 3) # => prints 1, 2, 3
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
#### Note about tuples
Tuples are *NOT* records - they don't even allocate memory. Think of them as a statically sized array, which use the stack for storage.
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
