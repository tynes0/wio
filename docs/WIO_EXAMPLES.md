# Wio Examples Guide

This document is the release-facing map for the example set that ships with
Wio `v0.13.0`.

The goal is simple:

- if you are new, you should know which example to open first
- if you have a specific workflow, you should know which example matches it
- if you are debugging interop, you should know when to leave `examples/` and
  go to the deeper test/scenario library

---

## 1. Release Example Set

The primary release example set now lives under `examples/`.

These examples are the intended first-stop references:

1. [`examples/plain_app`](../examples/plain_app/README.md)
2. [`examples/native_app`](../examples/native_app/README.md)
3. [`examples/hybrid_module`](../examples/hybrid_module/README.md)
4. [`examples/binding_import`](../examples/binding_import/README.md)
5. [`examples/packaged_quickstart`](../examples/packaged_quickstart/README.md)
6. [`examples/static_cmake_consumer`](../examples/static_cmake_consumer/README.md)
7. [`examples/wio_observatory`](../examples/wio_observatory/README.md)
8. [`examples/atlas_desk`](../examples/atlas_desk/README.md)

They are complemented by:

- [`examples/hybrid_arena`](../examples/hybrid_arena/README.md) for a heavier
  integration sample
- `scripts/wio/` for source-based tooling examples
- `tests/native/` for focused interop scenario coverage

---

## 2. Start Here By Goal

### 2.1 I Want The Smallest Real Wio Program

Open:

- [`examples/plain_app`](../examples/plain_app/README.md)

Use it for:

- first `wio.makewio`
- first `Entry`
- first `wio project build`
- first `wio project run`

### 2.2 I Want Wio To Call Existing C++

Open:

- [`examples/native_app`](../examples/native_app/README.md)

Use it for:

- `with native`
- `using cpp::header`
- `cpp::name`
- native include/source layout

### 2.3 I Want A Mixed Wio + C++ Host Project

Open:

- [`examples/hybrid_module`](../examples/hybrid_module/README.md)

Use it for:

- `hybrid-module`
- shared module output
- host executable build from the same manifest
- `@Command`, `@Event`, and module lifecycle behavior

### 2.4 I Want To Generate Bridge Files

Open:

- [`examples/binding_import`](../examples/binding_import/README.md)

Use it for:

- `wio bind import`
- `wio bind new`
- validating generated bridge files

### 2.5 I Want To Understand Installed / Packaged Wio

Open:

- [`examples/packaged_quickstart`](../examples/packaged_quickstart/README.md)

Use it for:

- installer-first usage
- `wio env status`
- `wio env doctor --backend-smoke`
- first project and single-file run from an installed toolchain

### 2.6 I Want A Host SDK / CMake Embedding Example

Open:

- [`examples/static_cmake_consumer`](../examples/static_cmake_consumer/README.md)

Use it for:

- static library output
- direct SDK consumption
- `cmake/WioProject.cmake`
- host-side calls through the public SDK

### 2.7 I Want A Heavier Stress Example

Open:

- [`examples/hybrid_arena`](../examples/hybrid_arena/README.md)

Use it for:

- multi-file Wio modules
- nested realms
- richer module/host choreography
- reload-oriented integration reading

### 2.8 I Want A Full Modern Wio Application

Open:

- [`examples/wio_observatory`](../examples/wio_observatory/README.md)

Use it for:

- a multi-module application rather than a focused feature snippet
- const generics, Option/Result, reflection, and modern std APIs together
- JSON policy input and generated JSON report output
- native POD components exposed through direct extension method syntax
- filesystem, regex, hash, random, time, collections, span, tuple, and buffer
  composition

### 2.9 I Want A Native Desktop Application

Open:

- [`examples/atlas_desk`](../examples/atlas_desk/README.md)

Use it for:

- the `application` lifecycle and stack-resident `system` services
- asynchronous workspace scanning with coroutine cancellation and completion
- typed user attributes, modern native declarations, and component extensions
- Option/Result-driven persistence and exact-integer JSON data
- Unicode-aware filtering and an opaque native font handle
- a polished Raylib desktop UI built primarily in Wio

---

## 3. Source-Based Tooling Examples

The `scripts/wio/` folder demonstrates the "Wio can help build Wio workflows"
direction.

Representative files:

- `scripts/wio/print_file.wio`
- `scripts/wio/line_count.wio`
- `scripts/wio/run_host_interop.wio`
- `scripts/wio/run_hybrid_arena_demo.wio`

These are not the primary user examples for shipping apps, but they are the
best examples for:

- source-based tooling
- `wio file run ...`
- internal automation patterns

---

## 4. `tests/native/` As The Interop Scenario Library

`tests/native/` is still extremely valuable, but it plays a different role.

Think of it as the detailed scenario catalog for questions like:

- how do I export a field?
- how does a host bind a command or event?
- how do enum/flagset wrappers look from the SDK?
- what happens during stale wrapper / reload behavior?

If `examples/` gives the product story, `tests/native/` gives the edge-case and
host-interop microscope.

---

## 5. Release Reading

For `v0.13.0`, the intended reading order is:

1. `examples/plain_app`
2. `examples/native_app`
3. `examples/hybrid_module`
4. `examples/binding_import`
5. `examples/packaged_quickstart`
6. `examples/static_cmake_consumer`
7. `examples/hybrid_arena`
8. `examples/wio_observatory`
9. `examples/atlas_desk`

That sequence moves from:

- smallest normal app
- to native bridge
- to hybrid host/module
- to bridge generation
- to packaged toolchain usage
- to CMake embedding
- to a heavier integration sample
- to a complete modern Wio application
- to a native desktop application using the 0.11 lifecycle and async model

---

## 6. Related Docs

- getting started:
  [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)
- CLI and project commands:
  [`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md)
- interop:
  [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- SDK:
  [`WIO_SDK.md`](./WIO_SDK.md)
- project model:
  [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- troubleshooting:
  [`WIO_TROUBLESHOOTING.md`](./WIO_TROUBLESHOOTING.md)
