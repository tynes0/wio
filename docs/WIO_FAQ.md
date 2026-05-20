# Wio FAQ

This document collects recurring design questions and practical "why did Wio
choose this?" answers.

It is not the formal spec. It is the design-language companion to the formal
docs.

---

## 1. Why `wio.makewio` Instead Of JSON?

Because the primary project file is meant to be read and edited by humans.

`wio.makewio` is intended to be:

- easier to scan
- easier to hand-edit
- less noisy for small projects
- friendlier as the default project manifest

`wio.project.json` still exists as legacy/compatibility input, but it is not
the main user-facing direction anymore.

---

## 2. Why Is The Main Entry Path `wio` Instead Of PowerShell Scripts?

Because Wio needs a real toolchain identity.

The intended `v1` model is:

- `wio` is the product
- PowerShell wrappers are compatibility launchers

That keeps the experience more consistent across:

- source-tree usage
- packaged usage
- future cross-platform usage

---

## 3. Why Does Wio Clean Up Generated C++?

Because generated backend code is an implementation artifact, not the main
source of truth.

Most users do not want:

- generated `.wio.cpp` files
- backend executables
- stray artifacts

to accumulate beside their source files.

If you want the generated C++, opt in with `--emit-cpp`.

---

## 4. Why Does `wio file run ...` Use Hidden Cache Output?

Because Wio is compiled, but single-file workflows should still feel clean and
lightweight.

The intended behavior is:

- compile and run like a normal language
- avoid cluttering source folders with backend artifacts

That is why hidden project/repo caches or user-cache fallbacks are used.

---

## 5. Why `std::Result<T>` Instead Of Exception-Only Source Semantics?

Because Wio currently prefers explicit fallible control flow at the source
level.

The current model is:

- canonical fallible std APIs return `std::Result<T>`
- `Foo!()` is explicit unwrap/fail
- `Foo?()` is explicit propagation

This keeps error flow visible at the call site instead of hiding it behind
general implicit throws.

---

## 6. Why Not `Result<T, E>`?

Because the current `v1` direction prefers one shared fallible model first.

That makes:

- std APIs
- diagnostics
- unwrap/propagate sugar
- runtime bridging

much easier to keep coherent.

The current design prefers:

- `std::Result<T>`
- shared `ResultError`

over opening a second major type-axis before `v1`.

---

## 7. Why `deref` Instead Of C++-Style `operator->`?

Because Wio is trying to stay explicit about reference layers.

Current model:

- `ref`
- `view`
- `deref`
- value-context auto-read

This keeps the reference story readable without importing all of C++'s pointer
surface complexity into `v1`.

---

## 8. Why Is `std::Vector2` At The Root Instead Of `std::vector::Vector2`?

Because some std types are meant to feel like first-class library primitives,
not tucked-away submodule details.

The current direction is:

- root `std::Vector2`
- root `std::Vector3`
- root `std::Vector4`
- helper realms for utility functions

This keeps high-frequency math types ergonomic.

---

## 9. Why Does `std::meta` Only Cover A Narrow Slice In `v1`?

Because broad compile-time programming is powerful, but expensive to freeze too
early.

The current `v1` slice focuses on:

- pack counting
- pack indexing
- top-level const-based pack index expressions
- a small but real `std::meta` helper surface

That gives useful compile-time leverage without pretending the whole future
const-generic system is already complete.

---

## 10. Why Are `std::heap` And `std::event` Still More Cautious?

Because they sit closer to runtime-model questions than ordinary pure library
helpers.

They are valuable, but they are also the kinds of surfaces where fuzzy
ownership or identity semantics can create long-term confusion if frozen too
loosely.

---

## 11. Why Does The SDK Keep Some Wrappers Generation-Bound?

Because pretending everything is magically reload-safe would create dangerous
half-truths.

The SDK chooses:

- reload-safe top-level callable refresh where that model is clean
- explicit stale-wrapper errors for generation-bound object/component wrappers

That is a safer and more honest hot-reload model for `v1`.

---

## 12. Should Wio Have A Docs Website Later?

Probably yes.

But for `v1`, the most valuable thing is not a framework choice. It is a
documentation structure that is already website-ready.

That is the current direction:

- `README.md` for fast orientation
- `docs/README.md` as the docs map
- focused companion documents for major areas

If we later wire this into a Vercel-hosted site, the content structure is
already in the right shape.
