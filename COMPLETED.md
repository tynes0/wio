# Wio Completed Work

This file gathers the completed work that used to be scattered across the old
`TODOLIST.md`.

Notes:

- These items are no longer brand-new work.
- Partially finished but real, landed work is still listed here.
- Anything that still needs hardening continues to be tracked as `[~]` in
  `TODOLIST.md`.

## Core Language Foundations

- [x] The first generic slice landed:
      generic free functions, generic aliases, generic `object` / `component` /
      `interface`, explicit generic calls, and `@Instantiate(...)`.
- [x] The first generic constraint / predicate slice landed:
      `@Apply(...)` plus predicate-based generic native/export bridges.
- [x] The first strong variadic generic / generic-pack slice landed:
      trailing-pack functions, pack forwarding, pack storage, `.size`,
      `.array`, `ToStaticArray<T>()`, and value/type-pack indexing.
- [x] The first two `std::meta` waves landed:
      `Head`, `First`, `Last`, `Types`, `Values`, `ContainsType`, `TypeCount`,
      plus related mutation/helper surface.
- [x] The `v1`-scoped `const generics` / `std::meta` wave 3 slice landed:
      top-level `const` integer declarations now work in pack index positions,
      simple compile-time integer expressions over those constants are accepted
      for pack indexing, and `std::meta` gained `Second`, `Penultimate`,
      `SecondValue`, `PenultimateValue`, and matching `Values<...>` helpers.
- [x] The first loop slice landed:
      `for`, `foreach`, `in`, range iteration, dictionary iteration,
      component-binding iteration, and parenthesized `for (...)`.
- [x] The first namespace/import slice landed:
      `realm`, basic `use`, `use ... as ...`, and multi-file module resolution.
- [x] Import ergonomics improved:
      alias-hide semantics, direct import, `use path::*`, and
      `use path::* as alias`.
- [x] The type-alias surface landed:
      `type Name = ExistingType;`, generic aliases, and alias constructor calls.
- [x] The first strong constructor-deduction slice landed:
      constructor-based type deduction for generic `object` and `component`.
- [x] The `else if` parser/codegen path was fixed.
- [x] `const` inside objects and outer-`const` visibility inside methods were fixed.

## Runtime Dynamic Types

- [x] `std::Result<T>` settled as the canonical model.
- [x] `Foo!()` unwrap sugar landed.
- [x] `Foo?()` propagation sugar landed.
- [x] Explicit generic `Foo<T>!()` / `Foo<T>?()` support landed.
- [x] `any` landed at both source and runtime levels.
- [x] The first strong `std::Box<T>` / `std::heap::box<T>` slice landed.
- [x] The `opaque` foreign-payload model landed.
- [x] A serious runtime/test line was established for `any / Box / opaque`.
- [x] `std::event` was added as the first real usage surface for
      `any/context/payload`.
- [x] The public `Result`, `std::Box<T>`, `any`, and `opaque` boundaries were
      frozen across the language/runtime/std documentation set.

## Mutable Data and Semantics

- [x] `ref values[index]` mutable indexed-reference semantics landed.
- [x] Nested mutable access and `ref` ergonomics improved significantly.
- [x] The first real in-place mutation coverage landed for arrays, dicts, and
      components.
- [x] The mutable reference/value-context behavior is now documented as part of
      the intended `v1` language contract.

## Enum / Flagset

- [x] `const` can now be used with enums and flagsets.
- [x] Native enum and native flagset support landed.
- [x] The first enum/flagset reflection slice landed.
- [x] The enum/flagset helper surface expanded with
      `Count`, `Name`, `Has`, `HasAny`, `With`, `Without`, `Toggle`, and `Clear`.
- [x] The normal Wio enum/flagset reflection surface was closed with
      `Value`, `Index`, `UnderlyingType`, and `Size`.

## Native Interop and ABI

- [x] `@Native`, `@CppHeader`, and `@CppName` settled into a real bridge.
- [x] The native `string -> const char*` bridge landed.
- [x] The first serious native `ref / view` passing semantics landed.
- [x] The declaration-level native POD component bridge landed.
- [x] Generic native POD component support landed.
- [x] Native POD field support expanded through static arrays.
- [x] Native `std::Box<T>` signature usage was enabled.
- [x] The top-level function export path landed with `@Export`.
- [x] The first strong public-member surface landed for exported
      `object` / `component`.
