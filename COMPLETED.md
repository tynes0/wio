# Wio Completed Work

This file gathers the completed work that used to be scattered across the old
`TODOLIST.md`.

Notes:

- These items are no longer brand-new work.
- Partially finished but real, landed work is still listed here.
- Anything that still needs hardening continues to be tracked as `[~]` in
  `TODOLIST.md`.

## v0.15 attribute foundation sprint

- [x] Declaration-leading `[Attribute]` syntax supports stacked and grouped
      lists while preserving legacy input and canonical `using` activation.
- [x] Bracket attributes retain target identity on ordinary declarations,
      members, parameters, enum/flagset cases, applications, systems,
      lifecycle handlers, and application/system fields.
- [x] User attributes compose other attributes with compile-time parameter
      substitution, default materialization, recursive expansion, and cycle
      diagnostics.
- [x] Effective-set contracts cover targets, requires, requires-any,
      conflicts, only-with allow lists, exclusive groups, cardinality, and
      before/after ordering cycles.
- [x] `std::attribute` publishes processor phases, meta-attribute policies,
      and declarations for the current compiler attributes.
- [x] Processor declarations are checked against exactly one validation,
      derive, pre, post, finally, or around capability interface.
- [x] Built-ins and user attributes share stable contract identities for
      target, requirement, and conflict matching.
- [x] Bounded validators execute at compile time with constant diagnostics;
      synchronous pre, post, and exactly-once finally hooks execute on Wio
      function bodies with explicit diagnostics for still-closed edge cases.
- [x] Behavioral processors honor topological `Before`/`After` dependencies;
      source order is deterministic for unrelated processors and exit hooks
      unwind in reverse order.
- [x] Unit synchronous around hooks own a guarded zero-or-one `Proceed`;
      duplicate calls and use after the around invocation are rejected without
      exposing mutable AST or token rewriting.
- [x] Typed synchronous around hooks map or replace exact return values;
      post processors observe typed results, finally processors observe normal
      versus exceptional completion, and pre/post/finally hooks execute inside
      async coroutine frames. Async around is explicitly rejected.
- [x] Object-method pre processors may receive the current receiver as `any`;
      boolean pre guards can skip unit-returning methods, while receiver use on
      free functions and result-bearing guards fail during analysis.
- [x] Method reflection exposes behavioral processors in effective execution
      order, including attribute identity, processor type, phase, hook, and
      invocation mode; `Describe<T>()` includes the same pipeline per method.
- [x] Bounded checked derives expose marked processor methods as real members
      on concrete and generic component/object targets. Every concrete generic
      instantiation inherits its generic primary declaration surface.
      Calls receive the target through a hidden `any` receiver or an immutable
      typed contract view from `DeriveProcessor<TTarget>` and use an isolated
      default-constructed processor. Incompatible targets, unsafe method
      shapes, conflicts, and ambiguous derives fail during analysis instead of
      becoming silent metadata.
- [x] Runtime type reflection exposes structured attribute descriptors with
      stable FNV-1a identity, retention, origin, normalized arguments, and
      default provenance while retaining the legacy string view.
- [x] The focused processor foundation matrix passes 29/29 new tests.

## v0.13 language coherence and SDK parity sprint

- [x] The normative Wio 0.13 coherence specification freezes first-class
      Unicode `text`, `const string`/`const text`, textual const generics,
      fixed-array extent inference, compact typed attributes, named attribute
      arguments, generic extensions, deterministic partial specialization,
      and guarded/exhaustive matching behavior.
- [x] Canonical `with`/`using` native metadata is emitted by project and
      binding generators, while legacy `@Attribute` source remains accepted as
      compatibility input.
- [x] Source-tree validation pins its own runtime, std, and SDK roots so an
      older installed Wio cannot silently satisfy or break repository tests.
- [x] The public C++ SDK is version-aligned at 0.13.0 and ships a
      machine-readable feature inventory plus host-semantic values for text,
      Option, Result/UnitResult, tuples, queues, sets, spans, buffers, pools,
      Box, any, and nullable values.
- [x] Module ABI descriptor v7 publishes product version, descriptor size,
      capabilities, canonical stable type IDs, and ordered concrete generic
      arguments. Generated shared modules round-trip Unicode `text` through
      typed and dynamic SDK field access.
