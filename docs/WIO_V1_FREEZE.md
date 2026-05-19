# Wio v1 Freeze Snapshot

This document is the short, practical freeze snapshot for the planned Wio
`v1.0.0` language surface.

It is intentionally smaller than the full language reference. The goal is not
to restate every rule, but to answer four questions quickly:

1. What is part of the intended `v1` language contract?
2. What is available, but still in hardening / polish mode?
3. What is explicitly outside `v1`?
4. What kinds of changes are still acceptable before the `v1` tag?

For full details, see:

- [`WIO_LANGUAGE_DRAFT.md`](./WIO_LANGUAGE_DRAFT.md)
- [`WIO_STD.md`](./WIO_STD.md)
- [`WIO_RUNTIME_TYPE_MODEL.md`](./WIO_RUNTIME_TYPE_MODEL.md)
- [`WIO_SDK.md`](./WIO_SDK.md)

---

## 1. Freeze Policy

The current `v1` rule is:

- behavior may still be hardened,
- diagnostics may still improve,
- conformance tests may still expand,
- documentation may still become stricter,
- but the user-facing meaning of stable items should not be casually redesigned.

In short:

- bug fixes are welcome,
- semantic tightening is welcome when it matches existing intent,
- large model pivots should now be treated as post-`v1` work unless they fix a
  release blocker.

---

## 2. Intended Stable Core For v1

The following should now be treated as part of the intended `v1` language
contract.

### 2.1 Declarations and Type Model

- `realm`
- `use`
- `component`
- `object`
- `interface`
- `enum`
- `flagset`
- `type` aliases, including generic aliases
- `let`, `mut`, and `const`
- arrays, dictionaries, trees, and the current pack/variadic surface

### 2.2 Function and Call Surface

- ordinary free functions
- object methods
- constructors through `OnConstruct`
- `@Native`, `@CppHeader`, `@CppName`, and the current native bridge model
- `@Apply(...)` generic constraints
- explicit generic calls such as `Foo<T>(...)`
- result sugars `Foo!()` and `Foo?()`

### 2.3 Reference and Runtime Reference Surface

- `ref`
- `view`
- `deref`
- auto-read in value contexts for readable references
- `std::Box<T>`
- `any`
- `opaque`

The intended `v1` reading model is:

- `ref` creates a readable/writable reference,
- `view` creates a readable reference,
- `deref` removes exactly one reference layer,
- value contexts may auto-read a readable reference when that matches the
  expected type.

### 2.4 Expressions and Control Flow

- arithmetic and logical expressions
- assignment
- `if`, `while`, `for`, `match`
- range expressions and range-based iteration
- `is` / `fit`
- string interpolation

### 2.5 Operator Overloading

The operator overloading surface should now be treated as part of the intended
`v1` contract:

- member and free operator overloads
- unary, binary, and assignment operator overloads
- conversion operator overloads through `fit`
- subscript operator overloads through `[]`
- call operator overloads through `()`
- generic operator overloads

### 2.6 Generics

The following generic slice is intended to be in `v1`:

- generic free functions
- generic aliases
- generic `object`, `component`, and `interface` declarations
- explicit generic argument passing
- constructor deduction for generic `object` and `component` declarations
- `@Apply(...)`-based constraint checking
- the current variadic/pack slice
- call-site concrete validation for generic bodies

The current `v1` generic intent is:

- generics are part of the language contract,
- generic hardening is still active,
- but the broad user-facing model should now be considered frozen.

---

## 3. Stable With Explicit Caveats

The following areas are part of the intended `v1` surface, but still carry
known caveats that should be resolved as polish rather than redesign.

### 3.1 `const`

`const` is in `v1`, but intentionally small:

- compile-time only
- scalar primitive values plus enum/flagset values
- compile-time evaluable initializers only

This is a deliberate `v1` boundary, not an accident.

### 3.2 `Result`

`std::Result<T>` plus `!` / `?` is the intended official fallible-flow model.
The remaining work is surface polish and documentation alignment, not a search
for a second error model.

### 3.3 `any / Box / opaque`

These are in `v1`, but must be treated as a disciplined runtime reference
family:

- `std::Box<T>` = owned boxed Wio value
- `any` = runtime-erased Wio-owned payload
- `opaque` = foreign/native handle

The boundary is stable; remaining work is mainly docs, tests, and SDK polish.

### 3.4 Enum / Flagset

Enums and flagsets are in `v1` with:

- native support
- `const` compatibility
- reflection support

The remaining freeze work is to finish the exact reflection and SDK contract.

### 3.5 Generated Code and Tool Output Policy

The intended `v1` tooling behavior is:

- ordinary compiles treat generated `.wio.cpp` files as intermediate artifacts,
- `--emit-cpp` is the explicit opt-in path that keeps generated C++ on disk,
- `wio file run ...` and source-based Wio workflow tools use hidden
  `.wio-build/file-run/` outputs instead of dropping executables beside source
  files.

The remaining work here is mainly cross-platform validation and packaging
polish, not a search for a different model.

---

## 4. Explicitly Outside v1

The following should currently be treated as post-`v1` unless a later release
decision changes that on purpose.

- `const generics`
- `std::meta` wave 3 as a full compile-time system
- generic defaults
- partial specialization
- generalized implicit user-defined conversions
- user-defined `operator->`
- broader async/task/concurrency language features
- a larger algebraic-data-type / pattern-matching redesign

These items are not “forgotten”; they are intentionally not part of the current
freeze target.

---

## 5. Areas Still In Active Hardening

These are the main language-adjacent items still expected to move before the
`v1` tag, but they should move inside the already chosen model:

- generic diagnostics and edge-case validation
- doc/spec tightening
- std module stability labeling
- SDK enum/flagset reflection surface
- cross-platform tooling validation

---

## 6. Practical Release Reading

When discussing new work before `v1`, use this rule of thumb:

- if it strengthens an item listed in section 2, it is probably `v1` work,
- if it clarifies a caveat listed in section 3, it is probably `v1` work,
- if it belongs to section 4, it should probably wait unless it unblocks a
  release blocker.
