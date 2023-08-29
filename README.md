# GAB
Gab is a dynamic scripting language.

```
def Person {
  first_name
  last_name
  age
}

def birthday[Person]()
  'Happy Birthday {self.first_name}!':print
  self.age = self.age + 1
end

Bob = { first_name = 'Bob', last_name = 'Smith', age  = 22 }

Bob:birthday # Happy Birthday Bob
```
# Goals
 - Be *fast*
 - Be *small*
 - Be *extensible*
 - Be *embeddable*
# Features
Gab's more defining features include:
### Syntax
It seems like it shouldn't matter, and yet it does. If you love `{}`, here are the parts of Gab's syntax that you might find peculiar.
```
anonymous_function = do (argument)
    'I am an anonymous function. Here is my argument: {argument}':print
end

do_work(anonymous_function, { config = true })

# is the same as:

do_work(anonymous_function) { config = true }

# you can also:

do_work { config = true }

# and with callbacks:

do_work do (argument)
    'Here is my argument: {argument}':print
end

# Thus, spawning a thread to do some work with some configuration looks like:

:fiber { mod = 'my_module' } do
    :some_other_work()
end

# There is no 'if'

condition then
    '{condition} was true':print
end

condition else
    '{condition} was false':print
end

# 'then' and 'else' aren't statements, they're actually expressions.
# They evaluate to their condition. The following works like a conventional if-else

condition then
    do_something()
end else
    do_something_else()
end
```
### Expression focused
In Gab, everything is an expression. 
```
a = cond and 1 or 2
```
### Shapes
Although Gab is dynamically typed, types are treated as first class.
Many dynamic languages implement their objects/tables/hashes using hidden classes (V8), or shapes (cruby). Gab takes this idea and makes it a first class language feature!
```
def Point { x y }
```
Records in Gab are structurally typed. This means that two records are the same type iff they share the same set of keys (order matters). This principle is used to define 'methods' for our types.
```
pos = { x = 10, y = 20 }

pos:is(Point):print # true
```
### Message Passing
Gab provides an abstraction for polymorphism through *message passing*.
A message is defined as follows:
```
def Dog { ... }

# Define a message 'speak' that can be sent to the receiver <Dog>
def speak[Dog]
    'woof!':print
end

# Define a message 'speak' that can be sent to the receiver <Person>
def speak[Person]
    'hello!':print
end
```
The expression in brackets is used to create a specialization for the message on that type.
In this example, the values of the Dog and Person shapes are used to instantiate a specific handler. Now we can write polymorphic code like this:
```
animal = getMammal() # This could be a person or a dog

animal:speak
# prints 'woof!'  if the animal is a dog
# prints 'hello!' if the animal is a person
```
### Globals
Gab has no notion of global variables. To export code from your modules, either return from the top-level or define messages.
### Suspensions
The last major feature of Gab is suspensions. From any block, instead of returning you may `yield`
```
def do_twice = do (cb)
    yield cb()
    cb()
end

first_result, suspense = do_twice do 
    yield 20
    return 22
end

second_result = suspense()

'The answer to life, the universe, and everything: {first_result + second_result}':print # 42
```
Yield pauses the block's execution, and returns a `Supsense` value in addition to whatever else is yielded. This value can be called to resume execution of the block. A yield can also receive values, which are passed to the block as the results of the yield expression.
```
def say_hello = do (maybe_nil)
    name = maybe_nil

    not name then
        name = yield
    end

    'Hello {name}':print
end

sus = say_hello()

sus('Bob') # Hello Bob
```
### Multi Values
The feature `multi values` is similar to multiple return values that are in other languages like Go and Lua.
```
def full_name[Person]
    return self.first_name, self.last_name
end

first, last = Bob:full_name
```
### What about imports?
Gab defines several builtin messages. `:print` is one you should be familiar with by now - `:require` is another!
It is used like this:
```
"io":require
```
The implementation searches for the following, in order:
 - `./(name).gab`
 - `./(name)/mod.gab`
 - `./lib(name).so`
 - `/usr/local/share/gab/(name)/.gab`
 - `/usr/local/share/gab/(name)/.gab`
 - `/usr/local/share/gab/(name)/mod.gab`
 - `/usr/local/share/gab/std/(name)/.gab`
 - `/usr/local/lib/gab/lib(name).so`
 Files ending in the `.gab` extension are evaluated, and the result of the last top-level expression is returned to the caller of `require`. Files ending in the `.so` extension are opened dynamically, and searched for the symbol `gab_mod`. The result of this function is returned to the caller.

Most of the time, the return value of the `:require` call can be ignored. It is just called once to define the messages in the module. For example, the `io` module defines three messages:
 - `<String>:open`, which returns a `<File>` handle
 - `<File>:read`
 - `<File>:write`
And thats it!

### Modules
There are some modules bundled with the main cli.
  - *list*: A growing container of contiguous elements
  - *map*: A growing container of sparse elements, indexed by keys
  - *string*: Helpers for the `<String>` type
  - *num*: Helpers for the `<Number>` type
  - *rec*: Helpers for the `<Record>` type
  - *symbol*: The `:symbol` constructor (Useful for libraries and namespacing)
  - *fiber*: The `:fiber` constructor (Multi-threading)
  - *io*: See above
  - *regex*: Basically a wrapper for posix regex
  - *socket*: Basically a wrapper for posix sockets
  - *dis*: Disassemble your blocks into bytecode for debugging purposes
  - *pry*: Pry into the callstack for debugging purposes
### Dependencies
libc is the only dependency for the interpreter.
### Installation
This project is built with Meson. To install it:
  - Clone this repo.
  - run `meson setup build`
  - run `meson install`
### Whats coming up (in no particular order):
 - [ ] Finalize c api and documentation
 - [ ] Windows support