- [x] The SDK parity matrix distinguishes real bridges, metadata-only values,
      host values, opaque identities, and deliberately deferred surfaces. The
      packaged distribution includes the SDK headers, release manifest, and
      parity documentation.
- [x] The focused SDK/interop regression matrix passed 16/16, package smoke
      passed, and an independently installed SDK header probe compiled and ran
      successfully with the 0.13 package surface.

## v0.12 async, structured concurrency, and platform-I/O sprint

- [x] Wio 0.12 separates continuation, blocking,
      and I/O capacity; checks executor-crossing captures; makes cancellation
      wake suspension boundaries; and provides ordered explicit shutdown.
- [x] `Task<T>` member ergonomics, typed non-blocking polling, recoverable
      deadlines, heterogeneous owning scopes, language `async scope`/`spawn`,
      explicit detach, and homogeneous selection shipped in Wio 0.12.
- [x] `spawn worker` and `spawn blocking` schedule synchronous expressions on
      the appropriate executor while retaining lexical scope ownership; unsafe
      borrowed captures fail during semantic analysis.
- [x] Applications bind and deterministically drain the owner executor;
      `await main` directly transfers the suspended caller, and headless hosts
      can bind/drain the same queue.
- [x] The Wio 0.12 platform-I/O slice adds a dedicated bounded executor and
      ownership-safe, Result-preserving asynchronous filesystem plus process
      run/capture operations, a cancellable portable first-change watcher, and
      leased DNS/connect plus TCP/UDP data I/O and ownership-safe async accept
      with close-race safety.
- [x] `std::process::Spawn` adds owned child lifetime, separate stdin/stdout/
      stderr pipes, sync/async streaming, running state, wait, termination,
      deterministic close/reap, pre-scheduling operation leases, and live-state
      diagnostics.
- [x] Windows entry-point arguments are normalized from the native UTF-16
      command line to UTF-8, including spaced and non-ASCII spawned arguments.
- [x] `std::async::AsyncChannel<T>` provides bounded or unbounded buffering,
      non-blocking `TrySend`/`TryReceive`, cancellable async backpressure,
      close wake-up, and buffered drain-before-completion semantics.
- [x] Wio enum members remain isolated to their declaring enum through the
      C++ backend, allowing separate enums to reuse natural names such as
      `pending`, `empty`, and `closed` without backend collisions.
- [x] Re-importing an already merged module under another alias now restores
      cached export/realm metadata, making nested and combined std imports
      deterministic regardless of module discovery order.

- [x] `async fn`, async object/interface methods, generic async declarations,
      async `Entry`, `await`, and the `coroutine<T>` type are implemented across
      lexer, parser, semantic analysis, and C++20 code generation.
- [x] Suspension-unsafe borrowed parameters/returns, component and extension
      receivers, lifecycle methods, operators, and native/export ABI surfaces
      fail during semantic analysis.
- [x] The shared task runtime provides eager execution, multiple awaiters,
      exception propagation, strong object-receiver lifetime, a worker pool,
      priority-queue timers, cooperative cancellation, and non-blocking timer
      drain during detached-task shutdown.
- [x] `std::async` provides Sleep/Yield, BlockOn/Run, task state and timed wait,
      cancellation/deadlines, All/Any/Race/Timeout, cancellation sources, and
      generic/void structured task groups.
- [x] Async generic object-method mangling now recursively recognizes generic
      parameters nested inside `coroutine<T>`.
- [x] The first-sprint focused matrix covers execution, async Entry, generics,
      object lifetime, structured groups, timeout/cancellation failure paths,
      and semantic rejection cases.
- [x] The second async freeze sprint repaired final-frame ownership, removed
      detached timer shutdown hangs, added configurable workers, recoverable
      timeout, cancellable sleep, owner-thread dispatch, async interface and
      multiple-await qualification, and high-volume native stress coverage.
- [x] Lambda capture is frozen as value-by-default: primitive/component values
      snapshot, object handles retain shared identity, and explicit `ref/view`
      captures remain borrows.
- [x] Windows/Ubuntu freeze gates, packaged documentation checks, native
      ASan/UBSan stress, and libFuzzer/ASan/UBSan corpus validation are part of
      release CI.
- [x] Escaping object-method lambdas retain `self`; scheduler destruction
      drains frame cleanup; backend output replacement tolerates transient
      Windows locks; import-alias and missing-array-extent diagnostics are
      deterministic.
