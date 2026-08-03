# Wio Completed Work

This file gathers the completed work that used to be scattered across the old
`TODOLIST.md`.

Notes:

- These items are no longer brand-new work.
- Partially finished but real, landed work is still listed here.
- Anything that still needs hardening continues to be tracked as `[~]` in
  `TODOLIST.md`.

## P0 release-blocking correctness sprint

- [x] Cascading diagnostics are stopped at their source. Imported modules with
      parser errors are no longer merged as partial ASTs, poisoned types flow
      through expressions/calls, and derivative overload/operator diagnostics
      are suppressed while independent root errors remain visible.
- [x] `ref` / `view` lifetime semantics now track static, caller, local, and
      temporary borrow origins. The checked matrix covers temporary component
      calls, nested fields, array elements, returned owning arrays, span range
      tokens, object handles, `self`, `deref self`, mutable receivers, and
      native boundaries. The contract is documented in
      `docs/REFERENCE_LIFETIMES.md`.
- [x] Deterministic lexer/parser/semantic/generated-C++ pipeline fuzzing and an
      optional Clang libFuzzer+ASan+UBSan frontend target landed. Corpus and
      mutations cover nested interpolation, malformed generics, deep types,
      import cycles, invalid UTF-8, arbitrary bytes, diagnostic/output budgets,
      timeouts, dry-run/emission differentials, and backend syntax checking.
      The sprint qualification run passed 259 candidates.
- [x] `FunctionType::toCppString()` now emits nested `std::function` types and
      has a direct type-layer regression test.
- [x] Arithmetic result typing no longer depends on the left operand. Mixed
      signed/unsigned and float/integer operations use a deterministic common
      type, float modulo/bit/shift misuse is rejected semantically, and
      integer-looking float literals generate valid C++.
- [x] `null` is restricted to runtime-nullable targets: object/interface
      handles, functions, `ref`/`view`, `any`, and `opaque`. Primitive,
      component, array, dictionary, and context-free inferred nulls are
      rejected before code generation; reference/null comparison codegen is
      covered.
- [x] `use` parsing now has explicit path-segment recovery, stable diagnostics,
      and conventional bitwise precedence. `use path::* as alias` was also
      repaired in module merge so direct and namespaced access both work.
- [x] Compiler filesystem helpers now follow a documented non-throwing policy
      for ordinary OS failures, close failed seek paths, detect short-read
      errors safely, iterate with `error_code`, and make directory creation
      idempotent. Allocation failure remains explicitly exceptional.

## P1-A language and standard-library correctness sprint

- [x] Integer arithmetic now has explicit semantics across all ten integer
      types. Ordinary `+`, `-`, `*`, unary negation, division, remainder, and
      compound assignment wrap deterministically; division by zero is a Wio
      runtime error. `std::numeric` provides checked and saturating add,
      subtract, and multiply for every integer type.
- [x] Implicit numeric conversion now permits safe widening only. Lossy or
      potentially lossy narrowing requires explicit `fit`, and diagnostics
      identify the source and destination types.
- [x] Enums expose `Value()` and `IsValid()`. `std::reflect` provides generic
      `IsValid`, `TryFromValue`, and strict `FromValue` overloads for every
      integer representation; unknown native values remain representable but
      are rejected by checked conversion and reflection validity checks.
- [x] `Result<T>` gained `Map`, `MapError`, `AndThen`, `OrElse`, `Inspect`,
      `InspectError`, `Flatten`, `ToOption`, `Collect`, and `Sequence`, with
      focused propagation and collection coverage.
- [x] `Option<T>` adoption is complete across intrinsic dictionary lookup and
      std containers. `Get`, `First`, and `Last` represent expected absence;
      strict `At` retains bounds/key failure behavior; iteration helpers,
      `Zip`, Result transpose, cloning, capacity, equality, and removal
      contracts are covered by one cross-container regression suite.
- [x] Canonical `std::fs` operations return structured `Result` values carrying
      the filesystem domain, portable code, native OS code, and actionable
      message. Reads/writes, enumeration, metadata, permissions, copy/move/
      remove, canonicalization, and atomic replacement share the model; legacy
      `Try*`/`*Raw` helpers are explicit compatibility escapes.
