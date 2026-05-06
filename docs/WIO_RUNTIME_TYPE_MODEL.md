# Wio Runtime Type Model

This document captures the intended high-level runtime type model for Wio.
It complements the language reference by answering a different question:

- not only "what syntax exists?",
- but also "what kind of runtime thing is this type supposed to be?"

This matters for:

- ownership and lifetime,
- native interop,
- host embedding,
- hot reload,
- future standard-library design,
- and long-term ABI stability.

Unless stated otherwise, this document is a design decision and roadmap note.
Some parts are already implemented, while others are planned but not yet fully
shipped.

For current syntax and stabilized semantics, see
[`WIO_LANGUAGE_DRAFT.md`](./WIO_LANGUAGE_DRAFT.md).

## 1. Core Decision

Wio should model runtime values in distinct categories instead of forcing
everything into one universal "object" bucket.

The recommended categories are:

1. `component`
2. `object`
3. managed special container types such as `string`, dynamic arrays, `Dict`,
   and `Tree`
4. `opaque`
5. `box<T>` as a future heap-wrapper for value types
6. `anyref` as a future common supertype for reference-like values

The rest of this document defines what each category means.

## 2. Summary Table

| Category | Role | Ownership Style | Native Interop Style | Status |
| --- | --- | --- | --- | --- |
| `component` | inline value / POD-like data | copied by value or passed by `ref` / `view` | structural POD bridge | partially implemented |
| `object` | user-defined heap object with identity | reference/handle semantics | handle/bridge, not POD layout sharing | implemented direction, still evolving |
| `string` / dynamic array / `Dict` / `Tree` | managed runtime containers | runtime-managed | dedicated bridge, not native POD | implemented direction |
| `opaque` | foreign host payload / external handle | host-owned or externally owned | pass-through opaque ABI value | initial source-level and native pass-through slice implemented |
| `box<T>` | heap allocation for value types | heap-owned wrapper around a value | wrapper/bridge, not POD | planned |
| `anyref` | common supertype for reference-like values | reference semantics | depends on wrapped runtime value | planned |

## 3. `component`

### 3.1 Purpose

`component` is Wio's inline value type category.

It is intended for:

- small or medium-sized data aggregates,
- math types such as vectors and matrices,
- plain state blocks,
- ABI-friendly POD-like payloads,
- deterministic copy-by-value behavior.

### 3.2 Mental Model

The best mental model is:

- "close to a C/C++ `struct`",
- but with Wio syntax, attributes, and lifecycle hooks.

Components should stay lightweight and structurally simple.

### 3.3 Expected Rules

Components are expected to remain:

- non-polymorphic,
- non-inheritable,
- field-oriented,
- safe to embed into other values,
- and friendly to structural native conversion.

Ordinary instance methods do not belong to the core component model. Lifecycle
hooks such as `OnConstruct` and `OnDestruct` are the right place for component
setup/teardown logic.

### 3.4 Native Interop Role

`component` is the primary native POD bridge category.

This is already the direction of the compiler:

- declaration-level native POD components can map to C++ POD structs,
- by-value, `view`, and `ref` bridging can be structural,
- nested POD-like components can remain bridgeable.

This is the recommended long-term rule:

- if a Wio type is meant to map directly to a native `struct`, it should be a
  `component`.

### 3.5 Boundary

Components should not gradually turn into "small objects".

That means a component should not become:

- an inheritance carrier,
- a runtime-polymorphic type,
- a host-identity object,
- or a generic heap entity by default.

## 4. `object`

### 4.1 Purpose

`object` is Wio's identity-bearing, behavior-rich type category.

It is intended for:

- gameplay entities,
- stateful domain objects,
- interface implementations,
- user-defined heap-style values,
- objects that naturally participate in ownership, inheritance, and method
  dispatch.

### 4.2 Mental Model

The right mental model is closer to:

- a C# / Unity-style class,
- or a managed gameplay object,
- not a POD struct.

In other words:

- an `object` should feel like a reference-bearing entity,
- not just a larger `component`.

### 4.3 Ownership Direction