- [x] The final Windows freeze qualification passed the 157-test traceability
      pack, the 78-test focused freeze matrix, package/file-cache/installed-
      package qualification, five consecutive executable replacement probes,
      and the complete 585-test repository matrix.
- [x] The post-0.11 async/multithreading direction is recorded as a dedicated
      simplicity-first evolution plan: a compact task model, structured work,
      explicit blocking and affinity, compiler-checked cross-thread safety,
      real application qualification, and staged I/O/stream expansion.

## P1 language semantics and standard-library foundation sprint

- [x] `std::UnitResult`, `OkUnit`, and `ErrUnit` are the canonical unit-success
      API and public console/filesystem/I/O/tooling surfaces use it.
- [x] Typed postfix `with`, scoped `using`, user attribute declarations,
      target/argument/repetition validation, conflict groups, and runtime
      type/field reflection landed with legacy spelling compatibility.
- [x] Pipeline calls and Option/Result/array destructuring have defined
      evaluation, binding, guard, reachability, and exhaustiveness behavior.
- [x] `|>` and `<|` were evaluated and reserved as ordinary-call pipelines;
      they preserve call inference/diagnostics and single evaluation.
- [x] The sequential stack-resident application/system lifecycle landed with
      deterministic start/update/reverse-close and orderly `self.Exit`.
- [x] The 0.11 semantic stabilization pass froze scoped-attribute boundaries,
      `with` clause diagnostics, pipeline precedence and single evaluation,
      match binding ownership/reachability, and application-root/lifecycle
      invariants. Its 22 newly added runtime and expected-diagnostic tests pass.
- [x] Unicode UTF-8/codepoint/grapheme foundations, text/byte builders,
      hardened JSON, versioned binary frames, CSV/INI, MIME, geometry/color,
      localization, bigint, and compression utilities landed.
- [x] Allocation-conscious `StringBuilder`, endian/varint `ByteWriter` and
      rollback-safe `ByteReader` provide the canonical builder surface.
- [x] Threads, recursive mutexes, condition variables, atomics, channels,
      blocking channels, TaskGroup, Promise/Future, and cancellation
      foundations now share one runtime surface.
- [x] DNS, URI, TCP, and UDP loopback foundations, expanded OS/process
      facilities, structured log sinks, regex safety limits, time formatting,
      secure random, full SemVer precedence, and UUID validation landed.
- [x] The normative 0.11 language delta and standard-library contract are
      published in `docs/spec/` with explicit boundaries for unfinished work.
- [x] Sprint qualification passed all 36 newly added runtime and expected-
      diagnostic tests, including generated C++ and native loopback/threading
      execution; unrelated legacy tests were intentionally not run.

## P1-B nullability, lifetime, and failure safety sprint

- [x] Explicit nullable types now use `T?`; object/interface, opaque,
      function, `ref`, and `view` values are non-null by default. Grouped type
      syntax distinguishes `ref T?` from `(ref T)?`; postfix ordering supports
      nullable elements in dynamic arrays and generic containers.
- [x] Null-flow analysis narrows direct variables through if/else comparisons,
      short-circuit boolean expressions, while conditions, and early-return
      guards. Assignment invalidates the proof, and nullable member use without
      narrowing receives a targeted diagnostic.
- [x] The initialization and lifecycle matrix is executable: non-null local and
      global handles require initializers, components copy/destruct per value,
      objects share identity and destruct at the last strong handle, and
      `OnDestruct` is restricted to a parameterless void hook.
- [x] Panic is the unrecoverable path, `Result<T>` remains the recoverable path,
      panic unwinding runs Wio cleanup, and generated native wrappers translate
      standard/unknown C++ exceptions into stable Wio runtime failures.
- [x] `std::resource` ships `Owned<T>`, `Borrowed<T>`, idempotent `Dispose`,
      `Release`, automatic final-owner close, use-after-dispose protection, and
      live-resource diagnostics, including real non-null and nullable native
      opaque-handle tests.
- [x] SDK module descriptors preserve nullable types through the appended
      `WIO_MODULE_TYPE_DESC_NULLABLE` kind and
      `TypeDescriptorView::is_nullable()`.
