# Wio Interop Guide

This guide covers the practical native and host interop story in Wio.

Use it when you want to answer questions like:

- How does Wio call C++?
- How does C++ call Wio?
- When do I use `native`, `export::c`, commands, or events?
- How do exported `object` and `component` types appear in the SDK?
- What is safe across hot reload, and what is intentionally generation-bound?

For the formal SDK contract, see [`WIO_SDK.md`](./WIO_SDK.md).

For the formal project/build/package story, see
[`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md).

---

## 1. The Four Main Interop Directions

Wio currently has four important interop paths.

### 1.1 Wio -> Native C++

Use this when Wio source needs to call existing C++.

Canonical surface:

- `with native`
- `using cpp::header(...)`
- `cpp::name(...)`

### 1.2 Native C++ -> Wio Exports

Use this when a host wants to call named Wio functions directly.

Surface:

- `@Export`
- SDK export loading

### 1.3 Native C++ -> Wio Commands And Events

Use this when the host wants semantic discovery instead of raw symbol names.

Surface:

- `@Command("name")`
- `@Event("name")`
- module API registries

### 1.4 Native C++ -> Exported Objects And Components

Use this when the host wants runtime reflection over structured Wio state.

Surface:

- exported `object`
- exported `component`
- `WioObjectType`
- `WioComponentType`
- `WioObject`
- `WioComponent`

---

## 2. Calling Native C++ From Wio

The normal pattern is:

```wio
using cpp::header("native_math.h");

fn Multiply(lhs: i32, rhs: i32) -> i32
    with native, cpp::name(native_math::Multiply);
```

This means:

- Wio does not generate the implementation
- the generated backend C++ includes `native_math.h`
- the call lowers to `native_math::Multiply(...)`

### 2.1 What `cpp::header` Means

`using cpp::header("...")` adds a public include edge to generated backend C++.

That header must be reachable through:

- project include directories
- package include directories
- `--include-dir`
- project manifest include settings

### 2.2 What `cpp::name` Means

`cpp::name(...)` binds the Wio declaration to a concrete native symbol or C++
identifier path.

Examples:

```wio
fn Multiply(lhs: i32, rhs: i32) -> i32
    with native, cpp::name(native_math::Multiply);

fn WriteLine(value: string)
    with native, cpp::name(wio::runtime::std_console::WriteLine);
```

### 2.3 How To Build Native Inputs

Repo-local single-file example:

```powershell
wio file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
```

Project-based workflows should prefer:

- manifest include dirs
- manifest native sources
- manifest link dirs
- manifest link libraries

instead of large ad hoc command lines.

### 2.4 When To Use It

Use native imports when:

- C++ already owns the implementation
- the operation is system/platform/runtime specific
- you are bridging an existing library
- Wio should remain a thin front-end over a native subsystem

### 2.5 Native Component Extensions

C++ free functions that receive a declaration-level native component can be
presented as extension methods without adding methods to the POD type:

```wio
using cpp::header("vector.h");

component Vector with native, cpp::name(native::Vector) {
    x: f32;
    y: f32;
}

extension VectorNative for Vector {
    public view fn Length() -> f32
        with native, cpp::name(native::Length);

    public ref fn Translate(x: f32, y: f32)
        with native, cpp::name(native::Translate);
}
```

`value.Length()` passes `value` as the first `const Vector&` argument, with a
`const Vector*` fallback for C-style APIs. `value.Translate(...)` uses
`Vector&`, with a `Vector*` fallback. Reference overloads take precedence and
the receiver is never copied. Native borrow returns are still rejected; return
an owning value or opaque handle instead.

Legacy `@Native`, `@CppHeader`, and `@CppName` syntax remains accepted during
the compatibility window. It describes the same metadata, but documentation,
generated bindings, and new source should use `with` and `using`.

---

## 3. Exporting Wio Back To A Host

If the host wants to call Wio, the narrow base mechanism is `export::c`.

Example:

```wio
fn AddNumbers(lhs: i32, rhs: i32) -> i32
    with export::c, cpp::name(WioAddNumbers) {
    return lhs + rhs;
}
```

This produces a host-visible bridge export.

### 3.1 Export Design Intent

The current `v1` export intent is:

- exports are narrow and ABI-oriented
- `export::c` is the low-level escape hatch
- commands/events provide higher-level discovery on top

### 3.2 When To Use `@Export`

Use `@Export` when:

- the host already knows exactly which function it wants
- you are making a small explicit bridge
- you want static or dynamic host loading without a larger reflection surface

---

## 4. Commands And Events

Commands and events make the host-facing API more semantic.

Example:

```wio
@Command("counter.add")
fn AddCounter(value: i32) -> i32 {
    return value + 1;
}

@Event("game.tick")
fn OnTick(deltaTime: f32) {
}
```

Why this matters:

- symbol names are no longer the only discovery mechanism
- hosts can look up semantic names
- command/event registries survive code organization changes better than raw
  exported names alone

Use commands when:

- the host invokes named actions
- you want a semantic command catalog

Use events when:

- the host broadcasts lifecycle or simulation signals
- more than one listener may care about the same logical event

---

## 5. Exported Objects And Components

This is the richer structured-data layer.

Example mental model:

- `component` is value-like structured data
- `object` is handle/identity-bearing structured state

The SDK can discover exported types and then construct or inspect them.

Example host shape:

```cpp
auto enemyType = module.load_object("Enemy");
auto enemy = enemyType.create();

enemy.set("hp", 40);
auto hp = enemy.get<std::int32_t>("hp");
```

### 5.1 What The Host Can See

For exported `object` / `component` types, the SDK currently supports:

- field metadata
- method binding
- typed field get/set
- dynamic field get/set
- nested object/component wrappers
- enum/flagset wrappers
- arrays, dicts, trees, and function fields

### 5.2 Ownership Model

Important rule:

- top-level created wrappers may own their handles
- nested wrappers are usually borrowed
- reload-sensitive wrappers are generation-bound

That is deliberate. The SDK is trying to be predictable, not magical.

---

## 6. Enum And Flagset Interop

Enum and flagset are now first-class across both std reflection and the SDK.

Host-side wrappers:

- `WioEnum`
- `WioFlagset`

This matters because the host does not need to flatten everything to raw
integers immediately.

The host can keep:

- symbolic names
- member index
- underlying scalar value
- type identity

Use that when building:

- editors
- debug UIs
- inspector tools
- serialization or tooling that wants readable symbolic state

---

## 7. Static Vs Shared

Wio can participate in both models.

### 7.1 Static

Use static output when:

- you want the host and Wio linked into one final executable
- deployment simplicity matters
- you do not need runtime DLL swapping

Good reference:

- [`examples/static_cmake_consumer`](../examples/static_cmake_consumer/README.md)

### 7.2 Shared

Use shared output when:

- you want runtime loading
- you want hot reload
- you want tool/editor style module replacement

Good reference:

- module lifecycle tests in `tests/native/`
- hot reload wrappers in the SDK

---

## 8. Hot Reload

Hot reload is an SDK-level workflow, not a promise that every wrapper remains
valid forever.

Stable mental model:

- top-level module callables loaded from `HotReloadModule` can be reacquired
  safely through the wrapper
- object/component/field wrappers are generation-bound
- stale wrappers fail fast instead of quietly calling old unloaded code

This is the right tradeoff for `v1`:

- safe enough for tool/editor workflows
- explicit enough not to hide lifetime boundaries

---

## 9. Interop Do And Do Not

### 9.1 Do

- keep `with native` declarations bodyless
- keep public bridge headers stable
- prefer commands/events when names matter semantically
- use exported objects/components when the host needs structured reflection
- reacquire generation-bound wrappers after reload
- treat `opaque` as the foreign handle category

### 9.2 Do Not

- rely on private runtime headers from user code
- treat stale object/component wrappers as reload-safe
- assume `ref` / `view` field export behavior is part of the stable host ABI
- flatten enum/flagset to integers too early if symbolic identity matters

---

## 10. Where To Look Next

Use this map:

- full host SDK contract:
  [`WIO_SDK.md`](./WIO_SDK.md)
- build, package, manifests, packaged layout:
  [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- runtime type design:
  [`WIO_RUNTIME_TYPE_MODEL.md`](./WIO_RUNTIME_TYPE_MODEL.md)
- repo examples:
  [`WIO_EXAMPLES.md`](./WIO_EXAMPLES.md)
