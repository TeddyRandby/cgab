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
  name: 'Bob'
  age: 22
}

Bob:celebrate
#prints 'Happy Birthday Bob'
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
let a = if cond
   1
 else
   2
 end
```
### Shapes
Although Gab is dynamically typed, types matter more than in other dynamic languages.
```
def Point {
    x
    y
}
```
Objects in Gab are structurally typed. This means that two objects are the same type iff they share the same set of keys. This principle is used to define 'method' for our types.
```
let pos = { x:10 y:20 }
print(pos is point)
# prints true
```
### Message Passing
Gab provides a nice abstraction for polymorphism through *message passing*.
A message is defined as follows:
```
def Dog { ... }
def Person { ... }

def speak[Dog]()
    print('woof!')
end

def speak[Person]
    print('hello!')
end
```
The expression in brackets is used to create a specialization for the message on that type.
In this example, the values of the Dog and Person shapes are used to instantiate a specific handler. Now later on we can write code like this:
```
let animal = getAnimal()
# This could be a person or a dog

animal:speak
# prints 'woof!'  if the animal is a dog
# prints 'hello!' if the animal is a person
```
### Globals
Gab has no global variables. If you wan't to export code from your modules, return from the top-level, or define messages.
### Effects
The last major feature of Gab is effects. From any function, instead of returning you may `yield`
```
def do_twice(cb)
    yield cb()

    cb()
end

let first_result, eff = do_twice()

let second_result = eff()
```
Yield pauses the functions execution, and returns an `Effect` object in addition to whatever else is yielded. This object can then be called to resume execution of the function. A yield can also receive values, which are passed to the effect as arguments.
```
def say_hello(name)
    if not name
        name = yield
    end

    print('Hello {name}')
end

let eff = say_hello()

eff('Bob')
# prints 'Hello Bob'
```
### What about imports?
Gab supplies a `require` global for importing code. 
It is used like this:
```
let socket = !require("socket")
```
The implementation searches for the following, in order:
 - `./(name).gab`
 - `./(name)/mod.gab`
 - `./lib(name).so`
 - `/usr/local/lib/gab/(name)/.gab`
 - `/usr/local/lib/gab/(name)/mod.gab`
 - `/usr/local/lib/gab/lib(name).so`

 Files ending in the `.gab` extension are evaluated, and the result of the last top-level expression is returned to the caller of `require`. Files ending in the `.so` extension are opened dynamically, and searched for the symbol `gab_mod`. The result of this function is passed up to the callee.
### Modules
There are some modules bundled with the main cli.
  - regex
  - socket
  - time
  - object
  - math
  - io
  - string
### Dependencies
libc is the only dependency for the interpreter.
### Installation
This project is built with Meson. To install it:
  - Clone this repo.
  - run `meson setup build`
  - run `meson install`
### Whats coming up (in no particular order):
 - [ ] Concurrency
 - [ ] Symbols
 - [ ] Pretty Printing for objects
 - [ ] Windows support
 - [ ] Finalize c api and documentation