The recommended direction is that objects behave as heap/identity values.

That means:

- copying an object value should behave like copying a reference/handle, not
  cloning the full object contents by default,
- identity should matter,
- object lifetime should be bridgeable through the runtime and SDK.

This is especially important for:

- host integration,
- scripting against game-engine entities,
- hot reload,
- and interface dispatch.

### 4.4 Native Interop Role

`object` should not use POD layout interop.

Even when native interop exists for object methods, the recommended long-term
model is:

- objects cross the boundary as handles or bridge wrappers,
- not as field-for-field ABI-shared structs.

This keeps object semantics compatible with:

- identity,
- inheritance,
- interfaces,
- runtime ownership,
- and reload-safe indirection.

### 4.5 Boundary

`object` should not become the language's catch-all type bucket.

In particular:

- foreign raw payloads should not be modeled as ordinary objects,
- POD math/data types should stay components,
- and a future top-level reference supertype should not reuse the `object`
  keyword itself.

## 5. Managed Special Container Types

### 5.1 Why They Should Be Separate

Types such as:

- `string`
- `T[]`
- `[T; N]`
- `Dict<K, V>`
- `Tree<K, V>`

should not be treated as ordinary user-defined objects.

They have language-level and runtime-level behavior that is more specialized
than a normal `object`.

### 5.2 Recommended Split

The recommended split is:

- `string`, dynamic arrays, dictionaries, and trees are managed runtime
  container types,
- static arrays remain value-like aggregate types,
- container methods are allowed,
- but containers do not join the ordinary object inheritance model.

### 5.3 Native Interop Direction

These types should not pretend to be direct ABI aliases of:

- `std::string`
- `std::vector`
- `std::map`
- `std::unordered_map`

The safer long-term model is:

- Wio owns their language semantics,
- the runtime owns their operational behavior,
- and native interop uses explicit bridge logic where needed.

This keeps behavior stable across:

- backends,
- runtime revisions,
- packaging,
- and hot reload.

### 5.4 Container Identity vs Value Semantics

Static arrays are value-oriented.

Managed dynamic containers are runtime-managed and may internally use reference
storage, but source-level semantics should stay intentionally designed rather
than leaking backend STL details.

## 6. `opaque` (Renamed from `userdata`)

### 6.1 Name Choice

The recommended replacement for `userdata` is `opaque`.

Why `opaque`:

- it clearly communicates "you may carry this value, but you may not inspect
  its internals from Wio",
- it reads better than `userdata`,
- it avoids implying scripting-language baggage,
- and it scales well to future forms such as `opaque<Tag>`.

### 6.2 Purpose

`opaque` is the foreign host payload category.

It is intended for values such as:

- engine-owned handles,
- native `void*`-style payloads,
- foreign object tokens,
- callback cookies,
- user data blocks owned outside Wio,
- or host-side context values that Wio only transports.

### 6.3 Rules

The recommended source-level rules are:

- Wio cannot inspect the inside of an `opaque` value,
- Wio cannot access fields on it,
- Wio cannot perform arithmetic on it,
- it may be copied, passed, compared for equality, and checked for null-ness,
- it may cross native boundaries,
- ownership remains explicit and external to ordinary Wio object semantics.

Current implemented slice:

- source-level `opaque` is a built-in type keyword,
- equality/inequality with other `opaque` values is supported,
- equality/inequality with `null` is supported,
- native `@Native` pass-through uses `void*`,
- arithmetic, field access, and ordered comparisons are intentionally rejected.

### 6.4 Interop Model

`opaque` should be the official answer to:

- "I need to store a host object pointer,"
- "I need to pass a native callback cookie,"
- "I need a C++ `void*`-like payload,"
- "I need to keep foreign engine state attached to Wio-visible code."

This is intentionally different from:

- `component`, which is structural data,
- `object`, which is a Wio runtime object,
- `box<T>`, which is a heap wrapper around a Wio value.

### 6.5 Why Not Reuse `object`

Using `object` for this purpose would blur two very different worlds:

- Wio-managed identity-bearing objects,
- and foreign opaque host payloads.