- [x] Every public `std::path` and `std::fs` operation is exercised from the
      repository and from a clean installed package. Windows and Linux
      qualification covers contextual `Extension` parsing and rejects source
      checkout leakage.

## Standard platform introspection

- Added typed operating-system and CPU-architecture enums plus public
  `std::platform` queries for pointer width, endian, hardware thread count,
  path-list separator, and native newline.
- Self-hosted environment commands now use platform identity directly instead
  of inferring Windows from executable suffixes.

## Self-hosted global CLI dispatch

- Empty invocation, global help, nested `wio help ...` rewriting, version
  routing, top-level command classification, and typo suggestions now run in
  Wio.
- Raw source/compiler invocations still bypass the companion, while unknown
  tooling-shaped commands enter Wio for consistent diagnostics.
- Native version lookup uses the explicit stage-0 bridge and cannot recurse
  back into the self-hosted companion.
- The generic `--native-cli`/environment-variable bypass was subsequently
  removed. Binding and release packaging now use named private compiler
  services, and the obsolete C++ tooling/env/file/perf CLI sources were
  deleted.

## Self-hosted environment CLI

- Wio owns `env` group/subcommand parsing, help/version behavior, typo
  suggestions, shell validation, root/bin discovery, and command rendering.
- `env print` and non-interactive `setup/remove` previews execute entirely in
  Wio.
- The follow-up completed interactive setup/removal, persistent user
  environment and PATH management, detailed status, duplicate-key diagnostics,
  and backend smoke entirely in Wio.
- Public `std::environment` now exposes user-scoped get/set/remove, user PATH
  membership/add/remove, and duplicate process-key inspection. Windows uses
  the user Environment registry; POSIX uses a managed `.profile` block.

## Self-hosted binding CLI

- Wio owns the `bind new/import` group and subcommand contract, including
  required inputs, optional output/header settings, flagset preference,
  help/version handling, and typo suggestions.
- JSON-manifest generation and namespace-aware C/C++ header importing now run
  in Wio using `std::json`, `std::regex`, `std::fs`, and `std::path`.
- The private binding service, its C++/Argonaut parser and generator, and the
  last shared C++ CLI parsing helper were deleted.
- The final standalone `process_cli.cpp/.h` launcher was removed. Stage-0 now
  launches the Wio companion through the shared host build of the public
  runtime process primitive, so process behavior has a single implementation.

## Self-hosted release-package CLI

- Wio owns every top-level `package` option, default, help/version path, and
  incompatible visual-installer option validation.
- Distribution staging, portable backend discovery/copy, metadata and
  quickstart generation, archive production, and installer orchestration now
  execute in Wio. The obsolete C++ package service was deleted.
- Public `std::process::FindExecutable` supports reusable PATH-based executable
  discovery for packaging and other tooling.

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
- [x] Explicitly typed `let` / `mut` declarations may omit assignment and now
      receive deterministic value initialization across scalars, strings,
      containers, and components. Untyped declarations and `const` still
      require initializers.
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
- [x] Installed-package qualification gained a real installer-based
      Windows/Linux release gate: it uses a clean toolchain root, validates all
      40 public std modules independently and together, exercises a complete
      external project build/run/test/package lifecycle, validates native
      interop, and rejects source-checkout resolution leakage. The first-class
      `wio project test` command supports discovery, manifest overrides,
      filtering, listing, incremental builds, and `--no-build`; `wio project
      package` emits `bin`/`lib`/assets/additional files plus machine-readable
      package metadata.
- [x] The release-facing example set now includes plain app, native app,
      hybrid module, binding import, packaged quickstart, static consumer,
      and the heavier hybrid arena companion example.
- [x] A Windows/Linux release validation workflow now covers the critical
      packaged/project/binding/process smoke matrix in CI.
- [x] The language/reference freeze snapshot now treats the chosen generic,
      Result, native interop, mutable reference, and runtime reference surface
      as the intended `v1` contract.
