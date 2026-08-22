# Wio SDK Evolution Plan

Status: pre-v1 parity plan for Wio `v0.13.0` and later. The current stable SDK contract
remains in [`WIO_SDK.md`](./WIO_SDK.md); this document tracks the work required
before Wio `1.0.0`.

## 1. Product and version rule

The SDK is part of the Wio product, not an independently aging compatibility
sample. Starting with the next release, the published SDK product version must
match the compiler, runtime, standard library, CLI, package, and VS Code
extension version.

The product version and raw ABI revision remain different concepts:

- `Wio SDK 0.x.y` identifies the release and documented source-level surface;
- `WIO_MODULE_API_DESCRIPTOR_VERSION` identifies the binary descriptor layout;
- capability bits and descriptor sizes negotiate optional ABI features;
- a host must receive a precise compatibility diagnostic instead of reading an
  unknown layout or silently dropping a feature.

The ABI revision changes only when the binary contract requires it. It does not
need to increase merely because the product version increases.

## 2. Meaning of Wio 1.0 parity

Before Wio `1.0.0`, every stable, host-observable Wio language/runtime feature
must have one of these outcomes:

1. a documented raw C ABI representation and ergonomic C++ wrapper;
2. an explicitly opaque handle with documented ownership and operations; or
3. a compile-time/export-time diagnostic explaining why that declaration
   cannot cross the host boundary.

Parity does not mean exposing parser, semantic-analysis, code-generation, or
other compiler internals. It means that a valid export never reaches the host
as an undocumented hole, an `UNKNOWN` placeholder, or a runtime "not yet
supported" path.

## 3. Required parity matrix

### 3.1 Values and types

- all primitive numeric/boolean/byte types;
- UTF-8 `string` and the planned Unicode-semantic `text` value, with explicit
  encoding and lifetime rules;
- enum and flagset identity, names, raw values, and unknown-native values;
- Option, Result, UnitResult, tuple, fixed array, vector, map, ordered/unordered
  set, queue, span, and supported views;
- exported object, component, interface, opaque, Box, and any values;
- nested combinations of the supported families;
- concrete type-generic and const-generic instantiations without promising a
  cross-language template ABI.

### 3.2 Calls and ownership

- functions, overloads, object methods, component extensions, commands,
  events, and callbacks;
- explicit ownership for owned, shared, borrowed `view`, mutable `ref`, nullable,
  move-only, and native-resource handles;
- no borrowed wrapper may outlive its module, owner, call frame, or reload
  generation;
- callback userdata, native thread entry, panic containment, and event-loop
  affinity must be defined and testable.

### 3.3 Reflection and metadata

- type, field, method, parameter, generic-instantiation, enum/flagset, and
  attribute metadata;
- typed attribute arguments, retention visibility, derives, and behavioral
  attribute descriptors where host observation is permitted;
- dynamic get/set/invoke for every advertised kind;
- stable IDs and deterministic lookup rules across compatible rebuilds;
- clean rejection of metadata requiring a newer descriptor capability.

### 3.4 Async and application integration

- Task/coroutine completion, result, failure, cancellation, and deadline
  observation without forcing the host thread to block;
- explicit polling, callback/continuation, and opt-in blocking adapters;
- owner/main-executor pumping for embedded GUI and application hosts;
- application/system lifecycle entry points, orderly exit, and reverse close;
- safe module unload only after SDK-visible work and leases are resolved.

### 3.5 Reload and diagnostics

- generation-aware wrappers for every exported handle family;
- explicit save/restore compatibility and schema/version reporting;
- stable SDK error categories with symbol/type/source context;
- machine-readable capability, product-version, ABI-version, and feature
  inspection;
- deterministic stale-binding behavior without calling unloaded code.

## 4. Delivery slices

1. **Inventory and version surface**: generate a language/std-to-SDK parity
   table, add product version queries, retain independent ABI negotiation, and
   fail export generation for unsupported public shapes.
2. **Value parity**: finish nested dynamic values, containers, Option/Result,
   interfaces, opaque/Box/any, Unicode text, and concrete generic instances.
3. **Ownership and invocation**: freeze `view`/`ref`, callbacks, extensions,
   events, move-only resources, and thread-affinity contracts.
4. **Async/application parity**: expose non-blocking task control, cancellation,
   executor pumping, and lifecycle hosting.
5. **Reflection and reload parity**: expose attributes and complete stable IDs,
   capability negotiation, state migration, and stale-wrapper coverage.
6. **1.0 qualification**: run the full matrix on Windows, Linux, and macOS,
   across supported compilers, static/shared modules, debug/release builds, and
   x64/ARM64 where available.

### 4.1 Progress after the 0.13 SDK parity sprint

The 0.13 foundation now completes these parts of slices 1 and 2:

- public SDK, generated module, compiler/runtime/std/CLI, and release manifest
  all report product version `0.13.0`;
- ABI descriptor version 7 publishes descriptor size, product version, stable
  type IDs, generic arguments, Unicode text, and machine-readable capability
  bits;
- [`WIO_SDK_0_13_PARITY_MATRIX.md`](./WIO_SDK_0_13_PARITY_MATRIX.md) records an
  explicit bridge, metadata, host-value, opaque, or deferred outcome;
- `wio_features.h` exposes the same distinction programmatically;
- `wio_values.h` supplies current host semantics for text, Option, Result,
  UnitResult, tuple, queue, sets, span, buffers, pools, Box, and any;
- generated `text` fields support typed and dynamic round-tripping;
- concrete std and user generic instantiations retain ordered arguments instead
  of collapsing into an unknown type.

Still open before slice 1/2 can be called fully closed:

- export-time diagnostics for public shapes that have neither a bridge nor an
  explicitly documented metadata/opaque outcome;
- recursively nested dynamic values for every advertised collection/value
  combination;
- retained typed-attribute metadata and its capability/version rules;
- interface, Box, any, and move-only resource adapters where metadata alone is
  insufficient for real host invocation.

After the parity baseline lands, each Wio release updates the SDK in the same
release PR and package. A release cannot be called complete when it adds a
host-observable stable feature without its SDK representation, explicit opaque
boundary, or export diagnostic.

## 5. Release gates

- C ABI headers compile as C-compatible declarations where promised and the
  ergonomic wrapper compiles under every supported C++ toolchain;
- generated descriptor layouts match independent C/C++ layout probes;
- old compatible hosts reject or negotiate new modules safely;
- unsupported exports fail during Wio compilation, not after module loading;
- SDK examples and real host applications run against the packaged toolchain;
- headers, libraries, docs, examples, version metadata, and licenses are
  included and verified in every distribution;
- Wio, SDK, CLI/package, and VS Code release versions are checked together in
  CI.

