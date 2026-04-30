# Wio Language Reference

This document reorganizes the current Wio design into a real language-reference
style document rather than a raw feature list. It is written in English and is
intended to be detailed enough for future implementation, compiler work, test
design, and language discussion.

The reference deliberately separates three different realities:

- **Language intent**: what Wio is supposed to mean as a language.
- **Current compiler behavior**: what the current parser, semantic analyzer, and
  code generator actually do today.
- **Planned or reserved features**: syntax or keywords that exist in the token
  set or design notes but are not fully implemented yet.

Whenever design intent and current compiler behavior differ, this document says
so explicitly.

For representative conformance tests tied to the stabilized language surface,
see [`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

## 1. Document Scope and Status

Wio is currently best described as an experimental statically typed language
with:

- a C++20 backend,
- a custom lexer, parser, semantic analyzer, and code generator,
- a hybrid object model based on `component`, `object`, and `interface`,
- explicit references via `ref` and `view`,
- code-generation oriented attributes,
- expression-oriented constructs such as `match`.

### 1.1 Status Labels Used in This Document

This reference uses the following practical labels:

- **Implemented**: present in the current compiler and usable today.
- **Partially implemented**: syntax exists, but semantics or tooling are still
  incomplete.
- **Planned**: intentionally part of the design but not fully parsed or lowered.
- **Reserved**: tokenized or named in the compiler, but not yet given final
  source-language meaning.

### 1.2 Naming Conventions

This document uses:

- `T` for a generic type placeholder,
- `N` for a compile-time size,
- `expr` for expression placeholders,
- `stmt` for statement placeholders.

All code examples are written in Wio unless otherwise noted.

## 2. Source Files, Lexical Structure, and Comments

### 2.1 Source Files

Wio source files conventionally use the `.wio` extension.

Example:

```wio
use std::io;

fn Entry(args: string[]) -> i32 {
    std::io::Print("Hello from Wio");
    return 0;
}
```

### 2.2 Whitespace

Whitespace is generally ignored except where it separates tokens.

Examples:

```wio
let x:i32=123;
let x : i32 = 123;
let    x    :    i32    =    123;
```

All three forms are intended to mean the same thing.

### 2.3 Statement Terminators

Wio uses semicolons as statement terminators.

Examples:

```wio
let x = 10;
x += 1;
return;
```

#### Edge Case: Newlines Are Not Statement Terminators

The lexer has a `newline` token in the token enum, but the current parser does
not treat line breaks as statement boundaries. In practice:

- semicolons are required for ordinary statements,
- line breaks alone do not terminate statements.

### 2.4 Comments

Wio currently supports:

- shell-style single-line comments: `# ...`
- C++-style single-line comments: `// ...`
- block comments: `#* ... *#`

Examples:

```wio
# this is a single-line comment
let x = 10;

// this is also a single-line comment
let y = 20;

#*
   this is a block comment
   spanning multiple lines
*#
let z = 30;
```

#### Edge Case: Block Comments Are Not Nested

The current lexer scans until the first `*#`. Nested block comments are not
supported.

## 3. Identifiers, Keywords, and Reserved Words

### 3.1 Identifiers

Identifiers begin with a letter or underscore and may continue with letters,
digits, or underscores.

Examples:

```wio
let hp = 100;
let _internal = 1;
let player_name = "Gargantua";
```

### 3.2 Core Keywords

Current core keywords include:

- `fn`
- `let`
- `mut`
- `const`
- `ref`
- `view`
- `fit`
- `enum`
- `flag`
- `flagset`

### 3.3 Type Keywords

Current built-in type keywords include:

- `i8`
- `i16`
- `i32`
- `i64`
- `u8`
- `u16`
- `u32`
- `u64`
- `f32`
- `f64`
- `isize`
- `usize`
- `byte`
- `bool`
- `char`
- `uchar`
- `string`
- `void`
- `component`
- `object`
- `interface`

### 3.4 Control-Flow Keywords

Current control-flow related keywords include:

- `if`
- `else`
- `match`
- `for`
- `in`
- `while`
- `break`
- `continue`
- `return`
- `is`
- `when`
- `assumed`

### 3.5 Logical Keywords

Wio also offers word-based logical operators:

- `and`
- `or`
- `not`

### 3.6 Module and Access Keywords

Other important keywords:

- `use`
- `as`
- `public`
- `private`
- `protected`
- `self`
- `super`

### 3.7 Reserved or Planned Keywords

The current token set already reserves names for future directions:

- `every`
- `after`
- `during`
- `wait`
- `async`
- `await`
- `coroutine`
- `yield`
- `thread`
- `loop`
- `system`
- `program`
- `type`

Most of these are not yet fully parsed as source-level features.

## 4. Literals

### 4.1 Boolean and Null Literals

Boolean literals:

```wio
true
false
```

Null literal:

```wio
null
```

#### Current Compiler Note

`null` is currently treated as compatible with all types during semantic
compatibility checks. This is convenient, but it also means nullability rules
are still broad and not yet formalized as a separate type discipline.

### 4.2 Integer Literals

Wio supports decimal, hexadecimal, binary, and octal integer literals.

Examples:

```wio
let a = 123;
let b = 0xFF;
let c = 0b101010;
let d = 0o755;
```

#### Integer Suffixes

Supported integer suffixes currently include:

- `i8`
- `u8`
- `i16`
- `u16`
- `i32`
- `u32`
- `i64`
- `u64`
- `isz`
- `usz`
- `i`
- `u`

Examples:

```wio
let a = 10i8;
let b = 10u16;
let c = 10i32;
let d = 10u64;
let e = 10isz;
let f = 10usz;
let g = 10i;
let h = 10u;
```

#### Default Integer Inference

When no suffix is provided, the current compiler infers:

- `i32` if the value fits,
- otherwise `i64` for larger signed values,
- otherwise `u64` for large positive values.

Examples:

```wio
let small = 123;         // usually i32
let bigger = 5000000000; // usually i64 or u64 depending on range
```

#### Edge Case: Negative Numbers and Signed Minima

Surface syntax still treats negative numeric forms as unary minus applied to a
literal:

```wio
let x = -123;
```

However, the current compiler now folds direct `-literal` forms early enough to
validate and lower signed minimum values correctly. In practice, these forms are
supported today:

```wio
let a: i8 = -128i8;
let b: i16 = -32768i16;
let c: i32 = -2147483648i32;
let d: i64 = -9223372036854775808i64;
let e: i8 = -128;
```

This means the front-end still models unary minus as an operator, but literal
range checking and backend lowering no longer reject signed minima such as
`-128i8`.

### 4.3 Floating-Point Literals

Wio supports decimal floats and scientific notation.

Examples:

```wio
let a = 1.0;
let b = 3.14159;
let c = 1.5e3;
let d = 4.2E-1;
```

#### Float Suffixes

Supported float suffixes currently include:

- `f`
- `f32`
- `f64`

Examples:

```wio
let a = 1.0f;
let b = 1.0f32;
let c = 1.0f64;
```

#### Default Float Inference

Without a suffix, the current compiler treats floats as `f64`.

### 4.4 Duration Literals

Wio currently recognizes duration-like numeric suffixes:

- `ns`
- `us`
- `ms`
- `s`
- `m`
- `h`

Examples:

```wio
let dt = 16ms;
let timeout = 2s;
let long_delay = 3m;
let uptime = 1h;
```

#### Current Compiler Note

At the moment, duration literals are semantically treated as `f32` values and
the backend lowers them to seconds.

Examples:

```wio
let a = 500ms; // lowered approximately to 0.5f
let b = 2m;    // lowered approximately to 120.0f
```

### 4.5 String Literals

Regular strings use double quotes:

```wio
let greeting = "Hello";
let message = "Damage dealt";
```

Supported escape sequences currently include:

- `\\`
- `\"`
- `\'`
- `\n`
- `\r`
- `\a`
- `\b`
- `\f`
- `\t`
- `\v`

Examples:

```wio
let a = "Line1\nLine2";
let b = "Quote: \"hello\"";
let c = "Tab:\tIndented";
```

#### Edge Case: Regular Strings Cannot Span Lines

A regular string that hits a newline before its closing quote is currently an
error.

### 4.6 Interpolated Strings

Interpolated strings are written with the `$"..."` form.

Examples:

```wio
let name = "Boss";
let hp = 900.0f;
let text = $"Name: ${name}, HP: ${hp}";
```

Interpolation uses `${ ... }` inside the string.

Examples:

```wio
std::io::Print($"2 + 3 = ${2 + 3}");
std::io::Print($"Position: ${self.x}, ${self.y}, ${self.z}");
```

#### Current Compiler Note

Interpolated strings are internally tokenized as alternating string fragments
and embedded expressions. They are ultimately lowered through `std::format(...)`
in the backend.

#### Edge Case: Multiline Interpolated Strings

The current lexer allows interpolated strings to span lines. That behavior is
currently tied to the `$"` form and is available today, although it should still
be considered lightly specified until formally documented elsewhere.

Example:

```wio
let block = $"First line
Second line
Value = ${123}";
```

### 4.7 Character Literals

Character literals use single quotes:

```wio
let a = 'x';
let b = '\n';
let c = '\\';
```

A character literal must contain exactly one character after escape processing.

### 4.8 Byte Literals

The current compiler contains `ByteLiteral` in the AST and token set, but the
surface syntax for byte literals is not finalized in the current lexer.

Practical guidance today:

- treat `byte` as a built-in type,
- do not rely on a dedicated byte literal syntax yet.

## 5. Type System

### 5.1 Built-in Primitive Types

Wio currently exposes the following built-in types:

- `i8`
- `i16`
- `i32`
- `i64`
- `u8`
- `u16`
- `u32`
- `u64`
- `f32`
- `f64`
- `isize`
- `usize`
- `byte`
- `bool`
- `char`
- `uchar`
- `string`
- `void`

Examples:

```wio
let hp: i32 = 100;
let speed: f32 = 5.0f;
let ok: bool = true;
let name: string = "Entity";
```

### 5.2 The Root `object` Type

Wio also has a built-in `object` type which acts as the base object concept for
the object model.

Example:

```wio
fn Inspect(target: view object) {
    // target may be any object-compatible value
}
```

### 5.3 Reference Types

Reference types are written with `ref` or `view` in type position.

Examples:

```wio
ref i32
view i32
ref Entity
view object
ref ref i32
```

`ref T` means a mutable reference-like type.

`view T` means a read-only reference-like type.

#### Edge Case: Nested Reference Types

Nested forms such as:

```wio
ref ref i32
view ref Entity
```

are accepted by the current parser and type system.

### 5.4 Dynamic Arrays

Dynamic arrays use postfix `[]`.

Examples:

```wio
let xs: i32[] = [1, 2, 3];
let names: string[] = ["A", "B", "C"];
let matrix: i32[][] = [[1, 2], [3, 4]];
```

### 5.5 Static Arrays

Static arrays use prefix bracket syntax:

```wio
[T; N]
```

Examples:

```wio
let a: [i32; 3] = [1, 2, 3];
let b: [[f32; 4]; 2] = [
    [0.0f, 1.0f, 2.0f, 3.0f],
    [4.0f, 5.0f, 6.0f, 7.0f]
];
```

#### Edge Case: Omitting the Size in Static Array Syntax

The parser currently accepts:

```wio
[i32]
```

as a static-array-like type with size `0`. This is almost certainly not what a
user should rely on. For real source code, always write:

```wio
[i32; 3]
```

for static arrays, and:

```wio
i32[]
```

for dynamic arrays.

#### Edge Case: Static Array Size Parsing

The current parser accepts integer literals for sizes as expected, but it also
accepts float literals and converts them. That should be considered an
implementation accident, not a stable language promise.

### 5.6 Function Types

Function types are written in the following form:

```wio
fn(T1, T2, T3) -> R
```

Examples:

```wio
fn(i32, i32) -> i32
fn(string) -> void
fn(ref Entity) -> bool
```

If the return type is omitted in the type form, the current parser defaults it
to `void`.

### 5.7 Dictionary Types

Although not part of your original numbered list, the current compiler already
contains first-class dictionary types:

- `Dict<K, V>` for unordered dictionaries,
- `Tree<K, V>` for ordered dictionaries.

Examples:

```wio
let scores: Dict<string, i32>;
let timeline: Tree<i32, string>;
```

### 5.8 Null Compatibility

The current compiler treats `null` as broadly compatible with most types.

Examples:

```wio
let a: view Entity = null;
let b: string = null;
let c: i32[] = null;
```

This is powerful but intentionally broad. A future language version may want a
sharper nullability model.

### 5.9 Type Aliases

The compiler has an internal `AliasType` and a reserved `type` keyword, but
source-level type alias syntax is not fully implemented yet.

Practical guidance:

- treat `type` as reserved,
- do not document a final alias syntax yet.

## 6. Variable Declarations

### 6.1 `let`

`let` declares an immutable binding.

Examples:

```wio
let hp: i32 = 100;
let title = "Boss";
```

### 6.2 `mut`

`mut` declares a mutable binding.

Examples:

```wio
mut hp: i32 = 100;
hp -= 10;
```

### 6.3 `const`

`const` is the compile-time constant form of variable declaration.

In the current v1 freeze, the language model is:

- Wio `const` is not just an immutable variable,
- Wio `const` requires a compile-time-valid initializer,
- the backend lowers it as C++ `constexpr`.

Examples:

```wio
const MaxPlayers: i32 = 16;
const Pi: f64 = 3.141592653589793;
```

#### Current Compiler Note

The semantic layer now enforces a deliberately small and safe subset:

- only scalar primitive types are supported,
- the initializer must be a compile-time scalar expression,
- such expressions may reference only other `const` declarations.

Supported value categories currently include numeric, `bool`, `char`, `uchar`,
and `byte`-like scalar primitives. Runtime containers, objects, dictionaries,
arrays, function calls, and ordinary `let`/`mut` variables are not allowed in
`const` initializers yet.

### 6.4 Type Inference

A declaration with an initializer may omit the explicit type:

```wio
let hp = 100;
let speed = 5.0f;
let alive = true;
let name = "Entity";
```

### 6.5 Arrays in Variable Declarations

Examples:

```wio
let dynamic_arr: i32[] = [1, 2, 3];
let static_arr: [i32; 3] = [1, 2, 3];
```

### 6.6 Initialization Rules

Ordinary local/global variable declarations must be initialized.

Example:

```wio
let X: i32 = 10;
mut Y: i32 = 20;
const X: i32 = 10;
```

These are valid, but:

```wio
let X: i32;
mut Y: i32;
const X: i32;
```

are errors.

For v1 semantic freeze, the rule is intentionally simple:

- `let` must have an initializer,
- `mut` must have an initializer,
- `const` must have an initializer.

Type inference therefore always happens from an initializer expression. There is
currently no ordinary local/global declaration form that reserves storage
without initializing it.

### 6.7 Member Field Rules vs General Variable Rules

Inside `component` and `object` declarations, member fields follow a different
rule than ordinary local/global declarations.

A member field must have:

- an explicit type,
- or an initializer.

Examples:

```wio
object Entity {
    hp: i32;
    name = "Boss";
}
```

But:

```wio
object Entity {
    hp;
}
```

is invalid.

## 7. References and Views

### 7.1 Reference Creation

Use `ref` in expression position to create a reference to another value.

Example:

```wio
let x = ref y;
```

The operand of `ref` must be addressable. In current compiler terms, this means
a variable, object/component member, or indexed element. A temporary expression
such as `ref (1 + 2)` is rejected before C++ generation.

When `ref` is followed by `fit`, Wio parses it as:

```wio
(ref value) fit TargetType
```

This keeps object/interface casts explicit and avoids treating
`ref value fit TargetType` as a reference to a temporary cast result.

### 7.2 Read-Only Views

Use `view` in type position to request a read-only reference-like parameter or
binding.

Examples:

```wio
fn PrintEntity(ent: view Entity) {
    ent.Render();
}

fn AcceptAnyObject(obj: view object) {
}
```

### 7.3 Chained References

Wio allows references to references.

Examples:

```wio
mut value: i32 = 42;
let a = ref value;
let b = ref a;
let c = ref b;
```

### 7.4 Mutability Propagation

The current semantic analyzer derives reference mutability from the referenced
symbol:

- referencing a mutable symbol yields a mutable reference,
- referencing an immutable symbol yields a read-only reference-like result.

Examples:

```wio
mut a: i32 = 1;
let ra = ref a; // mutable reference behavior

let b: i32 = 2;
let rb = ref b; // read-only reference behavior
```

### 7.5 Passing References to Functions

Reference parameters are part of normal function typing.

Examples:

```wio
fn Damage(hp: ref i32, amount: i32) {
    hp -= amount;
}

fn Show(hp: view i32) {
    std::console::Print($"HP = ${hp}");
}
```

### 7.6 Reference Compatibility Rules

The current compiler follows these broad rules:

- a `view` parameter may accept a compatible mutable reference,
- a mutable `ref` parameter requires a mutable source reference,
- derived-to-base reference compatibility is allowed in safe cases.

Examples:

```wio
fn Render(ent: view Entity) {
    ent.Render();
}

fn Mutate(ent: ref Entity) {
}
```

Calling `Mutate` with a read-only view should be rejected.

### 7.7 Assignment Through References

The current compiler also performs auto-dereference-style assignment in some
cases.

That means code such as:

```wio
mut x: i32 = 1;
let rx = ref x;
rx = 5;
```

is intended to behave as assignment to the referred value.

#### Current Compiler Note

This area already exists in semantic analysis and code generation, but it is
still complex enough that it deserves strong test coverage before being called
fully stable.

## 8. Numeric and Object Conversion with `fit`

### 8.1 Overview

`fit` is Wio’s explicit conversion operator.

It is used for:

- numeric conversion,
- object/interface conversion,
- pattern-style conditional binding together with `is`.

### 8.2 Numeric `fit`

Examples:

```wio
let a: i8 = 100 fit i8;
let b: f32 = 123 fit f32;
let c: i32 = 3.9f fit i32;
let d = 42 fit f64;
```

### 8.3 Narrowing and Clamping

If a numeric conversion narrows and the source value is out of range, the result
is clamped to the destination type’s range.

Examples:

```wio
let a = 500 fit i8;     // 127
let b = -999 fit i8;    // -128
let c = 1000 fit u8;    // 255
let d = -1 fit u8;      // expected to clamp to 0 in intent
```

#### Current Compiler Note

The current backend emits numeric `fit` using `std::clamp(...)` and then
`static_cast<...>`.

### 8.4 Object and Interface `fit`

Examples:

```wio
let as_damageable = ref target fit IDamageable;
let as_entity = ref target fit Entity;
```

This is intended to cover:

- upcasts,
- downcasts,
- interface casts.

#### Current Compiler Rules

- Object/interface `fit` requires both sides to be object/interface values or
  references.
- Component casts are not object/interface casts and are rejected.
- Non-numeric primitive casts such as `string fit i32` are rejected.

### 8.5 Conditional Type Binding

`fit` can be combined with `is` inside `if`:

```wio
if (target is IDamageable fit t) {
    t.TakeDamage(10.0f);
}
```

In this form:

- `target is IDamageable` is the type test,
- `fit t` introduces a branch-local bound value.

#### Current Compiler Note

The current analyzer only allows pattern-style `fit` bindings when the condition
is an `is` check. Other forms are rejected.

### 8.6 Bound Variable Behavior in `if (... is T fit x)`

Inside the successful branch:

- if `T` is an interface, the bound value behaves like the fitted interface view,
- if `T` is an object, the bound value behaves like the fitted object reference.

Examples:

```wio
if (target is Boss fit boss) {
    boss.Render();
}

if (target is IDamageable fit dmg) {
    dmg.TakeDamage(5.0f);
}
```

## 9. Type Testing with `is`

### 9.1 Overview

`is` performs a type check.

Examples:

```wio
if (target is Entity) {
}

if (target is IDamageable) {
}
```

### 9.2 Intended Scope

`is` is currently intended for:

- objects,
- interfaces.

#### Current Compiler Rules

- The left side of `is` must be an object/interface value or reference.
- The right side of `is` must name an object or interface type.
- Components and primitive values are rejected before generated C++.

### 9.3 `is` with `fit`

Examples:

```wio
if (target is Boss fit boss) {
    boss.Render();
}

if (target is IDamageable fit damageable) {
    damageable.TakeDamage(1.0f);
}
```

## 10. Expressions

### 10.1 Primary Expressions

Wio currently supports the following primary expression families:

- literals,
- identifiers,
- grouped expressions,
- array literals,
- dictionary literals,
- lambdas,
- `match` expressions,
- `self`,
- `super`,
- `null`,
- `ref ...`.

### 10.2 Member Access

Wio uses:

- `.` for instance/member-style access,
- `::` for namespace-like or type-like access.

Examples:

```wio
self.hp
super.Render()
std::io::Print("Hello")
Entity::SomeStaticLikeThing
```

#### Current Compiler Note

The parser accepts both `.` and `::` as member-access operators. Semantic
meaning depends on what the left-hand side resolves to.

### 10.3 Array Indexing

Examples:

```wio
let arr: i32[] = [1, 2, 3];
let x = arr[0];
```

The index expression must be numeric.

### 10.4 Function Calls and Constructor Calls

Examples:

```wio
let v = Vector3(1.0f, 2.0f, 3.0f);
let boss = Boss(99, "Gargantua", 1000.0f);
ProcessCombat(ref boss, 10.0f);
```

Calling a type name is treated as constructor invocation.

### 10.5 Lambdas

The current parser and analyzer support lambda expressions.

Syntax:

```wio
(x: i32, y: i32) -> i32 => x + y
```

or:

```wio
(x: i32, y: i32) -> i32 => {
    return x + y;
}
```

Examples:

```wio
let add = (x: i32, y: i32) -> i32 => x + y;
let twice = (x: i32) => x * 2;
```

#### Current Compiler Note

Lambda parameters must have explicit types semantically, even though the parser
can temporarily parse an omitted type.

### 10.6 Array Literals

Examples:

```wio
let xs = [1, 2, 3];
let ys = ["a", "b", "c"];
let nested = [[1, 2], [3, 4]];
```

#### Edge Case: Empty Array Literals

The current compiler gives empty array literals an unknown element type. This is
usable internally, but user-facing type expectations for empty arrays are still
not fully polished.

### 10.7 Dictionary Literals

The current compiler supports dictionary literals, even though they were not in
your original note list.

Unordered literal:

```wio
let dict = {
    "hp": 100,
    "mp": 50
};
```

Ordered literal:

```wio
let tree = {<
    1: "one",
    2: "two"
>};
```

#### Edge Cases

- All keys must have the same type.
- All values must have the same type.
- Empty dictionary literals currently end up with an unknown type.

### 10.8 `self` and `super`

Examples:

```wio
self.id
self.Render()
super.Render()
super.OnConstruct()
```

#### Current Compiler Note

- `self` is only valid inside component or object methods.
- `super` is only valid inside object methods that actually have a base object.

### 10.9 `match` as an Expression

Examples:

```wio
let label = match (hp) {
    0: "dead";
    1...25: "critical";
    26...100: "alive";
    assumed: "unknown";
};
```

## 11. Operator Reference and Precedence

The current parser precedence, from higher to lower, is approximately:

| Level | Operators |
|---|---|
| 13 | `fit` |
| 12 | call `()`, indexing `[]`, member access `.`, scope access `::` |
| 11 | prefix `ref`, `not`, `!`, `~` |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 8 | `<<`, `>>` |
| 7 | `...`, `..<` |
| 6 | `<`, `<=`, `>`, `>=`, `in` |
| 5 | `==`, `!=`, `is` |
| 4 | `&`, `^`, `|` |
| 3 | `&&`, `and` |
| 2 | `||`, `or` |
| 1 | `|>`, `<|` |
| 0 | assignments such as `=`, `+=`, `-=`, `*=`, `/=`, `%=` and bitwise assignment forms |

Special parse rule: `ref value fit Target` is parsed as
`(ref value) fit Target`, even though `fit` is listed above prefix `ref` in the
general precedence table. Use parentheses for the opposite meaning.

### 11.1 Arithmetic Operators

Supported arithmetic operators:

- `+`
- `-`
- `*`
- `/`
- `%`

Examples:

```wio
let a = 1 + 2;
let b = 8 - 3;
let c = 4 * 5;
let d = 20 / 2;
let e = 21 % 4;
```

### 11.2 Assignment Operators

Supported assignment operators:

- `=`
- `+=`
- `-=`
- `*=`
- `/=`
- `%=`
- `&=`
- `|=`
- `^=`
- `~=`
- `<<=`
- `>>=`

Examples:

```wio
mut x = 10;
x += 5;
x -= 1;
x <<= 2;
```

### 11.3 Comparison Operators

Supported comparison operators:

- `==`
- `!=`
- `<`
- `<=`
- `>`
- `>=`

### 11.4 Bitwise Operators

Supported bitwise operators:

- `&`
- `|`
- `^`
- `~`
- `<<`
- `>>`

### 11.5 Logical Operators

Symbolic logical operators:

- `!`
- `&&`
- `||`

Keyword logical operators:

- `not`
- `and`
- `or`

### 11.6 Range Operators

Supported range operators:

- `...` for inclusive range,
- `..<` for exclusive range.

### 11.7 Data-Flow Operators

Planned operators:

- `|>`
- `<|`

These are currently tokenized, but not considered working language features yet.

## 12. Statements and Control Flow

### 12.1 Blocks

Blocks use braces:

```wio
{
    let x = 10;
    let y = 20;
}
```

### 12.2 `if`

Basic form:

```wio
if (hp <= 0) {
    std::io::Print("Dead");
}
```

Parentheses are optional in the current parser:

```wio
if hp <= 0 {
    std::io::Print("Dead");
}
```

#### Single-Statement Then Branches

These are allowed:

```wio
if (ok)
    return;
```

### 12.3 `else`

Examples:

```wio
if (hp <= 0) {
    std::io::Print("Dead");
} else {
    std::io::Print("Alive");
}
```

#### Edge Case: `else` Requires a Block Unless It Is `else if`

The current parser handles:

- `else if (...) ...`
- `else { ... }`

but not:

```wio
else foo();
```

as a bare single-statement else branch.

### 12.4 `while`

Examples:

```wio
while (hp > 0) {
    hp -= 1;
}
```

The body may also be a single statement:

```wio
while (x > 0)
    x -= 1;
```

#### Current Compiler Note

The current semantic analyzer explicitly allows `while` conditions of:

- `bool`,
- numeric types,
- reference types,
- `null`.

So Wio currently has truthy-style `while` behavior in practice.

### 12.5 `break` and `continue`

Examples:

```wio
while (true) {
    if (done) {
        break;
    }

    if (skip) {
        continue;
    }
}
```

Both statements are only valid inside loops.

The semantic analyzer rejects both forms when used outside `while`, `for`, or
`foreach` bodies.

### 12.6 `return`

Examples:

```wio
return;
return 123;
return x + y;
```

Return statements are validated against the surrounding function’s return type.

### 12.7 `if` Condition Type Checking

Design intent says `if` should behave broadly like familiar C-family languages.

However, the current semantic analyzer is stricter for `while` than for `if`.
That means `if` condition typing still deserves dedicated tests and likely some
future tightening.

## 13. Functions

### 13.1 Declaration Syntax

General syntax:

```wio
fn Name(param1: T1, param2: T2) -> R {
    // body
}
```

Examples:

```wio
fn Add(x: i32, y: i32) -> i32 {
    return x + y;
}

fn Log(msg: string) {
    std::io::Print(msg);
}
```

### 13.2 Parameters

Parameters are written as:

```wio
name: Type
name: Type = expr
```

Examples:

```wio
fn Damage(target: ref Entity, amount: f32) {
}

fn ReadOnlyInspect(target: view object) {
}

fn Score(base: i32, bonus: i32 = 1) -> i32 {
    return base + bonus;
}
```

#### Current Compiler Note

- User-facing code should provide explicit parameter types.
- Parameters with default values must be trailing.
- Default parameters are currently supported on:
  - functions with Wio bodies,
  - methods with Wio bodies,
  - `OnConstruct(...)` constructors,
  - and declaration-only `@Native` functions.
- Default parameters are currently rejected on:
  - `Entry`,
  - module lifecycle exports such as `@ModuleLoad`,
  - and declaration-only non-`@Native` functions, including interface methods.
- Default expressions are analyzed against the declared parameter type.
- The current v1 surface does not promise parameter-dependent defaults such as
  `fn Foo(x: i32, y: i32 = x)`.
- Lowering uses generated wrapper overloads or delegating constructors rather
  than raw C++ default arguments.

### 13.3 Return Types

A return type is optional.

If omitted, the function is treated as returning `void`.

Examples:

```wio
fn Log(msg: string) {
    std::io::Print(msg);
}

fn Add(x: i32, y: i32) -> i32 {
    return x + y;
}
```

### 13.4 Overloading

Functions with the same name may coexist in the same scope and become an
overload set.

Examples:

```wio
fn Print(x: i32) {
}

fn Print(x: string) {
}
```

#### Current Compiler Note

The current overload resolution prefers:

- exact primitive matches,
- then compatible numeric matches,
- then safe reference compatibility,
- otherwise it reports ambiguity or no matching overload.

For default parameters, the current compiler also rejects overload sets where a
defaulted declaration would synthesize an arity that is already declared
explicitly.

### 13.5 Generic Functions

Wio supports the first generic function slice through angle-bracket generic
parameter lists.

Examples:

```wio
fn Identity<T>(value: T) -> T {
    return value;
}

fn PairText<T, U>(left: T, right: U) -> string {
    return $"${left}:${right}";
}
```

Current rules:

- top-level free functions support generic parameter lists,
- explicit call syntax such as `Identity<i32>(42)` is supported for non-pack
  generic functions,
- generic overload resolution uses parameter-driven deduction first and explicit
  generic arguments second,
- generic methods on `object` declarations are supported,
- generic methods on `component` declarations stay out of the current v1 slice,
- generic `interface` methods remain unsupported in v1.

### 13.6 Generic Packs and Pack Storage

Wio now supports the first broad variadic-generic slice across functions,
generic aliases, `object`, `component`, and `interface` declarations.

Examples:

```wio
fn ForwardAll<Args...>(args: Args...) -> i32 {
    return ffi::CountTypes(args...);
}

type First<Ts...> = Ts[0usize];

component Holder<Args...> {
    public pack values: Args...;
}

object Counter<Args...> {
    public storage: Holder<Args...>;

    public fn OnConstruct(values: Args...) {
        self.storage.values = values.array;
    }

    public fn First() -> Args[0usize] {
        return self.storage.values[0usize];
    }
}
```

Current rules:

- at most one generic parameter pack is supported per declaration,
- the generic pack must be the trailing generic parameter,
- a function parameter pack must be the trailing function parameter,
- top-level functions may declare their own function parameter packs via
  `fn Foo<Args...>(args: Args...)`,
- pack expansion is currently supported as a trailing function-call argument,
  such as `callee(args...)`,
- generic aliases, `object`, `component`, and `interface` declarations may carry
  a trailing generic parameter pack,
- `object` and `component` declarations may define pack storage fields via
  `pack values: Args...;`,
- pack storage fields support `.size`, `.array`, `ToStaticArray<T>()`, and
  compile-time indexing such as `values[0usize]`,
- raw value packs support `.size`, `.array`, `ToStaticArray<T>()`, and
  compile-time indexing such as `args[0usize]`,
- raw type packs support `.size`, `.array`, and compile-time type indexing such
  as `Args[0usize]`,
- native pack bridges are supported, so declaration-only `@Native` functions may
  use `fn Foo<Args...>(args: Args...) -> ...;`.

The source-level `std::meta` bootstrap layer currently includes:

- `type Head<T, Tail...> = T;`
- `type First<Ts...> = Ts[0usize];`
- `fn CountValues<Args...>(args: Args...) -> usize`
- `fn FirstValue<Head, Tail...>(head: Head, tail: Tail...) -> Head`
- `object Types<Ts...>`
- `object Values<Args...>` with a public `pack data: Args...;` field

Current v1 limitations:

- explicit type arguments are not supported on generic pack functions yet;
  deduction must come from call arguments,
- `@Apply(...)` and `@Instantiate(...)` are currently rejected on generic pack
  functions,
- export/module ABI attributes are currently rejected on generic pack functions,
- `when`/`else` clauses are currently rejected on generic pack functions,
- default parameters are currently rejected on generic pack functions,
- pack indexing currently requires a non-negative compile-time integer literal,
- pack storage is intentionally compile-time-shaped; it is not a normal mutable
  runtime array surface with methods like `Push` or `Remove`,
- richer pack meta transforms such as `Take`, `Drop`, `Zip`, `MapTypes`, and
  future const-generic indexing remain future work.

### 13.7 `when` Guards

Wio supports function-level guards via `when`.

Syntax:

```wio
fn add(x: i32, y: i32) -> i32 when (x < 100) else 100 {
    return x + y;
}
```

Meaning:

- the body only runs when the condition is true,
- otherwise the function returns the fallback value,
- for `void` functions, a bare early `return;` is used when no fallback exists.

Examples:

```wio
fn SafeAdd(x: i32, y: i32) -> i32 when (x < 100) else 100 {
    return x + y;
}

fn LogIfPositive(x: i32) when (x > 0) {
    std::console::Print("Positive");
}
```

#### Edge Cases

- Non-void functions must provide an `else` fallback when a `when` guard is
  present.
- The fallback expression must be compatible with the function return type.
- The guard condition must be a boolean, numeric, or reference-like condition.

### 13.8 Lifecycle Functions

Wio recognizes special lifecycle names:

- `OnConstruct`
- `OnDestruct`

These are used inside `component` and `object` declarations.

Examples:

```wio
component Vector3 {
    x: f32;
    y: f32;
    z: f32;

    OnConstruct(x: f32, y: f32, z: f32) {
        self.x = x;
        self.y = y;
        self.z = z;
    }
}
```

`OnConstruct(...)` may currently use trailing default parameters and lowers them
through generated delegating constructors.

### 13.9 `Entry`

The executable entry point must be named `Entry`.

`Entry` is supported only as a top-level function. It must define a Wio body and
it must return either `i32` or `void`.

`Entry` cannot declare default parameters.

Supported forms:

```wio
fn Entry() -> i32 {
    return 0;
}

fn Entry() {
    std::console::Print("Hello");
}

fn Entry(args: string[]) -> i32 {
    return 0;
}

fn Entry(args: string[]) {
    std::console::Print("Hello");
}
```

If a parameter is present, it must be exactly `string[]`.

## 14. `match`

### 14.1 Overview

`match` is a branch-selection construct that can be used both:

- as a statement,
- as an expression.

### 14.2 Basic Form

```wio
match (value) {
    1: foo();
    2: bar();
    assumed: baz();
}
```

### 14.3 Multiple Values per Case

Examples:

```wio
match (a) {
    1, 2, 3: std::console::Print("small");
    4 or 5: std::console::Print("medium");
    assumed: std::console::Print("other");
}
```

In `match`, `or` can be used as an alternative separator between case values.

### 14.4 Range Cases

Examples:

```wio
match (a) {
    0...10: std::console::Print("inclusive");
    10..<20: std::console::Print("exclusive upper bound");
    assumed: std::console::Print("fallback");
}
```

### 14.5 `assumed`

`assumed` is Wio’s default-like match arm.

Example:

```wio
match (x) {
    0: std::console::Print("zero");
    assumed: std::console::Print("not zero");
}
```

#### Current Compiler Rules

- `assumed` may appear at most once.
- `assumed` must be the last match arm.

### 14.6 `match` as an Expression

Examples:

```wio
let text = match (hp) {
    0: "dead";
    1...25: "critical";
    assumed: "alive";
};
```

### 14.7 Statement vs Expression Bodies

A match arm body is parsed as a statement.

That means you can write:

```wio
match (x) {
    1: foo();
    2: {
        foo();
        bar();
    }
    assumed: baz();
}
```

#### Current Compiler Note

The current match typing behavior is:

- if any case body is a block, the match is treated as `void`,
- if all case bodies are expression statements, the match produces a value,
- value-producing matches must include `assumed`,
- case values must be compatible with the matched value,
- value-producing arm result types must remain compatible across every arm.

#### Practical Recommendation

For value-producing matches:

- prefer expression-only arms,
- keep types consistent across all arms,
- always include `assumed`.

## 15. Object Model

Wio’s OOP model is built around:

- `component`
- `object`
- `interface`

## 16. `interface`

### 16.1 Purpose

An interface defines a contract made only of function declarations.

Example:

```wio
interface IDamageable {
    fn TakeDamage(amount: f32);
    fn IsDead() -> bool;
}
```

### 16.2 Rules

- Interface methods cannot have bodies.
- Interface methods are declarations only.
- Interfaces can be used with `@From(...)` on objects.

#### Edge Case

This is invalid:

```wio
interface IDamageable {
    fn TakeDamage(amount: f32) {
    }
}
```

Use:

```wio
interface IDamageable {
    fn TakeDamage(amount: f32);
}
```

## 17. `component`

### 17.1 Purpose

A component is a POD-like aggregation type.

Design intent:

- components are lightweight data carriers,
- components do not participate in inheritance,
- components should only contain fields plus lifecycle logic.

### 17.2 Example

```wio
component Vector3 {
    x: f32;
    y: f32;
    z: f32;

    OnConstruct(x: f32, y: f32, z: f32) {
        self.x = x;
        self.y = y;
        self.z = z;
    }
}
```

### 17.3 Rules

Design rules:

- a component cannot inherit from another type,
- a component cannot implement interfaces,
- a component cannot define ordinary methods,
- a component may define `OnConstruct` and `OnDestruct`.

The compiler reports Wio diagnostics for both component inheritance and ordinary
component methods before generated C++ is emitted.

### 17.4 Default Access for Components

Components default to `public` member access.

Example:

```wio
component Stats {
    hp: i32;
    mp: i32;
}
```

### 17.5 Custom Default Access

The `@Default(...)` attribute may override the component-wide default.

Example:

```wio
@Default(private)
component SecretData {
    token: string;
}
```

## 18. `object`

### 18.1 Purpose

Objects are the full OOP type in Wio.

An object may:

- contain data,
- define methods,
- inherit from one object,
- implement multiple interfaces.

### 18.2 Basic Example

```wio
@From(IRenderable)
@Default(public)
@GenerateCtors
object Entity {
    protected id: i32;
    protected position: Vector3;

    OnConstruct(id: i32) {
        self.id = id;
        self.position = Vector3(0.0f, 0.0f, 0.0f);
    }

    public fn Render() {
        std::console::Print($"[Entity] id=${self.id}");
    }
}
```

### 18.3 Inheritance

Wio object inheritance is expressed via `@From(...)`.

Example:

```wio
@From(Entity)
@From(IDamageable)
object Boss {
    hp: f32;
}
```

Rules:

- one base object maximum,
- multiple interfaces allowed,
- component bases are rejected,
- final object bases are rejected.

#### Current Compiler Note

The compiler currently enforces:

- no multiple object inheritance,
- no object-from-component inheritance,
- no inheritance from `@Final` objects,
- base-object default constructor availability for derived instantiation.

### 18.4 Default Access for Objects

Objects default to `private`.

Example:

```wio
object Entity {
    id: i32; // private by default
}
```

### 18.5 Overriding Default Access

Use `@Default(public)` or `@Default(protected)` to change the object-wide
default.

Example:

```wio
@Default(public)
object VisibleEntity {
    id: i32;
    hp: i32;
}
```

### 18.6 Access Modifiers

Supported explicit access modifiers:

- `public`
- `private`
- `protected`

Examples:

```wio
object Entity {
    private secret: i32;
    protected hp: i32;
    public fn Render() {
    }
}
```

### 18.7 `self` and `super`

Examples:

```wio
self.hp -= 10;
super.Render();
```

`self` refers to the current object instance.

`super` refers to the base object view.

### 18.8 Base Constructor Requirement

The current compiler requires a base object to have a parameterless constructor
for certain derived-object scenarios.

Practical guidance:

- if an object is intended to be inherited from, provide a default
  `OnConstruct()`,
- or use `@GenerateCtors` where appropriate.

## 19. `enum`, `flagset`, and `flag`

### 19.1 `enum`

`enum` is intended to play the role of a C++ `enum`.

Example:

```wio
enum DamageType {
    Fire,
    Ice,
    Poison
}
```

Underlying type may be customized with `@Type(...)`:

```wio
@Type(u8)
enum DamageType {
    Fire,
    Ice,
    Poison
}
```

### 19.2 `flagset`

`flagset` is intended to play the role of a C++ `enum class` used for bit flags.

Example:

```wio
@Type(u32)
flagset RenderFlags {
    Visible = 1,
    Selected = 2,
    ShadowCaster = 4
}
```

### 19.3 `flag`

`flag` defines a lightweight marker type.

Example:

```wio
flag Dirty;
flag NeedsSave;
```

### 19.4 Reflection Plans

You explicitly mentioned wanting default reflection support for:

- `enum`
- `flagset`

That should be documented as a future design direction, not as a current
feature.

## 20. Attributes

### 20.1 General Syntax

Attributes use `@Name(...)`.

Examples:

```wio
@GenerateCtors
@Default(public)
@From(Entity)
@Trust(Foo)
```

Attributes may appear before:

- top-level declarations,
- object and component declarations,
- interface methods,
- object and component members,
- functions.

### 20.2 Attribute Argument Shape

#### Important Current Compiler Limitation

Attribute arguments are still more restricted than full expressions, but the
current compiler reliably supports:

- plain identifiers such as `@Trust(Foo)`,
- plain type names such as `@Type(u32)`,
- type-like generic forms such as `@Apply(traits::IsInteger<T>)`,
- and interop instantiation forms such as `@Instantiate(i32, bool)`.

Reliable examples:

```wio
@From(Entity)
@Default(public)
@Type(u32)
@Trust(Foo)
use std::traits as traits;

@Apply(traits::IsInteger<T>)
@Instantiate(traits::IsNumeric<T>)
```

Potentially unreliable today:

```wio
@From(ns::Entity)
@Something(1 + 2)
```

This means attribute arguments should currently stay simple.

### 20.3 `@ReadOnly`

Marks a member as externally immutable.

Example:

```wio
object Player {
    @ReadOnly
    public id: i32;
}
```

### 20.4 `@From(...)`

Defines inheritance sources.

Examples:

```wio
@From(Entity)
@From(IDamageable)
object Boss {
}
```

Rules:

- at most one object base,
- any number of interface bases,
- component bases are rejected,
- components may not use `@From(...)`,
- final object bases are rejected.

### 20.5 `@Default(...)`

Changes declaration-local default member access.

`@Default(...)` accepts exactly one access modifier:

- `public`
- `private`
- `protected`

Any other argument is a Wio diagnostic.

Examples:

```wio
@Default(public)
object Entity {
    id: i32;
}

@Default(private)
component PackedState {
    hp: i32;
}
```

### 20.6 `@GenerateCtors`

Requests constructor generation.

Generated constructor categories:

- empty constructor,
- member constructor,
- copy constructor.

Examples:

```wio
@GenerateCtors
object Entity {
    id: i32;
    hp: f32;
}
```

### 20.7 Constructor Generation Matrix

Intended and current practical behavior:

- If no constructor is written and `@NoDefaultCtor` is absent, constructors are
  auto-generated.
- If at least one constructor is written, implicit default generation stops.
- If `@GenerateCtors` is present, the compiler generates any missing standard
  constructor forms.
- If the user already provided one form, only the missing forms are generated.

#### Edge Case: Member Constructor Matching

The current compiler recognizes a member constructor by comparing parameter types
against member field types in declaration order.

### 20.8 `@NoDefaultCtor`

Disables implicit constructor generation when no constructor is written.

Example:

```wio
@NoDefaultCtor
object Handle {
    value: i32;
}
```

### 20.9 `@Trust(...)`

Semantics:

- equivalent in spirit to C++ `friend`,
- grants trusted types access to private/protected members.

Example:

```wio
@Trust(Serializer)
object SaveData {
    private secret: string;
}
```

Rules:

- `@Trust(...)` expects object/component/interface type names,
- trusted access is checked by semantic analysis,
- trusted types may access private/protected members of the trusting type.

### 20.10 `@Final`

Rules:

- a final object cannot be inherited from.
- final inheritance is rejected by semantic analysis, not left to generated C++.

Example:

```wio
@Final
object FinalBoss {
}
```

### 20.11 `@Type(...)`

Used to select the underlying type of:

- `enum`
- `flagset`

Examples:

```wio
@Type(u8)
enum State {
    Idle,
    Run
}

@Type(u32)
flagset Bits {
    A = 1,
    B = 2
}
```

### 20.12 `@Instantiate(...)`

`@Instantiate(...)` is the current explicit-instantiation surface for generic
`@Native` and generic `@Export` functions.

Examples:

```wio
use std::traits as traits;

@Native
@CppHeader("native_generic_math.h")
@CppName(native_generic::DoubleValue)
@Instantiate(i32)
@Instantiate(traits::IsInteger<T>)
fn DoubleValue<T>(value: T) -> T;
```

Current rules:

- `@Instantiate(...)` is valid only on generic functions.
- Today it is supported only together with `@Native` or `@Export`.
- Generic pack functions are the current exception: they rely on argument
  deduction and currently reject `@Instantiate(...)`.
- Each attribute must provide exactly one argument per generic parameter.
- Each argument may be:
  - a fully concrete type such as `i32` or `string`,
  - or a supported predicate form such as `traits::IsInteger<T>` or `traits::IsNumeric<T>`.
- Predicate forms expand into concrete instance lists during semantic analysis.
- Call sites may use only instantiated generic bindings.

Current supported trait predicates live under `std::traits`:

- `std::traits::IsInteger<T>`
- `std::traits::IsNumeric<T>`

For multi-parameter generic functions, positional combinations are allowed:

```wio
use std::traits as traits;

@Instantiate(i32, bool)
@Instantiate(traits::IsInteger<T>, bool)
fn PickLeft<T, U>(left: T, right: U) -> T;
```

### 20.13 `@Apply(...)`

`@Apply(...)` is the current generic-constraint attribute surface.

It is supported on generic:

- functions,
- `type` aliases,
- `object` declarations,
- `component` declarations,
- `interface` declarations.

Examples:

```wio
use std::traits as traits;

@Apply(traits::IsInteger<T>)
fn Twice<T>(value: T) -> T {
    return value + value;
}

@Apply(traits::IsInteger<T>)
type IntList<T> = T[];

@Apply(traits::IsNumeric<T>)
object NumberBox<T> {
    public value: T;
}
```

Current rules:

- `@Apply(...)` is valid only on generic declarations.
- Generic pack functions currently reject `@Apply(...)`.
- Each attribute must provide exactly one argument per generic parameter.
- Each argument may be:
  - a fully concrete type such as `string`,
  - a supported predicate such as `traits::IsInteger<T>` or `traits::IsNumeric<T>`,
  - or a boolean constant (`true` / `false`).
- Arguments are positional. The predicate operand must target the matching
  generic parameter for that slot.

Constraint semantics today:

- entries inside one `@Apply(...)` are combined positionally,
- multiple `@Apply(...)` attributes behave as alternative allowed constraint
  rows,
- `true` means "this slot imposes no additional restriction",
- `false` means that constraint row can never match.

Example with multiple rows:

```wio
use std::traits as traits;

@Apply(traits::IsInteger<T>, string)
@Apply(traits::IsNumeric<T>, bool)
fn Accept<T, U>(value: T, tag: U) {
}
```

This accepts either:

- integer + string,
- or numeric + bool.

## 21. Access Control and Member Semantics

### 21.1 Public, Private, Protected

The current compiler enforces:

- private access only inside the same object,
- protected access inside the object hierarchy,
- public access everywhere.

### 21.2 Member Lookup

Member lookup traverses:

- the current type,
- then base types recursively.

This applies to:

- objects,
- interfaces,
- enums/flagsets as scoped members,
- namespaces.

### 21.3 Override Detection

The current compiler marks object methods as overrides when a base type already
contains a member with the same name.

#### Current Compiler Note

This is currently closer to name-based override detection than to a fully formal
signature-based override contract. That area should eventually be tightened.

## 22. Modules and Namespaces

### 22.1 `use`

`use` is the import mechanism.

Supported user-facing forms:

```wio
use std::console;
use std::console as console;
use gameplay::combat;
use gameplay::combat as combat;
use @CppHeader("native_bridge.h");
```

### 22.2 Standard Library Module Support

The standard library is source-based. `std` modules live under the configured
standard source directory and are merged before semantic analysis unless
`--no-builtin` is used.

Common current modules include:

- `std::console`
- `std::math`
- `std::collections`
- `std::strings`
- `std::fs`
- `std::path`
- `std::algorithms`
- `std::assert`

Example:

```wio
use std::console as console;

fn Entry(args: string[]) -> i32 {
    console::Print("Hello");
    return 0;
}
```

`use std::name as alias;` creates `alias` as a namespace alias in the current
scope. If a local symbol already uses the requested alias name, the compiler
reports a Wio diagnostic.

Missing standard modules are reported by Wio before semantic analysis continues:

```wio
use std::missing_module as missing; // error
```

Builtin `std` modules are source-based, but their native bridge boundary is
still intentionally narrow:

- builtin `std` sources may use `@CppHeader(...)`
- those headers must resolve from the toolchain's public runtime/sdk include surface
- builtin `std` sources must not depend on user include directories
- builtin `std` sources must not reach into private implementation files with
  parent-relative paths such as `../` or `../../`

That rule keeps packaged toolchains and repository builds aligned: if a `std`
module compiles in the repo, it should also compile from a packaged Wio install
without depending on private source layout details.

For the current v1-oriented module contract and the runtime-backed vs pure-Wio
split, see [`WIO_STD.md`](./WIO_STD.md).

### 22.3 User Modules

User modules are `.wio` files resolved from:

- the importing file's directory,
- then each configured `--module-dir` search root.

Module paths use `::` in source and map to path separators during resolution.

```wio
use combat::damage;
use combat::damage as damage;
```

User modules are merged recursively before semantic analysis. A non-aliased
module merge exposes its top-level declarations normally. An aliased import
creates an alias namespace containing the imported module's top-level exported
symbols.

If a module file cannot be found or cannot be read, the compiler emits a Wio
diagnostic and stops instead of falling through to generated C++ errors.

### 22.4 `self` and `super` in Imports

The parser accepts relative import prefixes:

- `self`
- `super`

Examples:

```wio
use self::math;
use super::shared::types;
```

`self::x` resolves relative to the current module directory. `super::x` resolves
through `..` in the module path.

### 22.5 `as`

`as` creates an explicit namespace alias for an imported module.

```wio
use std::console as console;
use gameplay::inventory as inv;
```

Alias names are local to the importing scope. Reusing an alias name that already
belongs to a different symbol is an error.

### 22.6 `realm`

`realm` declares a namespace-like scope.

```wio
realm gameplay {
    fn ScoreBonus() -> i32 {
        return 10;
    }
}

fn Entry(args: string[]) -> i32 {
    return gameplay::ScoreBonus();
}
```

Realms with the same name merge across files/modules. If the name already exists
as a non-realm symbol, the compiler reports a Wio diagnostic.

## 23. Features Mentioned in Design but Not Yet Fully Supported

### 23.1 `for`

Implemented in the first loop wave. Remaining work is semantic hardening,
diagnostic coverage, and final spec wording.

Current enforced behavior includes:

- range iteration rejects `ref` / `view` bindings,
- range step must be an integer and may not be zero,
- array step must be an integer and may not be zero,
- dictionary iteration does not support `step`,
- C-style `for` conditions currently accept `bool`, numeric, reference, and
  `null` expressions; other types are rejected.

### 23.2 `foreach`

Implemented in the first loop wave for the currently supported iterable
surfaces. Remaining work is semantic hardening, diagnostic coverage, and final
spec wording.

### 23.3 Flow Operators

The token set already contains:

- `|>`
- `<|`

but they are not yet working language features.

### 23.4 `use as`

Implemented as the explicit import-alias form:

```wio
use some::module as alias;
```

Remaining work is hardening/spec detail rather than first implementation.

### 23.5 Concurrency and Scheduling Keywords

The following are reserved or planned:

- `every`
- `after`
- `during`
- `wait`
- `async`
- `await`
- `coroutine`
- `yield`
- `thread`
- `loop`
- `system`
- `program`

### 23.6 Type Aliases

`type` is reserved and there is internal type-alias infrastructure, but the
source-language feature is not ready yet.

### 23.7 Byte Literals

Type exists, AST exists, surface syntax not finalized.

## 24. Practical Edge Cases and Recommendations

### 24.1 Always Use Semicolons

Do this:

```wio
let x = 10;
let y = 20;
```

Do not rely on newlines alone.

### 24.2 Always Put `assumed` Last in `match`

Recommended:

```wio
match (x) {
    1: foo();
    2: bar();
    assumed: baz();
}
```

### 24.3 Prefer Explicit Types for Public APIs

Even if inference is available, explicit types make:

- function parameters,
- object fields,
- module interfaces,

far easier to reason about.

### 24.4 Provide Default Constructors for Base Objects

If an object is intended to be inherited from, strongly consider:

```wio
OnConstruct() {
}
```

or:

```wio
@GenerateCtors
```

### 24.5 Keep Attribute Arguments Simple

Recommended:

```wio
@Default(public)
@Type(u32)
@Trust(Foo)
```

Avoid complex expressions in attribute argument lists for now.

### 24.6 Prefer Explicit `assumed` for Value-Producing `match`

Recommended:

```wio
let state = match (hp) {
    0: "dead";
    assumed: "alive";
};
```

### 24.7 Do Not Expect Uninitialized Ordinary Variables

This is rejected by the current compiler:

```wio
let x: i32;
mut y: i32;
```

If you need typed storage without an initializer, use a member field inside an
`object` or `component`, not an ordinary local/global declaration.

### 24.8 Keep `const` Initializers Compile-Time and Scalar

Recommended:

```wio
const Base: i32 = 4;
const Mask: i32 = (Base << 1) + 3;
```

Not currently supported:

```wio
let base: i32 = 4;
const Value: i32 = base + 1;
```

or:

```wio
const Names: string[] = ["wio"];
```

### 24.9 Use `@Trust` Only With Object/Component/Interface Types

`@Trust(...)` is semantically enforced and expects only object/component/interface
type names.

## 25. Complete Wio Examples

### 25.1 Basic Variables and Arrays

```wio
fn Entry(args: string[]) -> i32 {
    let a: i32 = 123;
    mut b: i32 = 456;
    const MaxCount: i32 = 1024;

    let inferred = 999;

    let dyn_arr: i32[] = [1, 2, 3, 4];
    let stat_arr: [i32; 4] = [10, 20, 30, 40];

    b += a;

    return 0;
}
```

### 25.2 Numeric `fit`

```wio
fn Entry(args: string[]) -> i32 {
    let a = 500 fit i8;
    let b = 1000 fit u8;
    let c = 3.75f fit i32;
    let d = 42 fit f64;

    std::io::Print($"a=${a}, b=${b}, c=${c}, d=${d}");
    return 0;
}
```

### 25.3 References and Views

```wio
fn AddOne(x: ref i32) {
    x += 1;
}

fn Show(x: view i32) {
    std::io::Print($"x = ${x}");
}

fn Entry(args: string[]) -> i32 {
    mut hp: i32 = 10;

    AddOne(ref hp);
    Show(ref hp);

    return 0;
}
```

### 25.4 Interfaces and Objects

```wio
interface IRenderable {
    fn Render();
}

interface IDamageable {
    fn TakeDamage(amount: f32);
    fn IsDead() -> bool;
}

@GenerateCtors
component Vector3 {
    x: f32;
    y: f32;
    z: f32;
}

@From(IRenderable)
@Default(public)
@GenerateCtors
object Entity {
    protected id: i32;
    protected position: Vector3;

    OnConstruct(id: i32) {
        self.id = id;
        self.position = Vector3(0.0f, 0.0f, 0.0f);
    }

    fn Render() {
        std::io::Print($"Entity ${self.id}");
    }
}

@From(Entity)
@From(IDamageable)
@Default(public)
object Boss {
    name: string;
    hp: f32;

    OnConstruct(id: i32, name: string, hp: f32) {
        self.id = id;
        self.position = Vector3(10.0f, 20.0f, 30.0f);
        self.name = name;
        self.hp = hp;
    }

    fn Render() {
        std::io::Print($"Boss ${self.name}");
    }

    fn TakeDamage(amount: f32) {
        self.hp -= amount;
    }

    fn IsDead() -> bool {
        return self.hp <= 0.0f;
    }
}
```