- [x] The first versioned normative document is published at
      `docs/spec/WIO_LANGUAGE_SPEC_0_8.md`; completing the full lexical/syntax/
      resolution specification remains active work.

## P1-D const generics and native component sprint

- [x] Functions, aliases, interfaces, components, and objects accept ordinary
      integer const parameters such as `<T, const N: usize = 4>`, including
      trailing defaults and use as read-only values in declaration bodies.
- [x] Const arguments accept non-negative integer literals, earlier const
      parameters, and top-level compile-time integer const declarations.
      Type/value slot mismatches and non-integer const declarations are
      diagnosed before backend generation.
- [x] Static arrays support symbolic extents such as `[T; N]`; substitution,
      constructor matching, literal-size checks, deduction, C++ emission, and
      generic aliases preserve the concrete extent.
- [x] Const values participate in generic identity, invariance, exact/partial
      specialization matching, specificity, ambiguity, and C++ template
      argument generation.
- [x] Declaration-level native components now have a single POD-alias contract
      for both type-only and type/value C++ templates. Native generic component
      specializations inherit the primary ABI mapping and no longer generate
      illegal C++ alias-template specializations.
- [x] Native component extensions bind C++ free functions directly. `view`
      receivers dispatch to `const T&` or `const T*`; `ref` receivers dispatch
      to `T&` or `T*`, with reference overloads taking precedence and no POD
      copy.
- [x] The 0.10 normative contract freezes const-generic and native-component
      rules in `docs/spec/WIO_LANGUAGE_SPEC_0_10.md`.
- [x] P1-D qualification passed the focused const-generic/native-component
      runtime and diagnostic tests, including generated C++ compilation and
      direct native extension receiver dispatch.

## P1-C generics and type-system sprint

- [x] Generic functions, aliases, interfaces, components, and objects accept
      trailing default type parameters, including dependent defaults such as
      `<T = i32, U = T>`. Deduction wins and defaults fill only unresolved
      parameters; invalid ordering, pack defaults, and forward references have
      focused diagnostics.
- [x] Object and component specialization now supports exact and partial
      patterns. Exact matches outrank partial matches, specificity orders
      partial candidates, equal best matches are diagnosed as ambiguous, and
      specialization visibility works across merged modules.
- [x] Readable `where Parameter: Trait` clauses are supported on generic
      functions, aliases, interfaces, components, and objects and share the
      established `@Apply` predicate and user-trait machinery. `+` composes
      multiple predicates conjunctively within one parameter slot.
- [x] Defaulted generic parameters participate in `@Instantiate(...)` for
      native functions, including dependent-default substitution before
      concrete backend instantiation.
- [x] `std::meta` gained `AllSame`, `IndexOf`, and `UniqueCount` as free
      functions and `Types<Ts...>` methods.
- [x] The versioned 0.9 generics contract defines completion order,
      specialization ordering and ambiguity, cross-module visibility,
      constraints, invariant compatibility, and native/export boundaries in
      `docs/spec/WIO_LANGUAGE_SPEC_0_9.md`.
- [x] P1-C qualification passed the 109-test generic/pack matrix, the complete
      508-test repository matrix, and 259 deterministic frontend/backend fuzz
      candidates generated from 250 seeded mutations.

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

- [x] Fixed arrays support inferred extents through `[T; _]`, including empty,
      copied, global, field, generic-element, and nested initializers.
      Inference materializes ordinary fixed storage before codegen and rejects
      missing, dynamic, ragged, and non-variable inference contexts.
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

## v0.11 Desktop and Editor Validation

- [x] Atlas Desk landed as a substantial native desktop workspace dashboard
      using the `application` lifecycle, stack-resident systems, async
      coroutine scanning and cancellation, typed attributes, modern native
      declarations, extensions, Option/Result, JSON, and Unicode APIs.
- [x] Atlas Desk was built and smoke-rendered with the published Wio `0.11.0`
      portable toolchain, with its rendered dashboard retained as release
      evidence.
- [x] `wio-vscode` was rebuilt for `0.11.0` around modular CLI, diagnostics,
      source-index, and provider layers with modern grammar and snippets.
- [x] Compiler and extension releases now share their major/minor release line;
      the matching extension check, unit-test, Windows/Ubuntu CI, and VSIX
      package gates are part of the release policy.

## v0.14 Standard Library and SDK Value Parity

