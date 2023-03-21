# GAB
Gab is a simple and embeddable scripting language.

```
def Person {
  name
  age
}

def celebrate[Person]()
  'Happy Birthday {self.name}!'
  self.age = self.age + 1
end

def Bob = {
  name = 'Bob'
  age  = 22
}

Bob:celebrate # Happy Birthday Bob
```
# Goals
 - Be *fast*
 - Be *small*
 - Be *extensible*
# Features
Gab's more defining features include:
### Expression focused
In Gab, everything is an expression. 
```
let a = cond and 1 or 2
```
### Shapes
Although Gab is dynamically typed, types are treated as first class.
Many dynamic languages implement their objects using hidden classes (V8), or shapes (cruby). Gab makes this a first class language feature!
```
def Point {
    x
    y
}
```
Objects in Gab are structurally typed. This means that two objects are the same type iff they share the same set of keys. This principle is used to define 'methods' for our types.
```
let pos = { x=10 y=20 }

print(pos is point) # true
```
### Message Passing
Gab provides a nice abstraction for polymorphism through *message passing*.
A message is defined as follows:
```
def Dog { ... }
def Person { ... }

# Define a message 'speak' that can be sent to the receiver <Dog>
def speak[Dog]
    print('woof!')
end

# Define a message 'speak' that can be sent to the receiver <Person>
def speak[Person]
    print('hello!')
end
```
The expression in brackets is used to create a specialization for the message on that type.
In this example, the values of the Dog and Person shapes are used to instantiate a specific handler. Now later on we can write code like this:
```
let animal = getMammal()
# This could be a person or a dog

animal:speak
# prints 'woof!'  if the animal is a dog
# prints 'hello!' if the animal is a person
```
### Globals
Gab has no global variables. If you wan't to export code from your modules, return from the top-level, or define messages.
### Suspensions
The last major feature of Gab is suspensions. From any function, instead of returning you may `yield`
```
def do_twice = do (cb)
    yield cb()
    cb()
end

let first_result, suspense = do_twice()

let second_result = suspense()
```
Yield pauses the functions execution, and returns a `Supsense` object in addition to whatever else is yielded. This object can then be called to resume execution of the function. A yield can also receive values, which are passed to the effect as arguments.
```
def say_hello = do (maybe_nil)
    let name = maybe_nil

    not name then
        name = yield
    end

    print('Hello {name}')
end

let sus = say_hello()

sus('Bob') # Hello Bob
```
### What about imports?
Gab supplies a `require` global for importing code. 
It is used like this:
```
def io = require("socket")
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

 Files ending in the `.gab` extension are evaluated, and the result of the last top-level expression is returned to the caller of `require`. Files ending in the `.so` extension are opened dynamically, and searched for the symbol `gab_mod`. The result of this function is passed up to the callee.
### Modules
There are some modules bundled with the main cli.
  - io
### Dependencies
libc is the only dependency for the interpreter.
### Installation
This project is built with Meson. To install it:
  - Clone this repo.
  - run `meson setup build`
  - run `meson install`
### Whats coming up (in no particular order):
 - [ ] Windows support
 - [ ] Finalize c api and documentation