- [x] The `v1` release contract now has explicit companion notes for
      compatibility policy and performance/memory expectations.
- [x] The docs set was expanded into a release-quality, website-ready
      structure: getting started, CLI reference, interop guide, examples
      guide, troubleshooting, FAQ, refreshed SDK/std/project-system
      navigation, and a clearer root/docs index story now all point at one
      coherent `v1` product narrative.

## Recent Partial Foundation

- [x] The self-hosted CLI bootstrap landed: the native stage-0 compiler builds
      and packages a Wio + Argonaut-Wio companion, provides a recursion-safe
      internal fallback bridge, and routes `project test/package` through Wio
      argument validation with lifecycle and installed-package probes.
- [x] `wio project new` became the first command with both parsing and business
      logic fully implemented in Wio; its plain, native, module, and hybrid
      templates remain compatible with the existing project build/run tests.
- [x] The complete project family moved behind the self-hosted boundary:
      makewio discovery and normalization, `describe`, Wio/native-host
      `build`, `run`, regex-driven `test`, directory `package`, and the
      `wio run` shorthand execute in Wio without calling the legacy native
      project handlers.
- [x] The second self-hosted CLI wave landed: `file run/check/tokens/ast`,
      repository `build/test`, `dev build/test`, and the five-scenario
      `perf smoke` implementation now execute in Wio with parity, packaged
      cache, and installed-layout tests.
- [x] `std::environment` now exposes portable process environment,
      home/cache/temp directory helpers, and `std::statistics` provides
      reusable sum/mean/median/variance/summary operations.
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

## Version 1 Contract and Product Closure Milestones

The following completed checklist used to remain in `TODOLIST.md`. It is kept
here as historical evidence rather than active work:

- [x] Operator overloading was completed across member/free,
      unary/binary/assignment, `fit`, `[]`, `()`, generic overload, and
      explicit `deref` paths.
- [x] The intended language surface was frozen across the freeze snapshot,
      language draft, std docs, runtime type model, SDK docs, examples, and
      compatibility policy.
- [x] `wio.makewio` became the official primary project format, with
      `wio.project.json` retained only for compatibility.
- [x] The primary CLI behavior was frozen around `build`, `test`, `file`,
      `project`, `bind`, `env`, and `package`.
- [x] PowerShell helpers were removed from the primary workflow and retained
      only as compatibility wrappers.
- [x] The split between native CLI orchestration and Wio-written tools under
      `scripts/wio` was formalized.
- [x] Single-file/tool output, generated-C++ cleanup, package cache, and
      non-writable-install policies were settled.
- [x] Packaging/install UX was unified around `wio package`, installer
      wrappers, `env print/setup`, `QUICKSTART.md`, and executable-relative
      discovery.
- [x] Windows/Linux release validation was established for the critical
      project, package, binding, process, and packaged-file flows.
- [x] The stable/experimental std boundary was documented.
- [x] `std::Result<T>`, `Foo!()`, and `Foo?()` were sealed as the official
      recoverable error-flow model.
- [x] The `@Native`, `@CppHeader`, `@CppName`, POD/native enum/flagset,
      export, and ABI-safe interop contract was frozen.
- [x] `any`, `Box`, and `opaque` boundaries were made official.
- [x] Mutable indexed/container access and `ref` value-context ergonomics were
      finalized for the chosen contract.
- [x] Enum/flagset support was closed across constants, native interop,
      reflection, helpers, normal Wio APIs, and SDK identity.
- [x] Release-facing tooling tests, documentation, examples, compatibility
      policy, and performance/memory notes were completed.
- [x] The scoped const-generic/std-meta wave 3 slice was completed.

## Standard Library Expansion Through v0.4

- [x] `std::hash` landed with FNV-1a as the default and SHA-256 digest/hex
      support.
- [x] `std::random` landed with MT19937 as the default plus xoroshiro128+,
      LXM, and Wichmann-Hill generators.
- [x] `byte`/`bit`, `ByteBuffer`, generation-checked `BytePool`, and typed
      `Pool<T>` landed.