- [x] Stable Option, Result/unit, tuple, nested collection, queue, set, checked
      span-range, and owned ByteBuffer values gained C++ SDK round-trips.
- [x] ABI descriptor v8 gained exact const-generic metadata and analysis-time
      rejection for unsupported or unspecialized export shapes.
- [x] The SDK feature inventory gained explicit Supported, Partial, and
      Deferred states and a normative 0.14 parity matrix.
- [x] Unicode normalization, exact JSON numbers, typed serialization codecs,
      bounded regex records, deterministic vectors, and container invariants
      landed with focused conformance tests.

## v0.15 Typed and Behavioral Attributes

- [x] Declaration-leading `[Attribute]` applications, composition, retention,
      target/require/conflict/cardinality policies, and deterministic ordering
      became the canonical typed model for built-ins and user attributes.
- [x] Compile-time validators, checked method derives, typed receiver guards,
      typed post/finally/around hooks, coroutine pre/post/finally execution, and
      explicit rejection of unsupported async around behavior were frozen.
- [x] Runtime reflection exposes normalized attribute applications and method
      pipelines; module ABI v9 exposes retained type/field/method/export
      descriptors to C++ hosts.
- [x] `wio migrate attributes --check|--write` converts legacy `@` syntax while
      preserving strings and comments, and compiler lowering resolves built-ins
      through their canonical typed identities.

## v0.16 Ownership, Async Hosting, and Applications

- [x] ABI descriptor v10 gained host-owned, main-thread-affine application
      state with non-blocking update, explicit main pumping, contained errors,
      stale-generation rejection, and retained module lifetime.
- [x] Stable scalar exported async functions gained typed C++ task handles with
      poll, explicit wait, deadlines, cancellation, completion callbacks,
      main-executor delivery, and safe shutdown.
- [x] Cancellation now propagates into directly awaited children;
      `TryWithCancellation` and filesystem/process/network token overloads
      preserve expected cancellation as ordinary Option/Result outcomes.
- [x] `WioHostCallback` gained typed scalar signatures, retainable userdata,
      thread declaration, and exception containment across the native boundary.
- [x] `WioOwnedNativeResource`, `WioBorrowedNativeResource`, and the move-only
      SDK `UniqueNativeResource` wrapper froze transferred versus borrowed
      native ownership with exactly-once cleanup.
- [x] Applications gained real monotonic frame deltas, deterministic explicit
      and fixed schedules, explicit `ref`/`view` resource injection, hosted
      headless execution, and reverse partial-start rollback.
- [x] Console, resource/tool, desktop event-loop, fixed game-loop, service I/O,
      callback, native-resource, and native-host scenarios entered the Windows
      and Ubuntu release acceptance matrix.

## v0.17 Attribute-driven Application Surface

- [x] Application and system bodies accept ordinary default-mutable fields and
      ordinary helper functions while retaining stack-resident lowering.
- [x] Conventional `Start`/`Update`/`Close` and descriptive
      `[Start]`/`[Update]`/`[Close]` functions normalize to one lifecycle ABI.
- [x] `[Fixed]`, `[After]`, and `[Main]` produce a deterministic stage graph;
      source-module system fields enter that graph automatically.
- [x] `[Worker]` is reserved with an explicit safety diagnostic until
      cross-thread `ref`/`view` conflict analysis is available.
- [x] Legacy v0.16 application syntax remains accepted and has a documented
      migration map instead of a flag-day removal.
- [x] SDK ABI v11 and `ApplicationHost::stages()` expose the normalized stage
      graph, dependencies, fixed frequencies, order, affinity, and stage kind
      flags to native hosts.

## Backend-neutral WIR Foundation

- [x] Typed WIR and canonical Lowered WIR gained stable type/function/block/
      value identities, source spans, deterministic printers, and independent
      structural verifiers.
- [x] Structured control flow, SSA block arguments, conditionals, loops,
      short-circuiting, match projections, arrays, numeric conversion, and
      literal typing lower without backend-specific recovery guesses.
- [x] The place and memory model now makes locals, initialization, loads,
      stores, fields, indices, borrows, construction, and reverse lexical
      cleanup explicit for component values and owning object handles.
- [x] Object/interface methods are receiver-aware WIR functions; nominal types
      retain deterministic override slots and abstract entries, while direct,
      virtual, and interface dispatch remain distinct verified operations.
