# Wio Standard Library

This document defines the current v1-oriented standard library surface for Wio.
It is intentionally practical: the goal is to make it obvious which modules are
currently part of the stable user-facing surface, which modules are backed by
runtime C++ bridges, and which modules are pure Wio source modules.

For representative conformance tests tied to that stable surface, see
[`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

For practical onboarding and design Q&A around the shipped std surface, also
see:

- [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)
- [`WIO_FAQ.md`](./WIO_FAQ.md)

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

- `std` modules are written as `.wio` files under [`std/`](../std)
- some modules are pure Wio wrappers over language intrinsics
- some modules call public runtime helpers through typed `with native` and
  `using cpp::header(...)`; legacy `@Native`/`@CppHeader(...)` remains accepted
  as compatibility input during migration

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

The 0.11 foundation additionally ships `std::unicode`, `std::builders`,
`std::binary`, `std::json`, `std::encoding`, `std::concurrency`, `std::net`,
`std::log`, `std::regex`, `std::time`, `std::uuid`, `std::semver`,
`std::bigint`, `std::compression`, `std::csv`, `std::config`, `std::mime`,
`std::geometry`, and `std::localization`. Their frozen guarantees and explicit
incomplete boundaries are defined by
[`spec/WIO_STD_SPEC_0_11.md`](./spec/WIO_STD_SPEC_0_11.md); a module appearing
here does not imply that TLS/HTTP, full Unicode normalization, generic
serialization derives, or async I/O are complete.

`std::async` is a frozen 0.11 module. It provides coroutine scheduling, timers,
task state/cancellation,
worker-pool offload, combinators, and structured task groups. Its exact
pre-freeze contract and remaining hardening work are documented in
[`WIO_ASYNC_MODEL.md`](./WIO_ASYNC_MODEL.md).

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

- `std::Result<T>` is the shared fallible result model used by `std::io`,
  `std::console`, `std::fs`, and other recoverable std operations
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
- `Result<T>` supports `Map`, `MapError`, `AndThen`, `OrElse`, `Inspect`, and
  `InspectError`; `std::Flatten`, `Collect`, and `Sequence` compose nested or
  repeated results, and `ToOption` deliberately discards error detail when
  only presence matters

For `v1`, this should be treated as the sealed std error-flow model rather than
as a transition toward a second competing API family.

### 2.1.2 Filesystem Result Contract

Canonical `std::fs` operations return `Result<T>` for reads, writes, recursive
enumeration, metadata, permissions, copy/move/remove, canonicalization, and
atomic replacement. A failure carries `ResultDomain::fs`, a portable operation
code, the native OS error code, and an actionable message. Empty content,
missing content, and an operating-system failure are therefore distinct.

`Try*` and `*Raw` filesystem functions remain explicit compatibility and
low-level escape hatches. Public `std::path` and `std::fs` surfaces are tested
both from the repository and through clean installed packages on Windows and
Linux.

The unreleased 0.12 candidate adds `ReadTextAsync`, `WriteTextAsync`,
`AppendTextAsync`, `CreateDirectoriesAsync`, `RemoveAsync`, `RemoveAllAsync`,
`CopyFileAsync`, `MoveFileAsync`, `ReplaceFileAtomicAsync`,
`ListFilesRecursiveAsync`, and `MetadataAsync`. They return `Task<Result<T>>`
through ordinary `async fn` lowering, preserve the same filesystem error
domain/codes, and execute portable filesystem calls on a dedicated bounded I/O
executor rather than the continuation or generic blocking pool.

### 2.1.3 Runtime-Backed Stable Module With Explicit Caveat

- `std::process`

`std::process` is part of the intended `v1` std surface, but it carries one
explicit caveat:

- the public `Result`-based orchestration surface is intended to be stable,
- the remaining hardening work is cross-platform behavior and packaged-toolchain
  validation, not a different public API direction.

The unreleased 0.12 candidate adds `RunAsync` and `CaptureAsync`. They preserve
the synchronous Result/exit-code/output contract while waiting on the bounded
I/O executor, so process completion cannot occupy a continuation worker or an
application owner thread. Streaming pipes, signals, and forced cancellation
are not implied by these whole-process operations.

### 2.2 Mixed Stable Module

- `std::assert`
- `std::math`
- `std::convert`
- `std::chars`
- `std::strings`
- `std::resource`

`std::resource` provides `Owned<T>` for deterministic, idempotent native
cleanup and `Borrowed<T>` for explicit non-closing handle transport.
`Owned<T>.Dispose()` closes early, `Release()` transfers the raw value,
`OnDestruct` closes a still-live value at final-owner destruction, and
`LiveResourceCount()` supports leak-oriented tests and diagnostics. Close
callbacks must not throw or panic.

`std::assert` is a mixed module:

- low-level failure primitives are runtime-backed
- higher-level helpers are implemented in Wio

`std::math` is also mixed:

- the numerically sensitive core is runtime-backed through `std_math.h`
- higher-level aliases and convenience wrappers such as `Square`,
  `Clamp01`, and range predicates are implemented in Wio

The conversion/text family is mixed as well:

- `std::convert` exposes checked native parsing through Wio `Result<T>`
  wrappers
- `std::chars` provides locale-independent ASCII classification and case
  conversion
- `std::strings` remains primarily a Wio convenience module, with a small
  runtime-backed ASCII text core
- `string.ToI8()` through `string.ToUSize()`, `string.ToF32()`,
  `string.ToF64()`, and `string.ToBool()` are language-owned ergonomic
  intrinsics
- integer member conversions accept an optional base; base `0` recognizes
  `0x`, `0b`, and `0o` prefixes, while explicit bases may range from 2 to 36
- direct `To*` conversion panics with a Wio runtime diagnostic on invalid input;
  `convert::Parse*` returns `Result<T>` and `convert::TryTo*` writes through an
  output reference
- parsing trims ASCII whitespace but requires the remaining input to be
  consumed completely, so values such as `"42x"` are rejected
- `std::ToString<T>` and `convert::ToString<T>` provide generic formatting for
  primitive values, enums, arrays/maps, and Wio objects that expose a public
  `ToString() -> string` method

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
- `std::algorithms`
- `std::vector`
- `std::result`
- `std::traits`
- `std::option`
- `std::iterator`
- `std::range`

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

The collection wave also includes:

- `std::Queue<T>` / `std::queue::Queue<T>` with amortized FIFO storage
- `std::Set<T>` and `std::UnorderedSet<T>` backed by `Dict<T, bool>`
- `std::OrderedSet<T>` backed by `Tree<T, bool>`
- heterogeneous `std::Tuple<Args...>` / `std::tuple::Tuple<Args...>`
- `std::sort`, whose default `Sort` path uses the adaptive array intrinsic
  (already-sorted and reverse-sorted fast paths, insertion sort for small
  inputs, dense integral counting, and standard comparison-sort fallback)
- explicit `InsertionSort`, `SelectionSort`, and stable merge-sort helpers
- `std::span::Span`, a checked range token used together with an explicit
  `view T[]`/`ref T[]`; the source reference is deliberately not stored because
  Wio does not yet have escaping borrow lifetimes

Component extensions provide the ergonomic member surface without changing
component layout. `Span` exposes `End`, `Empty`, and `Slice`; `Vector2`,
`Vector3`, and `Vector4` expose length, dot, distance, normalization, approximate
equality, and in-place normalization methods. The original realm functions
remain available for source compatibility.

The foundation module wave also adds:

- `std::Option<T>` with `Some`, `None`, `Value`, `ValueOr`, presence queries,
  `Map`, `AndThen`, `Filter`, `OrElse`, `Inspect`, `ForEach`, `ToArray`, `Zip`,
  `ToResult`, and Result transpose helpers
- absence-oriented collection APIs return `Option`: `algorithms::First`,
  `Last`, `Find`, and `FindIndex`; `collections::First`, `Last`, and `Get`;
  `strings::First`, `Last`, and `Get`; `span::Get`; and intrinsic
  array/string/dictionary `Get`
- `std::iterator` array algorithms: `Map`, `Filter`, `Fold`, `Any`, `All`, and
  `Find`
- `std::range::IndexRange`, including member-style `Count`, `Contains`, and
  `ToArray`
- `std::encoding` for hex, Base64, and URL encode/decode operations
- `std::serialization` for JSON escaping, quoting, scalar encoding, and
  object/array composition
- `std::json` as the concise JSON-facing facade

`Option<T>` represents an expected absence such as an empty collection or a
missing match. `Result<T>` represents an operation that can fail and carries a
structured error. Strict `At` operations return the value and fail on a
missing index/key; `Get`, `First`, and `Last` return `Option` when absence is
expected. Existing `...Or` and `Try...` functions remain available for source
compatibility.

The utility and stream wave adds:

- `std::numeric` checked and saturating addition, subtraction, and
  multiplication for `i8/i16/i32/i64/isize` and
  `u8/u16/u32/u64/usize`; checked value APIs integrate with `Option<T>`
- `std::stream::StringReader` and `StringWriter` for in-memory sequential text
  processing
- `std::uuid` UUID v4 generation and format validation
- `std::semver::Version` as a one-field stack component whose `major`, `minor`,
  and `patch` values are packed into one `u64`; parsing, access, mutation,
  formatting, and ordering are supplied through extensions
- `std::log::Logger` with severity filtering, readable text output, JSON
  structured output, and console/error routing

Packed semantic versions allocate 21 bits to each numeric part. Each part is in
the inclusive range `0..2097151`. Prerelease and build metadata are rejected by
the packed parser because they cannot be represented reversibly in the `u64`.

`std::json` provides a recursive `Value` model for null, boolean, number,
string, array, and object values. `Parse` returns `Result<Value>` with source
positions in parse errors. Values provide typed fallback access, array indexing,
and object lookup through `Option<Value>`. `Write` emits compact JSON and
`WritePretty` emits configurable indentation. Parser and writer support nested
objects/arrays, escapes, numbers, literals, whitespace, syntax validation, and
round trips.

`std::traits` now provides both compiler constraints and query functions.
Built-in constraints cover integer/numeric/floating/signed/unsigned,
enum/flagset, object/component/interface, array, and reference categories.
Users may declare an empty generic interface, implement its concrete
specialization with `@From`, and use that interface as a nominal
`@Apply(UserTrait<T>)` predicate. Query functions include `IsSameType`,
constructibility checks, and the corresponding type-category checks.

### 2.3.1 Pure-Wio Stable Module

- `std::reflect`

`std::reflect` is part of the intended `v1` std surface.

### 2.3.2 Reflection Surface

`enum`, `flagset`, `component`, `object`, and `interface` declarations now have
a first-class metadata layer in [`std/reflect.wio`](../std/reflect.wio).

The stable ergonomic surface is:

- `reflect::Count<T>()`
- `reflect::Name(value)`
- `reflect::Value<T>(index)`
- `reflect::Index(value)`
- `reflect::IsValid(value)`
- `reflect::TryFromValue<T>(raw)` and strict `reflect::FromValue<T>(raw)`
- `reflect::UnderlyingType<T>()`
- `reflect::Size<T>()`
- `reflect::Has(flags, mask)`
- `reflect::HasAny(flags, mask)`
- `reflect::With(flags, mask)`
- `reflect::Without(flags, mask)`
- `reflect::Toggle(flags, mask)`
- `reflect::Clear(flags)`
- `reflect::TypeName<T>()`, `TypeKind<T>()`, `TypeSize<T>()`, and
  `TypeAlignment<T>()`
- `reflect::FieldNames<T>()`, `FieldTypes<T>()`, `FieldAccess<T>()`
- `reflect::MethodNames<T>()`, `MethodSignatures<T>()`,
  `MethodAccess<T>()`
- `reflect::BaseTypes<T>()`
- `reflect::Describe<T>()`, which composes those arrays into `TypeInfo`

Enum values also provide member-style `Value()` and `IsValid()`. Unknown raw
values received from native code remain inspectable for forward compatibility,
but validity is false and `TryFromValue` returns `None`.

This keeps common state/kind/mode style code readable without forcing all
flag-oriented operations back to raw integer math, while also giving enum and
flagset types enough metadata for stable reflection code while exposing the
same source-level metadata for generic and non-generic components, objects,
and interfaces. Generic declarations report their declared parameter names in
field/method signatures while kind, access, bases, and member shape remain
fully available for each instantiation.

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

### 2.6 Hash, Random, Regex, Time, And Buffer Modules

- `std::hash`
  - `Hash(string|byte[])` defaults to FNV-1a 64
  - explicit FNV-1a 32/64 functions are available
  - `Sha256` returns lowercase hexadecimal output and `Sha256Digest` returns
    the 32 digest bytes
  - `HashValue<T>` hashes the canonical `std::ToString<T>` representation
- `std::random`
  - `Random`/`Default` are aliases of MT19937
  - xoroshiro128+, LXM, and Wichmann-Hill generators are available as explicit
    stateful objects
  - all generators accept deterministic seeds; `Create()` uses `SystemSeed()`
- `std::regex`
  - match, find, captures, find-all, replace, split, and escaping
  - invalid patterns are returned as `Result` errors instead of leaking native
    exceptions
- `std::time`
  - system Unix clocks, monotonic clocks, sleeping, durations, UTC/local
    breakdown, leap-year/month helpers, checked `DateTime` creation, and
    ISO-8601 formatting
- `std::buffer` / `std::heap`
  - checked `ByteBuffer` cursor operations and explicit little-endian codecs
  - generation-checked `BytePool` leases reject stale/double returns
  - `Pool<T>` provides the same lifecycle discipline for typed values
  - unchecked reinterpret-style `As<T>` is intentionally not exposed; typed
    pools and explicit codecs keep ownership and representation errors visible

The language keywords `byte` and `bit` are active semantic aliases of `u8` and
`bool` respectively. New byte-oriented APIs use `byte[]` directly.

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
- `std::convert`
- `std::chars`
- `std::collections`
- `std::strings`
- `std::algorithms`
- `std::result`
- `std::traits`

### 4.1 Conversion Quick Reference

```wio
use std::convert as convert;

let decimal = "42".ToI32();
let prefixed = "0x2A".ToI32(0);

let safe = convert::ParseU16("65535");
if (safe.IsOk()) {
    let value = safe.Unwrap();
}

mut fallback = 0i32;
if (convert::TryToI32("not a number", ref fallback)) {
    // fallback was replaced
}

let hex = convert::ToHexUpper(48879u64); // "BEEF"
let values: i32[] = [1, 2, 3];
let text = convert::ToString<i32[]>(values); // "[1, 2, 3]"
```

`ParseError` distinguishes empty input, invalid format, trailing characters,
range overflow, and invalid bases. Floating-point parsing rejects non-finite and
out-of-range results. Boolean parsing accepts `true/false`, `1/0`, `yes/no`,
and `on/off` case-insensitively.

The ASCII-oriented helpers under `std::chars` deliberately avoid
locale-dependent behavior. `std::strings` adds case-insensitive comparison,
prefix/suffix composition, before/after extraction, whitespace splitting and
collapsing, capitalization, occurrence counting, and truncation helpers.

The generic algorithm utility layer includes `Distinct`, `TakeWhile`,
`SkipWhile`, `Reject`, `Chunk`, `Windowed`, `Flatten`, `Repeat`, `Range`,
`SequenceEqual`, and numeric sum helpers.

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

- [`std_contract_v1_run.wio`](../tests/std_contract_v1_run.wio)

That test intentionally exercises both families together:

- runtime-backed: `console`, `assert`, `fs`, `path`
- pure-Wio: `collections`, `algorithms`
- mixed Wio/runtime: `math`, `strings`, `convert`, `chars`

If that test breaks, it usually means either:

- the stable std module surface changed,
- a runtime-backed module stopped matching its public contract,
- or a pure-Wio helper no longer matches the intrinsic behavior it wraps.