- [x] Queue, ordered/unordered sets, heterogeneous tuple, regex, time, span,
      range, and adaptive sorting modules landed.
- [x] Type traits expanded across built-in categories and user-defined nominal
      predicates usable by `@Apply`.
- [x] Reflection expanded beyond enum/flagset to component, object, interface,
      field, method, access, base, size, and alignment metadata.
- [x] Checked numeric/string conversion and parsing landed through
      `std::convert`, including base-aware integer formatting/parsing and
      generic `ToString`.
- [x] `std::chars`, expanded string helpers, and the larger generic algorithms
      wave landed.
- [x] `std::Option<T>` landed with presence queries, `Value`, `ValueOr`,
      `Map`, `AndThen`, `Filter`, `OrElse`, and `ToResult`.
- [x] Option-returning lookup helpers landed for array algorithms,
      collections, strings, spans, and iterator search.
- [x] `std::iterator`, `std::range`, and `std::encoding` landed.
- [x] `std::serialization` and the recursive `std::json::Value` system landed
      with parsing, compact/pretty writing, nested arrays/objects, typed
      access, source-positioned errors, and round-trip coverage.
- [x] Checked/saturating `std::numeric`, in-memory `std::stream`, UUID v4,
      packed component/extension SemVer, and structured `std::log` landed.

## Component Extensions and Interpolation

- [x] Stack-preserving component extensions landed as externally lowered
      methods with member-call ergonomics.
- [x] Mutable and immutable extension receivers, access isolation, semantic
      resolution, code generation, documentation, and positive/negative tests
      landed.
- [x] Component extensions were applied to `Span` and vector components.
- [x] Immutable component extension calls on temporary values were fixed in
      the C++ backend by safely materializing the temporary through the complete
      call expression.
- [x] Interpolated-string lexing moved from global flags to balanced nested
      frames.
- [x] Interpolation expressions now support ordinary string literals, nested
      function calls/parentheses/braces, dictionary calls with string
      arguments, and nested interpolated strings.
- [x] Contextual keyword identifiers were hardened across declarations,
      component/object fields, member and realm qualification, imported APIs,
      and native bindings. `std::path::Extension` no longer makes packaged
      `std::path` / `std::fs` imports fail, and a focused end-to-end regression
      test covers the complete path.
- [x] The right-associative `condition ? whenTrue : whenFalse` conditional
      expression landed with lazy branch evaluation, boolean-condition and
      branch-compatibility diagnostics, direct C++ lowering, precedence
      documentation, and focused positive/negative coverage.

## CLI Expansion and Releases

- [x] Bare project commands gained manifest discovery from the current
      directory and its ancestors.
- [x] `wio run` became a project-run shorthand.
- [x] Contextual `wio help`, command-family version/help, application
      arguments after `--`, repeated arguments, working-directory control,
      command printing, and no-build/no-manifest-args controls landed.
- [x] Direct cross-platform process launching replaced shell command strings
      for project, package, and performance subprocesses.
- [x] Argument forwarding now preserves spaces, flag-like values, shell
      characters, literal separators, child exit codes, and Windows PATH
      behavior.
- [x] Wio `v0.2.0`, `v0.3.0`, and `v0.4.0` release branches/packages were
      produced; `v0.4.0` also received a Git tag and GitHub Release with
      installer and portable assets.

## Real-World External Validation

- [x] A large multi-module Wio colony simulation was built outside the repo
      with deterministic generation, component extensions, missions, economy,
      interactive CLI, native persistence, and JSON output.
- [x] A non-game native desktop productivity application was built with Wio
      and raylib, including a responsive task board, quick notes, search,
      focus timer, mouse/keyboard interaction, and JSON persistence.
- [x] The raylib desktop application compiled through the installed Wio
      `v0.4.0` toolchain and completed a real GLFW/OpenGL render smoke test on
      Windows.
- [x] That external validation exposed the installed-package
      `std::path::Extension` contextual-keyword regression; its fix and the
      release gate preventing recurrence remain active P0 items in
      `TODOLIST.md`.