- [x] Safe object/interface upcasts, checked `fit`, runtime `is`, object
      identity equality, and `self`/`deref self`/`ref`/`view` return semantics
      survive unchanged into Lowered WIR.
- [x] The Callable Model freezes overload results and generic specialization
      identities in WIR; named function values, closures with ordered
      value/reference/retained-self environments, indirect calls, and
      extension implementation calls are distinct verified operations that
      survive canonical lowering unchanged.
- [x] The Value and Container Model preserves dictionary construction and
      keyed read/write places, array/string/text indexing, structured string
      and Unicode interpolation, enum/flagset constants and intrinsics, `any`
      boxing/testing/casting, nullable wrapping, and nominal
      Option/Result/Tuple/Span identities through verified canonical lowering.
- [x] Direct `dictionary[key]` reads and writes are accepted by semantic
      analysis and use the same checked native intrinsic as existing direct
      array/string/text indexing.
- [x] The Ownership and Cleanup Model classifies trivial, owned-value,
      intrusive-reference-counted, borrowed, and generic types; records borrow
      lifetimes; makes managed copy/move/replace/release/drop explicit; lowers
      object copies and drops to retain/release while keeping component/value
      glue distinct; and verifies exactly-once local cleanup across CFG exits.
- [x] The Native Interop and ABI Model gives native declarations a canonical
      C/C++ symbol, header, stable identity, failure boundary, receiver mode,
      parameter/result ownership and marshalling contract. Native POD,
      `opaque`, ref/view, callbacks, and generic template specializations now
      lower through explicit `native-call`/`native-invoke` operations.
- [x] Deterministic native thunk planning and the C-shaped
      `wio_native_abi.h` SDK contract define concrete specialization adapters,
      owner-provided intrusive handle operations, callback lifetimes/thread
      policy, contained failures, and exactly-once foreign-resource release.
- [x] Realm-qualified calls no longer probe namespace identifiers as method
      receivers, and contextual `ref` arguments use the selected ABI parameter
      instead of leaking semantic `<unknown>` placeholders into WIR.
- [x] The Module, Export, and SDK Model gives each module a
      checkout-independent identity and retains explicit Wio/standard/native
      dependencies, stable exports, concrete generic SDK signatures,
      reflection descriptors, lifecycle/state-transfer hooks, and deterministic
      call-table slots through Typed and Lowered WIR.
- [x] Module verifiers reject stable-ID drift, duplicate exports, call-table
      mismatch, invalid targets, malformed reflection, and unpaired hot-reload
      hooks. The versioned C-shaped `wio_module_contract.h` sidecar adds
      stable-ID lookup without breaking the existing `WioModuleApi` v11 ABI.
- [x] The Async, Coroutine, and Thread WIR Model distinguishes task await from
      executor handoff, records known scheduler operations and affinity, and
      lowers every suspension into a cancellation check plus canonical
      suspend/resume state. Async returns become coroutine completion, while
      conservative frame slots retain explicit ownership and cleanup metadata
      for both the future C++ backend and VM.
- [x] Typed and Lowered async verifiers reject payload drift, coroutine layout
      misuse, missing cancellation checks, malformed state/resume edges, and
      ordinary returns inside lowered async functions. Deterministic printers
      expose state ids, affinity, frame size, and thread-switch behavior.
- [x] The Application and System WIR Model freezes stack-resident system types,
      application entry/lifecycle identities, deterministic stage order and
      dependencies, fixed/main affinity, resolved run callables, and typed
      read/write resource injection in the backend-neutral module contract.
- [x] Effective attributes now retain canonical target/origin/argument/
      retention data and ordered processor phases in WIR. Reflection records
      expose fields, methods, visibility, mutability, dispatch slots, async
      state, and shared attribute identities; Typed and Lowered verifiers and
      printers cover the complete contract.
- [x] The Language Surface Model gives globals stable declarations and
      initializer functions, and lowers their reads and writes through explicit
      global places. Range, array, and dictionary `for-in` share canonical
      iterator operations with index/key bindings and structured loop cleanup.
- [x] Result unwrap/propagation, duration literals, range containment, generic
      constant/pack types, per-operand pack expansion, and resolved overloaded
      operators now retain backend-neutral operations and identities through
      verified Typed-to-Lowered WIR.