Keeping `opaque` separate makes the language easier to reason about and keeps
the ABI cleaner.

## 7. `box<T>` (Planned)

### 7.1 Purpose

`box<T>` is the recommended future heap-wrapper for value types.

It answers a different need than `object`.

Use cases:

- heap-allocating a large component or static array,
- sharing a value through reference-style ownership,
- avoiding forced conversion of data-only types into `object`.

### 7.2 Why `box<T>` Matters

Without `box<T>`, users often force data into the object model just because
they want heap allocation.

That makes the language less clear.

`box<T>` lets us say:

- "this is still a value type,"
- "but I want it stored on the heap."

### 7.3 Native Interop Direction

`box<T>` should not try to become a POD bridge.

It is a runtime wrapper concept and should use runtime/SDK bridging rules
instead of raw layout rules.

## 8. `anyref` (Planned)

### 8.1 Purpose

If Wio eventually needs a C#-like common reference supertype, it should use a
name such as `anyref`, not `object`.

This avoids overloading the meaning of `object`.

### 8.2 Intended Meaning

`anyref` would represent:

- "some reference-like runtime value",
- not "any possible Wio value".

Components and scalar values should remain outside that category unless they are
explicitly boxed.

### 8.3 Why This Separation Matters

This keeps three distinct concepts separate:

- `object`: user-defined runtime object category
- `anyref`: possible future umbrella reference type
- `opaque`: foreign host payload

That separation is healthy for both language clarity and ABI design.

## 9. Native Interop Policy

The recommended policy is:

### 9.1 `component`

Use structural conversion.

This is the right place for:

- math structs,
- POD data packets,
- save/load snapshots,
- compact data bridges.

### 9.2 `object`

Use handle/bridge interop.

This is the right place for:

- gameplay entities,
- interface-capable stateful objects,
- engine-script object interaction,
- hot-reload-aware wrappers.

### 9.3 Managed Containers

Use dedicated runtime-aware interop rather than pretending they are direct STL
aliases.

### 9.4 `opaque`

Use pure pass-through ABI transport.

This is the right place for:

- host-owned payloads,
- tokens,
- pointers,
- callback user context,
- foreign engine handles.

## 10. Practical Recommendations

### 10.1 Use `component` When

- the type is primarily data,
- copying by value makes sense,
- native POD interop matters,
- identity and inheritance do not matter.

### 10.2 Use `object` When

- the type has behavior and identity,
- inheritance or interfaces matter,
- runtime ownership matters,
- host/runtime bridging should happen through handles.

### 10.3 Use Managed Containers When

- the type is text or collection-oriented,
- the runtime needs to control semantics,
- container APIs matter more than object inheritance.

### 10.4 Use `opaque` When

- Wio should carry a foreign payload without understanding it,
- the host owns the meaning of the payload,
- the value resembles a `void*`, token, or engine handle.

### 10.5 Use `box<T>` Later When

- the value is not conceptually an object,
- but heap allocation is still desired.

## 11. Current Direction vs Future Work

### 11.1 Already Compatible with This Model

The current compiler/runtime direction already fits much of this design:

- `component` vs `object` is already a real language distinction,
- object inheritance is separate from component composition,
- native POD bridging is already the natural home of `component`,
- exported object/component reflection already treats the categories
  differently,
- managed containers already behave like special runtime-backed types rather
  than ordinary user-defined objects.

### 11.2 Still Planned

The following items are still future-facing:

- a real `opaque` type in the source language,
- `box<T>`,
- `anyref`,
- final container/top-reference categorization details,
- full native object policy beyond the current bridge slices.

## 12. Decision Summary

The recommended long-term Wio runtime type model is:

1. `component` is the value/POD/data category.
2. `object` is the identity/heap/behavior category.
3. `string`, arrays, dictionaries, and trees are managed special container
   types.
4. `opaque` is the foreign host payload category and replaces the old
   `userdata` naming idea.
5. `box<T>` is the future heap wrapper for value types.
6. `anyref` is the future name for a common reference supertype if one is added.

This keeps the language readable, the runtime model consistent, and native
interop much easier to reason about.