- [x] The module lifecycle/state ABI landed in its first strong form.
- [x] Shared/static/executable target separation and host interop were tested.
- [x] The intended `v1` native interop contract is now documented explicitly
      across the language freeze docs.

## Standard Library Surface

- [x] The first source-based std slice landed:
      `std::console`, `std::math`, `std::collections`, `std::strings`,
      `std::fs`, `std::path`, `std::algorithms`, `std::assert`, and
      `std::testing`.
- [x] `std::console` and `std::io` were rebuilt and moved onto the `Result`
      model.
- [x] `std::process` was added.
- [x] The `std::reflect` enum/flagset surface was strengthened.
- [x] `std::console` / `std::io` were aligned with the runtime header surface.

## SDK / Host Integration

- [x] The public host SDK surface gained its own identity.
- [x] The higher-level `WioObject` / `WioComponent` direction was opened.
- [x] SDK-side enum/flagset identity landed through `WioEnum` and
      `WioFlagset`, including exported field metadata and dynamic access.
- [x] Host-side exported types, interop, lifecycle, hot-reload, and
      stale-wrapper tests landed as the first serious wave.
- [x] SDK examples and packaged host-interop flows received real repo coverage.

## Tooling, Project Model, and Packaging

- [x] `wio.makewio` clearly became the primary project manifest direction.
- [x] `wio build` and `wio test` landed.
- [x] `wio file run/check/tokens/ast` landed.
- [x] `wio project new/describe/build/run` landed.
- [x] `wio bind import/new` landed.
- [x] `wio env print/setup` landed.
- [x] `wio package` landed.
- [x] The CLI command surface was frozen as the primary tooling contract.
- [x] The source-based tooling split was formalized:
      core orchestration stays in the CLI while `scripts/wio/*.wio` hosts
      source-based workflow helpers.
- [x] Packages now include installer wrappers (`Install-Wio.ps1`,
      `install-wio.sh`) and source-based tool scripts.
- [x] Packaging and install UX now share one documented model through
      `wio package`, `wio env print/setup`, and packaged `QUICKSTART.md`.
- [x] PowerShell stopped being the primary path; larger scripts became
      compatibility wrappers.
- [x] The first `.wio` tooling folder and source tool examples landed.
- [x] The `wio` CLI was split into separate modules and `main.cpp` became a
      thin entrypoint.
- [x] `wio file run` and source-based tool workflows now keep backend outputs
      out of source directories, use project/repo hidden caches when available,
      and fall back to user-cache locations for packaged/non-project use.
- [x] Ordinary generated `.wio.cpp` files are now treated as backend
      intermediates near the output root, while `--emit-cpp` remains the
      explicit opt-in path for keeping source-adjacent generated C++.

## Binding Automation

- [x] Manifest-based binding scaffolding landed.
- [x] A header -> Wio binding bootstrap importer landed.
- [x] Binding smoke tests were moved onto the CLI path.

## Testing and Productization

- [x] The first serious positive feature-test waves landed.
- [x] The first serious invalid-program corpus landed.
- [x] The first clear backend compile/link failure tests landed.
- [x] Native/shared/static/SDK/package/project-model smoke tests landed.
- [x] The release-facing tooling smoke matrix now covers CLI help, file flows,
      project create/describe/build/run, binding import/new, package staging,
      packaged file-run cache behavior, and source-based Wio tool dry-runs.
- [x] Packaged toolchains were validated through real project
      create/build/run flows.
- [x] The language/reference freeze snapshot now treats the chosen generic,
      Result, native interop, mutable reference, and runtime reference surface
      as the intended `v1` contract.
- [x] The `v1` release contract now has explicit companion notes for
      compatibility policy and performance/memory expectations.

## Recent Partial Foundation

- [x] The first member-operator overloading slice landed.
- [x] Operator overloading was fully closed:
      member/free unary-binary-assignment operators, `fit`, `[]`, `()`,
      generic overloads, and the explicit `deref` ergonomics slice now work
      together.
- [x] Generic validation hardening landed:
      generic bodies keep symbolic validation for codegen, gain concrete
      call-site validation, and now understand numeric/integer constraint
      predicates like `std::traits::IsNumeric<T>` during semantic checks.
      Unary/binary member operator resolution and codegen are working.
      Final hardening continues to be tracked in `TODOLIST.md`.
