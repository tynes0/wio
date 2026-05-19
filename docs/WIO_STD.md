# Wio Standard Library

This document defines the current v1-oriented standard library surface for Wio.
It is intentionally practical: the goal is to make it obvious which modules are
currently part of the stable user-facing surface, which modules are backed by
runtime C++ bridges, and which modules are pure Wio source modules.

For representative conformance tests tied to that stable surface, see
[`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

---

## 0. Stability Reading

For the current `v1` freeze, the std surface should be read in four buckets:

- **Stable now**: public module names and basic behavior should be treated as
  part of the intended `v1` contract.
- **Stable with explicit caveats**: the module belongs to `v1`, but a narrow
  hardening edge is still documented on purpose.
- **Experimental**: implemented and usable, but not yet frozen as part of the
  main `v1` library contract.
- **Not part of the stable surface**: helper realms, private scaffolding, or
  future-facing bootstrap areas.

That status split matters more than whether the implementation happens to live
in pure Wio source or behind runtime-backed native helpers.

---

## 1. Design Boundary

The current Wio standard library is source-based:

- `std` modules are written as `.wio` files under [`std/`](C:/Users/cihan/RiderProjects/wio/std)
- some modules are pure Wio wrappers over language intrinsics
- some modules call public runtime helpers through `@Native` and `@CppHeader(...)`

The v1 boundary is:

- language intrinsics stay language-owned
- convenience wrappers live in `std`
- runtime-backed modules may only depend on the toolchain's public runtime/sdk
  include surface
- builtin `std` modules must not depend on private runtime implementation files
  or on user include directories

That keeps repository builds and packaged toolchains aligned.

---

## 2. Stable Module Families

### 2.1 Runtime-Backed Stable Modules

- `std::console`
- `std::io`
- `std::fs`
- `std::path`

These modules rely on public runtime headers and native bridges.

Current v1 expectation:

- their public names and basic behavior should be treated as stable
- their implementation may evolve in runtime C++ without changing Wio call sites
- diagnostics for missing native headers should come from Wio, not from generated
  C++
- when an operation can fail, the canonical API direction is `Result`-returning
  names such as `io::Open(...)` and `console::PrintLine(...)`
- compatibility `...Result` aliases may remain temporarily, but they are no
  longer the canonical user-facing names for `v1`

### 2.1.1 Shared Result Convention

- `std::Result<T>` is the shared fallible result model used by `std::io` and
  `std::console`
- `std::result` remains as a helper/compatibility realm for factories and
  legacy aliases
- `Foo!()` unwraps a `Result<T>`-returning call and panics if it contains
  an error
- `Foo?()` unwraps a `Result<T>`-returning call and propagates the
  contained error from the enclosing `Result<U>`-returning function
- this keeps fallible std APIs on one naming convention instead of splitting
  into `ReadAll` vs `ReadAllResult`
- canonical public console helpers such as `Print`, `PrintLine`, `Write`,
  `WriteJoined`, `PrintFormat`, `Capabilities`, and `GetStandardOutputInfo`
  follow this `Result` model
- `Try*` and `*Raw` names remain available as low-level escape hatches, but
  they are no longer the recommended surface for normal Wio code

For `v1`, this should be treated as the sealed std error-flow model rather than
as a transition toward a second competing API family.

### 2.1.2 Runtime-Backed Stable Module With Explicit Caveat

- `std::process`

`std::process` is part of the intended `v1` std surface, but it carries one
explicit caveat:

- the public `Result`-based orchestration surface is intended to be stable,
- the remaining hardening work is cross-platform behavior and packaged-toolchain
  validation, not a different public API direction.

### 2.2 Mixed Stable Module

- `std::assert`
- `std::math`

`std::assert` is a mixed module:

- low-level failure primitives are runtime-backed
- higher-level helpers are implemented in Wio

`std::math` is also mixed:

- the numerically sensitive core is runtime-backed through `std_math.h`
- higher-level aliases and convenience wrappers such as `Square`,
  `Clamp01`, and range predicates are implemented in Wio

Current v1 expectation:

- `Fail`, `Require`, and `Unreachable` are the runtime boundary
- `NotImplementedYet()` and `NotImplementedYet(message)` are the intentional
  development-time trap helpers for unfinished code paths
- expectation helpers such as `ExpectEqual`, `ExpectNear`, and collection/string
  assertions are part of the stable testing-oriented std surface
- math convenience wrappers should stay thin over the runtime-backed contract
- there is no separate stable `std::testing` module in v1; testing helpers live
  in `std::assert`

### 2.3 Pure-Wio Stable Modules

- `std::collections`
- `std::strings`
- `std::algorithms`
- `std::vector`
- `std::result`
- `std::traits`

These modules are currently pure Wio source and do not require native bridge
headers.

Current v1 expectation:

- they are allowed to wrap language-owned member intrinsics such as array, dict,
  tree, and string methods
- they provide convenience and naming stability, not a second independent runtime
  container implementation
- if a feature already belongs to the language, `std` should wrap it rather than
  re-implement a competing version

The vector family also lives here:

- `std::Vector2`
- `std::Vector3`
- `std::Vector4`
- helper realms such as `std::vector2`, `std::vector3`, and `std::vector4`

The intended stable direction is:

- value-type math components at the root `std::` level
- utility helpers grouped under dedicated sub-realms
- predictable scalar safety behavior for divide/modulo-oriented helpers

### 2.3.1 Pure-Wio Stable Module

- `std::reflect`

`std::reflect` is part of the intended `v1` std surface.

### 2.3.2 Enum And Flagset Surface

`enum` and `flagset` now have a stable first-class convenience layer, even
though the implementation currently lives in
[`std/reflect.wio`](C:/Users/cihan/RiderProjects/wio/std/reflect.wio).

The stable ergonomic surface is:

- `reflect::Count<T>()`
- `reflect::Name(value)`
- `reflect::Value<T>(index)`
- `reflect::Index(value)`
- `reflect::UnderlyingType<T>()`
- `reflect::Size<T>()`
- `reflect::Has(flags, mask)`
- `reflect::HasAny(flags, mask)`
- `reflect::With(flags, mask)`
- `reflect::Without(flags, mask)`
- `reflect::Toggle(flags, mask)`
- `reflect::Clear(flags)`

This keeps common state/kind/mode style code readable without forcing all
flag-oriented operations back to raw integer math, while also giving enum and
flagset types enough metadata for stable `v1` reflection code.

### 2.4 Stable-With-Caveats Pure-Wio Meta Module

- `std::meta`

`std::meta` is the current compile-time pack/meta surface for `v1`.

Current `v1` expectation:

- it is intentionally narrower than a full future const-generic transformation
  system
- it currently focuses on pack counting, pack storage, indexed pack aliases,
  explicit `Values<Args...>` / `Types<Ts...>` wrappers, simple value/type
  helpers, source-level pack capture/reset helpers, and pack-storage mutation
  helpers
- the stable `v1` wave now includes:
  - `TypeCount<Ts...>()`
  - `ContainsType<T, Ts...>()`
  - `type Second<Ts...>`
  - `type Penultimate<Ts...>`
  - `Types<Ts...>.Contains<T>()`
  - `SecondValue<Args...>(...)`
  - `PenultimateValue<Args...>(...)`
  - `Values<Args...>.Set(...)`
  - `Values<Args...>.Second()`
  - `Values<Args...>.Penultimate()`
  - `Values<Args...>.ReplaceFirst(...)`
  - `Values<Args...>.ReplaceSecond(...)`
  - `Values<Args...>.ReplaceLast(...)`
- `std::meta` pack indexing now accepts:
  - non-negative compile-time integer literals
  - same-pack `.size - N` expressions
  - top-level `const` integer declarations
  - simple compile-time integer expressions over those `const` declarations
- compile-time array conversion still uses the existing pack surface directly:
  `args.ToStaticArray<T>()` or `values.data.ToStaticArray<T>()`
- richer transforms such as `Take`, `Drop`, `Zip`, `MapTypes`, and broader
  non-pack const-generic helpers are still future work

### 2.5 Experimental Runtime-Model Helper Modules

- `std::heap`
- `std::event`

These modules sit closer to the runtime type model than to ordinary collection
helpers.

Current expectation:

- `std::heap::box<T>` is the implementation-facing heap wrapper object
- `std::Box<T>` is the canonical public alias for heap-wrapped typed values
- it is intentionally std-backed for now rather than a dedicated builtin
  keyword
- `std::event` provides the implementation-facing `any`-powered
  event/context/payload surface
- `std::Event` is the canonical public alias for the std event object used for
  userdata-style flows, handler dispatch, and small message pipelines
- the public boundary between `std::Box<T>`, `any`, and `opaque` should be
  treated as frozen:
  - `std::Box<T>` is the owned boxed Wio value path,
  - `any` is the erased Wio-owned payload path,
  - `opaque` is the foreign/native payload path,
- remaining work here is helper growth and documentation polish, not a search
  for a different boundary.

---

## 3. Language vs Std Ownership

The current ownership split is:

- language-owned:
  - array member methods
  - dict member methods
  - tree member methods
  - string member methods
  - type checking / casting syntax
  - loops / ranges / literals
- std-owned:
  - aliases and convenience wrappers
  - composed helpers such as `WriteJoined`, `EnsureParentDirectory`,
    `JoinAll`, `ExpectSequenceEqual`, `Filter`, `Map`, and `Reduce`

This matters for evolution:

- changing a language intrinsic is a language/runtime compatibility decision
- changing a std wrapper is a library API decision

---

## 4. Current Stable v1 Surface

The following should be treated as stable user-facing module names in the current
v1 direction:

- `std::console`
- `std::io`
- `std::assert`
- `std::fs`
- `std::path`
- `std::process`
- `std::math`
- `std::collections`
- `std::strings`
- `std::algorithms`
- `std::result`
- `std::traits`

The following is part of the intended stable surface, but still carries an
explicit caveat:

- `std::reflect`

The following is part of the intended stable surface, but still carries an
explicit caveat:

- `std::meta`

The following is available but still experimental / hardening-oriented:

- `std::heap`
- `std::event`

The following is intentionally not part of the current stable surface:

- `std::testing`
- private `std` helper realms created only for invalid tests
- direct inclusion of runtime private headers from builtin `std`

---

## 5. Contract Test

The combined smoke test for the current v1 std surface is:

- [`std_contract_v1_run.wio`](C:/Users/cihan/RiderProjects/wio/tests/std_contract_v1_run.wio)

That test intentionally exercises both families together:

- runtime-backed: `console`, `assert`, `fs`, `path`
- pure-Wio: `math`, `collections`, `strings`, `algorithms`

If that test breaks, it usually means either:

- the stable std module surface changed,
- a runtime-backed module stopped matching its public contract,
- or a pure-Wio helper no longer matches the intrinsic behavior it wraps.
