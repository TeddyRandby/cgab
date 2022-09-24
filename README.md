# GAB

Gab is a simple and embeddable scripting language.

```
def Person {
    def New(name, age) { name age }

    def Celebrate_birthday(person) do
        !print('Happy Birthday {person.name}')
        person.age = person.age + 1
    end
}

let bob = Person.New('Bob', 20)
bob:Person.Celebrate_birthday()

```

# Goals

 - Be *fast*
 - Be *small*
 - Be *extensible*

# Features

Gab's more defining features include:

### Expression focused

In Gab, everything is an expression. 

`let a = if (cond) 1 else 2`

`def square(num) num * num`

When you need to a block of code, use the `do` expression!

It evaluates to the last expression in the block.

```
if cond !print('Hello World')

if cond do
  !print('Here are')

  !print('multiple expressions')
end
```

### Universal Function Call Syntax

Method chaining and OOP programming galore! Lets refer to our example above:

```
def Person {
    def New(name, age) { name, age }

    def Celebrate_birthday(person) do
        !print('Happy Birthday {person.name}')
        person.age = person.age + 1
    end
}

let bob = Person.New('Bob', 20)
bob:Person.Celebrate_birthday()

```

`Person` is just a normal object, but is serving as a namespace here. There are two functions belonging to person, `New` and `Celebrate_birthday`. 

The `:` operator is where the magic happens. Calling a function with the arrow operator automatically inserts the target as the first argument. And this can work with *any* target and *any* function. That is the beauty of *Universal Function Call Syntax*.

```
'Hello World':!print()
```

### Shapes

Objects in Gab are governed by their *shape*. This sort of looks like *duck typing*.
```
def Point {
    x
    y
}?
```
This says *set point to the shape of an object with a field x and a field y*

Now, code like this works as expected:
```
let pos = { x:10 y:20 }
!print(pos is point)
```
This prints `true`!

### Globals
Global state in Gab is all kept in a top-level object accessed by - you guessed it - the `!` operator!

All this time, the `!print(...)` statements are calling out to a function kept in the global top level object.

The Gab CLI stores all the builtin functions in this object. If you embed Gab in your c projects, you can easily
extend this object with your own built-ins.

The object is also useful wherever global state is - like in the REPL!

```
> !msg = 'hello world'
> !print(!msg)
```

### Iterators
Right now, iterators in Gab are just normal old Gab closures.
Here is an example of one to iterate over all the elements in an ordered object:

```
def iter(i) do
  let v = -1
  || return (v = v + 1), i[v]
end
```
The iterator accepts the object as the argument, and returns a lambda.
(`| args | body`) is the syntax for a lambda expression.

The lambda uses the `return` expression to return multiple values -  the index, and the item at the index. When the list is out of elements, i[v] will return `null`
Since null is falsey, this will trigger the end of the iterator. Using it looks like this:
```
let nums = [ 1 2 3 4 ]

for index, num in iter(nums)
  !print('nums has {num} at index {index}')
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
There are two modules that have been developed so far:
  - regex
  - socket
  - time
  - object

They are just simple wrappers for the POSIX regular expressions, and Linux sockets APIs. They are built as part of this project, so a simple `sudo make install` should install the modules alongside Gab.

### Dependencies

libc is the only dependency for the language

### Whats coming up (in no particular order):

 - [ ] Concurrency
 - [ ] Unicode Support
 - [ ] More syntax sugar in definitions
 - [ ] Symbols
 - [ ] Pretty Printing for objects
 - [ ] Windows support
 - [ ] Finalize c api and documentation
