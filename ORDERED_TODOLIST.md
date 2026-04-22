# Wio Ordered Todo

This file is the active execution roadmap.

- `TODOLIST.md` remains the exhaustive backlog and design ledger.
- This file stays intentionally short and ordered.
- Priority axis: `Foundation First`.

## Completed Foundations

These are already in the repo and should not be treated as net-new feature
work:

- First generic slice:
  generic functions, aliases, `object` / `component` / `interface`,
  explicit generic calls, and interop-oriented `@Instantiate(...)`
- First loop slice:
  `for`, `foreach`, `in`, range iteration, dictionary iteration, component
  binding, and parenthesized `for (...)`
- First namespace/import slice:
  `realm`, basic `use`, basic `use ... as ...`, and initial user/std module
  lookup
- First source-based std slice:
  `std::console`, `std::math`, `std::collections`, `std::strings`,
  `std::fs`, `std::path`, `std::algorithms`, `std::assert` / `std::testing`
- First interop/runtime/productization slice:
  `@Native`, `@CppHeader`, `@CppName`, `@Export`, `@Command`, `@Event`,
  lifecycle/state ABI, shared/static outputs, packaged toolchain,
  `wio.makewio`, templates, and public host SDK
- First real test wave:
  positive feature tests, invalid-program coverage, and interop/SDK tests

## Phase 1 - Todo Truth Audit

- Goal:
  make `TODOLIST.md` match the actual repo state
- Why now:
  the current backlog still describes several already-landed features as if
  they were untouched
- Exit criteria:
  summary/priority sections match reality, obvious stale items are reclassified,
  and shipped foundations are no longer presented as net-new work
- Dependency:
  none

## Phase 2 - Compiler Correctness and Diagnostics

- Goal:
  make Wio fail safely and explain failures clearly
- Why now:
  correctness and diagnostics are the biggest risk multipliers for every future
  feature
- Exit criteria:
  parser UB/crash audit completed, syntax failures consistently become Wio
  diagnostics, and parser/sema/backend/runtime-facing errors are clearly
  separated
- Dependency:
  Phase 1

## Phase 3 - Semantic Freeze for Shipped Features

- Goal:
  freeze the behavior of features that already exist
- Why now:
  the language already has real surface area, but too much of it is still
  "works in practice, not yet frozen"
- Exit criteria:
  `use`, `as`, `realm`, `for` / `foreach`, `ref`, `view`, `fit`, `match`,
  `when`, initialization/inference, object/component/interface rules, ctor
  rules, and core attributes all have deterministic semantics
- Dependency:
  Phase 2

## Phase 4 - Test and Traceability Strengthening

- Goal:
  make every stabilized behavior provable
- Why now:
  frozen semantics without traceable tests will drift again
- Exit criteria:
  each stabilized feature has positive and negative coverage, invalid-program
  tests start checking diagnostics more directly, and spec sections can point to
  concrete tests
- Dependency:
  Phase 3

## Phase 5 - Std, Runtime, and Host ABI Hardening

- Goal:
  turn the shipped std/runtime/SDK surface into a stable alpha contract
- Why now:
  the first wave exists already, so the value now is in clarifying boundaries
  and guarantees
- Exit criteria:
  std module boundaries are clearer, collection APIs are better classified,
  stable host ABI expectations are documented, and game-scripting-oriented host
  integration rules are more explicit
- Dependency:
  Phase 4

## Phase 6 - Backend Quality and Portability

- Goal:
  make the generated-C++ backend more deterministic, debuggable, and portable
- Why now:
  backend quality is currently one of the main bottlenecks on trust
- Exit criteria:
  better generated-C++ readability, stronger path/toolchain handling,
  cwd-independent backend behavior, and clearer non-Windows expectations
- Dependency:
  Phase 5

## Phase 7 - Project Model and Tooling Hardening

- Goal:
  turn the current project system from "works" into "comfortable and stable"
- Why now:
  packaging, templates, manifests, and helpers already exist, so polish matters
  more than inventing another workflow
- Exit criteria:
  `wio.makewio`, templates, CLI/help, build/run/check/debug flows, and examples
  feel predictable from a fresh checkout
- Dependency:
  Phase 6

## Phase 8 - Documentation and Education

- Goal:
  replace scattered draft/reference fragments with a status-marked language and
  tooling manual
- Why now:
  without clearer docs, already-shipped features will keep getting treated as
  ambiguous
- Exit criteria:
  versioned reference direction is clear, feature status markers exist, and the
  first tutorial set covers getting started, object model, references/`fit`,
  modules, and host interop
- Dependency:
  Phase 7

## Parking Lot / V2+

- Reflection for `enum` / `flagset`
- Concurrency and scheduling keywords
- Pipeline/data-flow operators
- `system` / `program`

These stay out of the active roadmap until the earlier phases are solid.
