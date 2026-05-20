# Wio Examples Guide

This document maps the current example and reference scenarios in the
repository.

The goal is to answer:

- which example should I open first?
- which example covers my workflow?
- which files are "real product" examples vs test-oriented scenario coverage?

---

## 1. Example Families

Wio examples currently come from three places:

1. `examples/`
2. `scripts/wio/`
3. `tests/native/` and selected conformance tests

They are not all the same thing.

### 1.1 `examples/`

These are the closest thing to user-facing showcase projects.

### 1.2 `scripts/wio/`

These are source-based workflow helpers written in Wio itself.

### 1.3 `tests/native/`

These are mostly scenario coverage and host-interop references rather than
polished end-user tutorials.

They are still valuable because they show real surfaces working end to end.

---

## 2. Best Starting Examples

If you only open a few examples, open these first:

1. [`examples/static_cmake_consumer`](../examples/static_cmake_consumer/README.md)
2. [`examples/hybrid_arena`](../examples/hybrid_arena/README.md)
3. `tests/native/` host-interop cases for focused bridge questions

---

## 3. `examples/static_cmake_consumer`

This is the clearest "real host consumes Wio" reference right now.

What it demonstrates:

- `wio.makewio`
- CMake integration through `cmake/WioProject.cmake`
- static library output
- host use of `WioModuleGetApi()`
- SDK-based loading and invocation

Use it when you want:

- a practical embedding example
- a CMake-centered host integration reference
- a small and readable example before the bigger hybrid demo

Good fit for:

- engine embedding
- tools that statically link Wio-generated code
- learning the public SDK flow

---

## 4. `examples/hybrid_arena`

This is the bigger integration stress example.

What it demonstrates:

- mixed Wio + C++ layout
- multi-file Wio modules
- nested realms
- `component`, `object`, `interface`
- `@Native`
- `@Command`
- `@Event`
- shared-library lifecycle
- save/restore and reload behavior

Use it when you want:

- a "realer" module/host shape
- hot-reload-oriented reference behavior
- a broader language + interop sample together

It is intentionally heavier than the static consumer example.

Open it after you already understand the simpler project story.

---

## 5. `scripts/wio/`

This folder shows that parts of the tooling/workflow story can live in Wio
source rather than only in PowerShell or C++.

Representative files include:

- `scripts/wio/print_file.wio`
- `scripts/wio/line_count.wio`
- `scripts/wio/run_host_interop.wio`
- `scripts/wio/run_hybrid_arena_demo.wio`

Use them when you want to understand:

- how source-based Wio tooling is expected to feel
- how `wio file run ...` supports internal workflow tools
- where the project is heading as it reduces script-wrapper dependence

---

## 6. `tests/native/` As A Scenario Library

The `tests/native/` area is not only tests; it is also a strong interop
catalog.

It includes scenarios for:

- `@Native` imports
- `@Export`
- commands and events
- host-side module loading
- SDK object/component reflection
- enum/flagset metadata
- stale-wrapper and hot-reload behavior

If you are asking a narrow question such as:

- "how do I export an object field?"
- "how does a host bind a command?"
- "what does stale reload behavior look like?"

then a targeted `tests/native/*` scenario is often the best answer.

---

## 7. Example Picks By Goal

### 7.1 I Want The Fastest First Project

Use:

- `wio project new ...`
- then read [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)

### 7.2 I Want A Host Application To Call Wio

Start with:

- [`examples/static_cmake_consumer`](../examples/static_cmake_consumer/README.md)

Then read:

- [`WIO_SDK.md`](./WIO_SDK.md)
- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)

### 7.3 I Want Wio To Call Existing C++

Start with:

- `tests/native/` import-oriented scenarios
- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)

### 7.4 I Want Hot Reload

Start with:

- `examples/hybrid_arena`
- `tests/native/` reload scenarios
- [`WIO_SDK.md`](./WIO_SDK.md) hot reload section

### 7.5 I Want To See Wio-Written Tooling

Start with:

- `scripts/wio/`
- [`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md)

---

## 8. Example Release Reading

For `v1`, examples should be read like this:

- `examples/` are the primary showcase layer
- `scripts/wio/` demonstrates the source-based tooling direction
- `tests/native/` is the detailed interop scenario library

That means a polished release story should eventually make all three easy to
discover, but they serve different audiences.

---

## 9. Next Places To Read

- getting started:
  [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)
- CLI and project commands:
  [`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md)
- interop:
  [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- SDK:
  [`WIO_SDK.md`](./WIO_SDK.md)
- troubleshooting:
  [`WIO_TROUBLESHOOTING.md`](./WIO_TROUBLESHOOTING.md)
