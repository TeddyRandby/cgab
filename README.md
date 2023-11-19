# Gab
Gab is a dynamic scripting language. It's goals are:
- be *simple* - in design and implementation. 
- be *fast*. Performance is a first-class concern.
- be *embeddable*. The c-api should be stable, simple, and productive.

## At a glance
```
def Person {
  first_name
  last_name
  age
}

def birthday[Person]()
  'Happy Birthday {self.first_name}!':print
  self:age = self:age + 1
end

Bob = { first_name = 'Bob', last_name = 'Smith', age  = 22 }

Bob:birthday # prints 'Happy Birthday Bob'
```
# TOC
- Features
    - Syntax
    - Expressions
    - Shapes
    - Messages
    - Suspense
    - Multi Values
- Imports
- Modules
- Dependencies
- Installation
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
```
### Expression focused
In Gab, everything is an expression. 
```
a = cond and 1 or 2

# There is no 'if'
some_condition then
    '{some_condition} was true':print
end

some_condition else
    '{condition} was false':print
end

# 'then' and 'else' aren't statements, they're also expressions.
# They evaluate to their condition. The following works like a conventional if-else

some_condition then
    do_something()
end else
    do_something_else()
end
```
### Shapes
Although Gab is dynamically typed, types are treated as first class.
```
# Get the type of a value with the ? operator
a = 2
?a # Number
```

Aside from the normal primitive types, Gab has an aggregate type called a `Record`.
```
example_record = { some_key = 1, other_key = 2 }
```
The type of a record is a special value called a `Shape`. This is the unique list of keys of the record, in order.
```
?example_record # <Shape some_key other_key>
```
These shapes are structurally typed. This means that any two records with the same keys in the same order are of the same type.
```
other_record = { some_key = 'hello', other_key = 'world' }

?example_record == ?other_record # true
```
### Message Passing
There are no traditional classes or methods in Gab. Polymorphic code is written by `sending messages`.
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
The expression in `[]` is used to create a specialized implementation of the message for that type.
In this example, the values of the Dog and Person shapes are used to specialize the message `:speak`. Now we can write polymorphic code like this:
```
animal = getMammal() # This could be a person or a dog

animal:speak
# prints 'woof!'  if the animal is a dog
# prints 'hello!' if the animal is a person
```
### Suspense
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
Gab lacks exceptions, and prefers returning errors as values using multi values. By convention, the status is return as the first value.
```
status, content = io:open('hello.txt')

status == .ok else
    'Encountered error {status}':print
end
```
# What about imports?
Gab defines several native messages. `:print` is one you should be familiar with by now - `:use` is another!
It is used like this:
```
:use 'io'
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
 - `:io.open`, which returns a `<File>` handle
 - `<File>:read`
 - `<File>:write`
And thats it!

# Modules
There are some modules bundled with the main cli.
  - *list*: A growing container of contiguous elements
  - *map*: A growing container of sparse elements, indexed by keys
  - *string*: Helpers for the `<String>` type
  - *num*: Helpers for the `<Number>` type
  - *rec*: Helpers for the `<Record>` type
  - *symbol*: The `:symbol` constructor (Useful for libraries and namespacing)
  - *fiber*: The `:fiber` constructor (Multi-threading)
  - *io*: See above
  - *socket*: Basically a wrapper for posix sockets
  - *dis*: Disassemble your blocks into bytecode for debugging purposes
  - *pry*: Pry into the callstack for debugging purposes
# Dependencies
libc is the only dependency for the interpreter. However, some libraries (such as http and term) depend on some c libraries. 
# Installation
This project is built with `Make`. The `Makefile` expects some environment variables to be present. Specifically:
  - `GAB_PREFIX`: A place for gab to install data without root. This is for packages and other kinds of things.
  - `GAB_INSTALLPREFIX`: Where gab should install itself - the executable, header files, libcgab, etc.
  - `GAB_CFLAGS`: Additional flags ot pass to the c compiler.

[Clide](https://github.com/TeddyRandby/clide) is a tool for managing shell scripts for projects. This project uses it to manage useful scripts and build configuration. To configure and build with clide, run `clide build` and complete the prompts. 
