# Wio Standard Library

This document defines the current v1-oriented standard library surface for Wio.
It is intentionally practical: the goal is to make it obvious which modules are
currently part of the stable user-facing surface, which modules are backed by
runtime C++ bridges, and which modules are pure Wio source modules.

For representative conformance tests tied to that stable surface, see
[`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

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
- `std::fs`
- `std::path`

These modules rely on public runtime headers and native bridges.

Current v1 expectation:

- their public names and basic behavior should be treated as stable
- their implementation may evolve in runtime C++ without changing Wio call sites
- diagnostics for missing native headers should come from Wio, not from generated
  C++

### 2.2 Mixed Stable Module

- `std::assert`

`std::assert` is a mixed module:

- low-level failure primitives are runtime-backed
- higher-level helpers are implemented in Wio

Current v1 expectation:

- `Fail`, `Require`, and `Unreachable` are the runtime boundary
- expectation helpers such as `ExpectEqual`, `ExpectNear`, and collection/string
  assertions are part of the stable testing-oriented std surface
- there is no separate stable `std::testing` module in v1; testing helpers live
  in `std::assert`

### 2.3 Pure-Wio Stable Modules

- `std::math`
- `std::collections`
- `std::strings`
- `std::algorithms`

These modules are currently pure Wio source and do not require native bridge
headers.

Current v1 expectation:

- they are allowed to wrap language-owned member intrinsics such as array, dict,
  tree, and string methods
- they provide convenience and naming stability, not a second independent runtime
  container implementation
- if a feature already belongs to the language, `std` should wrap it rather than
  re-implement a competing version

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
- `std::assert`
- `std::fs`
- `std::path`
- `std::math`
- `std::collections`
- `std::strings`
- `std::algorithms`

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
