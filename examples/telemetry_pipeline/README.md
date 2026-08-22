# Telemetry Pipeline

This is a deliberately substantial end-to-end interop project:

```text
C++ native analytics -> Wio policy and state -> Wio SDK -> C++ host
```

It models a small production telemetry engine rather than a toy calculator.
Raw samples enter a native C++ analytics layer, Wio applies calibration and
alert policy, and a separate C++ host consumes only the generated module ABI
and public SDK.

## What it exercises

- a native C++ POD imported as a Wio `component`
- `view` and `ref` native extension methods over `const T&` and `T&`
- a multi-file Wio shared module
- exported objects and components with constructors, fields, and methods
- Unicode `text`, dynamic arrays, dictionaries, nested components, enums, and flagsets
- fixed-width scalar values crossing both boundaries, including an internal
  native/Wio `u64` fingerprint folded into a stable SDK-facing `u32`
- module lifecycle, typed commands, and multi-argument events
- SDK metadata inspection and dynamic field access
- native-derived values returning to C++ through the SDK

## Layers

- `native/`: deterministic calibration, scoring, classification, and FNV-1a
  sample fingerprinting implemented in C++.
- `wio/telemetry_native.wio`: native component and extension declarations.
- `wio/telemetry_model.wio`: Wio domain model, policies, rolling history, and
  exported reflected types.
- `wio/module.wio`: lifecycle, command, and event-facing module API.
- `host/main.cpp`: independent C++ SDK consumer and executable acceptance test.

## Run

From the Wio repository root:

```powershell
$env:WIO_ROOT = (Get-Location).Path
build\app\Debug\wio.exe project describe --project .\examples\telemetry_pipeline
build\app\Debug\wio.exe project build --project .\examples\telemetry_pipeline
build\app\Debug\wio.exe project run --project .\examples\telemetry_pipeline
```

The host exits non-zero on any ABI, metadata, native calculation, lifecycle,
container, Unicode, enum, or flagset mismatch. A successful run prints one
summary line beginning with `Telemetry pipeline:`.

## Current boundary exposed by this example

On 64-bit Windows, `std::uint64_t` and `std::uintptr_t` can be the same C++
type. The current typed SDK therefore cannot always distinguish Wio `u64` from
`usize` by a C++ function signature alone. This example keeps the full 64-bit
timestamp and FNV-1a calculation across the native C++/Wio boundary, then
publishes stable `u32` values to the typed SDK host. Resolving that ambiguity
with explicit ABI marker types belongs to the v0.14 SDK value-parity work.
