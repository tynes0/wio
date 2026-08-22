# Changelog

All notable user-facing changes to Wio are recorded here.

## [Unreleased]

### Added

- Added a public SDK product-version contract through `wio_version.h`,
  `wio::sdk::product_version`, and `product_version_string`. A release manifest
  now aligns compiler, runtime, std, CLI, SDK, VS Code, and documentation
  product versions while keeping the module ABI descriptor revision independent.
- Added explicit fixed-array extent inference with `[T; _]`, including empty,
  copied, global, component-field, and nested fixed arrays. Ragged nesting,
  dynamic-array sources, missing initializers, and non-variable use sites now
  receive semantic diagnostics.
- Added `const string` and `const text` declarations with literal values,
  constant references, concatenation, comparison, matching, and global, local,
  or component-static storage.
- Added `string` and `text` const generic parameters on functions, aliases,
  interfaces, components, and objects, including defaults, specialization,
  qualified module constants, and a Wio-owned C++20 structural representation.
- Added runtime generic reflection through
  `std::reflect::GenericParameterNames<T>()` and `GenericArguments<T>()`.
  `Describe<T>()` now includes source parameter names and concrete type or
  const-value arguments for primary, exact, and partial specializations.
- Added deterministic compile-time evaluation budgets for const dependencies:
  128 nesting levels, 16,384 visited nodes, and 1 MiB of folded text, with a
  dedicated cyclic-dependency diagnostic before backend generation.
- Constant `string` and `text` interpolation now accepts constant-evaluable
  embedded expressions while continuing to reject runtime calls and bindings.
- Added the first-class `text` primitive and validated UTF-8 `u"..."` and
  interpolated `u$"..."` literals. Text supports Unicode code-point counting
  and slicing, read-only indexing, grapheme counting/slicing, display width,
  case folding, byte counts, concatenation, comparison, hashing, matching,
  generic code-point/grapheme iteration, console output, and explicit
  `std::unicode` UTF-8/UTF-16/UTF-32 boundaries. Fallible decoding reports a
  `Result<text>` and rejects invalid UTF-8, unpaired UTF-16 surrogates, and
  invalid UTF-32 scalar values.
- Added source-tree host-interop coverage for exported functions that accept
  and return `text`, including Unicode content and code-point operations.
- Added runtime reflection metadata for `text`, including its stable language
  name, primitive kind, size, and alignment.
- Added `std::traits` queries for primitive, byte-string, and Unicode-text
  identity, and extended `IsArrayType` to recognize fixed arrays.
- Added compact user-defined attribute declarations such as
  `attribute route(fn)(method: string) with attribute::runtime;`.
- Added named arguments for user-defined attribute applications with duplicate,
  unknown, missing-required, and ordering diagnostics.
- Typed attribute applications now accept folded scalar, `string`, and `text`
  constants. Trailing defaults are materialized into declaration order before
  runtime reflection, including defaults that reference other constants.
- Added structural partial-specialization ordering, including repeated generic
  parameter relationships such as `Pair<T, Box<T>>`.
- Added trailing default parameters to component extension methods.
- Added generic component extension methods with deduction, explicit type
  arguments, trailing generic defaults, and `where` constraints.
- Added boolean guards to literal, alternative, range, and enum match arms, plus
  exhaustive value-producing enum matches without a redundant `assumed` arm.

### Changed

- Made typed `with`/`using` native metadata the canonical spelling throughout
  current guides and migrated all standard-library header imports to
  `using cpp::header(...)`; legacy `@...` spellings remain compatible.
- `wio bind new`, `wio bind import`, and every `wio project new` template now
  emit the canonical `with`/`using` attribute syntax. Binding smoke tests
  compile the generated modules and assert the emitted spelling.
- Source-tree tests now pin `WIO_ROOT` and `WIO_HOME` to their checkout so an
  older installed toolchain cannot silently supply mismatched std/runtime/SDK
  files during validation.
- Published the normative Wio 0.13 coherence/Unicode specification and linked
  its text, const-generic, fixed-array, attribute, extension, specialization,
  and match rules to the conformance corpus.
- Text may flow safely to a UTF-8 `string`; constructing `text` from a
  potentially invalid `string` requires the fallible
  `std::unicode::FromUtf8` conversion.
- Invalid textual operators and mixed `string`/`text` expressions now fail in
  semantic analysis instead of surfacing as C++ backend errors.
- Attribute declaration policies can use namespaced postfix policy attributes
  instead of the legacy `for`/`retain`/`repeatable` keyword sequence; the old
  declaration spelling remains accepted for compatibility.
- Nested generic closers no longer require whitespace: `>>`, `>=`, and `>>=`
  are split contextually while parsing generic types/calls and remain shift or
  comparison operators in expressions.
- Textual const generics remain a Wio-owned type-system feature; native
  functions and native component templates continue to accept only integer
  const parameters until an ABI contract is deliberately added.

### Fixed

- Isolated all source-tree tests from ambient `WIO_ROOT`/`WIO_HOME` values so
  an older installed toolchain cannot supply mismatched std, runtime, or SDK
  headers during repository validation.
- Distinguished byte-string and Unicode-text literals in typed attribute
  validation instead of treating `text` as an integer-like fallback type.
- Fixed literal-to-literal byte-string concatenation and comparison generating
  raw C++ pointer operations instead of Wio string-value operations.
- Avoided generic reflection template parameters shadowing the stable
  `TypeReflection::Name` metadata member, including partial specializations
  whose source parameter names differ from their primary declaration.
- Primitive generic arguments now use stable Wio names such as `i32`,
  `string`, and `text` instead of the backend's `<unknown>` fallback.
- Preserved keyword-shaped `if (... fit name)` bindings after `text` became a
  first-class type keyword, and synchronized the indexed-value diagnostic test.
- Gave the repeated asynchronous listener-close stress test a CI-safe timeout
  budget while retaining all 64 ownership/cancellation iterations.
- Fixed object methods referencing global scalar, `string`, or `text`
  constants before their generated C++ definitions.
- Immutable global declarations now preserve `const` in their generated C++
  forward declarations.
- Fixed host-interop tests compiling against the source SDK while accidentally
  resolving runtime headers from an older installed Wio distribution.
- Moved asynchronous listener readiness onto the dedicated I/O executor with
  a native pre-scheduling lease. Closing a listener now drains accept work
  without depending on the general blocking pool on Linux. POSIX socket waits
  use an explicit non-blocking wake pipe, so close does not rely on
  `shutdown()` waking `select()` for listening sockets.

## [0.12.0] - 2026-08-13

### Added

- Added the `std::async::Task<T>` public alias plus `Send`/`Sync` executor
  safety markers for synchronized user-owned types.
- Added blocking-executor capacity/runtime diagnostics and explicit,
  idempotent async runtime shutdown.
- Added owning heterogeneous async scopes, lexical `async scope`, `spawn`,
  deadlines, cancellation propagation, explicit detach, and owner/main-thread
  dispatch integrated with application lifecycle.
- Added dedicated filesystem and network I/O execution, cancellable file
  watching, asynchronous DNS/TCP/UDP operations, and ownership-safe async
  listener accept.
- Added Result-preserving asynchronous process helpers and owned process
  streams with separate stdin/stdout/stderr, wait, terminate, close, and
  live-state diagnostics.
- Added bounded and unbounded `AsyncChannel<T>` with non-blocking try
  operations, suspending send/receive, close, drain, and cancellation behavior.
- Added scoped enum backend emission and module re-import alias regression
  coverage.

### Changed

- `RunBlocking` now uses a distinct bounded blocking pool configured through
  `WIO_ASYNC_BLOCKING_WORKERS` and `WIO_ASYNC_BLOCKING_QUEUE`; it no longer
  consumes continuation/timer workers.
- Async timer suspension is cancellation-aware and wakes cancelled work
  promptly instead of retaining it until the original deadline.
- `Run` and `RunBlocking` now reject borrowed, opaque, callable, or unsafe
  object captures before C++ generation. Structurally safe values pass
  automatically; synchronized objects opt in through `std::async::Send`.
- Network connect timeouts now govern the actual non-blocking connection
  attempt on Windows and POSIX instead of only later socket I/O.
- Socket handles retain native descriptors until in-flight leases drain, so
  close interrupts blocking work without descriptor reuse or mutex deadlock.
- Release validation now runs on release branches, supersedes stale runs,
  stresses async/process ownership under sanitizers, and fails fast on hung
  network regressions.

### Fixed

- Fixed cancelled coroutines continuing past `Yield`/`Sleep` suspension and
  executing user code after cancellation.
- Fixed shutdown races between accepted blocking work, continuation posting,
  delayed timers, and process/static destruction.
- Fixed async process leases being acquired after coroutine suspension,
  stream-drain ordering, and UDP test scheduling that could self-starve a
  bounded I/O executor.
- Fixed Linux/Windows socket close races that could report a false UDP success,
  reuse a descriptor during receive, or leave async accept blocked forever.
- Fixed cached-module re-imports losing exported-symbol and top-level-realm
  metadata, which broke a second alias of the same module.
- Fixed duplicate/rejected function declarations leaving an expired weak
  symbol for the semantic resolution pass and crashing sanitizer fuzzing.

## [0.11.1] - 2026-08-11

### Added

- Added `wio project build --emit-cpp`, which retains generated C++ in the
  manifest-resolved output directory while preserving source roots, native
  includes/sources, link configuration, target, and output policy.
- Added Atlas Desk, a substantial Raylib desktop workspace dashboard exercising
  application/system lifecycle, async scanning and cancellation, typed
  attributes, modern native declarations, extensions, Option/Result, JSON, and
  Unicode APIs.
- Added a dedicated project emit-C++ regression gate.

### Changed

- The compiler, package, installer, examples, and companion VS Code extension
  advance together to `0.11.1`.
- Explicit `--intermediate-dir` now controls retained C++ placement even when
  `--emit-cpp` is selected; source-adjacent output remains the standalone
  default when no intermediate directory is supplied.

### Fixed

- The 0.11.1 VS Code extension now routes manifest-owned files through
  `wio project build/run/describe`, preserving C++ headers, native sources,
  libraries, source roots, application entry, host targets, and working paths.
- Standalone library/source files are checked as non-executable targets and no
  longer report a misleading missing-`Entry` error; attempting to run one now
  produces an actionable library explanation.
- Corrected editor C++ emission, project-scoped diagnostic replacement,
  compile diagnostics during run, multi-root settings, and native-source save
  checks.

## [0.11.0] - 2026-08-10

### Added

- Added the multi-module Wio Observatory example, combining const generics,
  Option/Result flows, JSON reporting, reflection, modern standard-library
  containers/utilities, and direct native POD component extensions in one
  practical workspace-audit application.
- Added typed postfix attributes and scoped activation, ordinary-call pipeline
  operators, Option/Result/array match destructuring, and the deterministic
  stack-resident application/system lifecycle.
- Added `async fn`, async object/interface methods, `await`, hot shared
  `coroutine<T>` tasks, async `Entry`, and a C++20 worker/timer runtime.
- Added `std::async` task state, blocking/worker bridges, cancellation and
  deadlines, All/Any/Race, recoverable timeout, generic/void task groups,
  cancellable sleep, worker configuration, and owner-thread dispatch queues.
- Added Windows/Ubuntu 0.11 freeze gates, native async runtime stress,
  ASan/UBSan runtime qualification, and sanitizer-guided frontend fuzzing CI.

### Changed

- Lambda capture is value-by-default: primitive/component values snapshot,
  object captures preserve shared identity, and explicit `ref/view` captures
  remain borrows.
- Distribution packaging now installs the complete Markdown documentation and
  validates the 0.11 language, std, async, and freeze contracts.
- Self-hosted CLI generation is now an incremental build artifact instead of
  an unconditional application post-build step; redundant builds no longer
  relink or race the executable.
- The package/compiler candidate version advances to 0.11.0.

### Fixed

- Fixed generic object-method mangling when a generic parameter is nested in
  `coroutine<T>`.
- Fixed coroutine final-frame ownership and task-state destruction races.
- Fixed process shutdown hanging on distant detached timers; shutdown drains
  their continuation cleanup without honoring the remaining timer delay.
- Fixed scheduler shutdown continuation posting and removed mutation of a
  `priority_queue::top()` element through `const_cast`.
- Fixed escaping object-method lambdas retaining only a raw backend `this`;
  closures that use `self` now keep the object alive.
- Fixed nondeterministic import-alias conflict diagnostics and missing static
  array extent recovery.
- Fixed single-configuration builds placing `wio-selfhost` in a configuration
  subdirectory where the primary CLI could not discover it.
- Fixed the sanitizer-guided frontend harness releasing semantic symbols before
  C++ generation, which made valid corpus inputs fail with dangling bindings.
- Backend executables/shared libraries now link to a staging file and replace
  the destination with bounded retry, avoiding transient Windows executable
  locks without hiding persistent failures.

## [0.10.0] - 2026-08-05

### Added

- Added ordinary integer const generics across functions, aliases, interfaces,
  components, and objects, including defaults, top-level const evaluation,
  static-array extents, value substitution, specialization, and C++ template
  argument emission.
- Published the normative Wio 0.10 const-generic and declaration-level native
  component specification.
- Added direct `@Native` extension methods for declaration-level native
  components, making C++ free functions available through Wio method syntax.

### Changed

- Declaration-level native generic components now support Wio exact/partial
  specialization while retaining one inherited C++ POD-template ABI mapping.
- Generic diagnostics now distinguish type arguments from compile-time integer
  arguments before backend generation.
- Native extension receivers now prefer C++ references and fall back to
  matching mutable/const pointer APIs without copying the component.

## [0.9.0] - 2026-08-04

### Added

- Added trailing default type parameters across generic functions, aliases,
  interfaces, components, objects, and object methods, including dependent
  defaults such as `<T = i32, U = T>`.
- Added partial object/component specialization with deterministic exact,
  partial-specificity, and primary-declaration ordering plus ambiguity
  diagnostics and cross-module visibility.
- Added readable generic constraints through `where T: Trait`, including
  conjunctive same-slot predicates with `where T: TraitA + TraitB`.
- Added `std::meta::AllSame`, `IndexOf`, and `UniqueCount` as free functions
  and `Types<Ts...>` helpers.
- Published the normative Wio 0.9 generics and constraints specification.

### Changed

- Generic inference now binds observable parameters first and fills only the
  remaining holes from defaults.
- Defaulted generic parameters now participate in native `@Instantiate(...)`,
  including dependent-default substitution before backend instantiation.
- Generic compatibility is explicitly invariant in every type argument.

## [0.8.0] - 2026-08-04

### Added

- Added explicit nullable types (`T?`), grouped nullable function/borrow types,
  nullable array/generic elements, flow-sensitive null narrowing, and SDK
  nullable type descriptors.
- Added deterministic object/component `OnDestruct` coverage and
  `std::resource::Owned<T>` / `Borrowed<T>` with idempotent disposal, release,
  final-owner cleanup, and live-resource diagnostics.

### Changed

- Object/interface, opaque, function, and borrow types are non-null by default;
  non-null local/global handles require initialization and nullable values must
  be narrowed before use.
- Native C++ exceptions are translated at generated Wio wrappers, while panic
  unwinding now has an explicit cleanup contract.

### Fixed

- Inline object/component lifecycle hooks can reference module globals because
  mutable globals receive generated forward declarations.
- Backend errors in generated native wrappers now map to the Wio native
  declaration instead of synthetic wrapper line numbers.
- Fixed LP64 integer literal lowering so `i64`, `u64`, `isize`, and `usize`
  retain their Wio types during C++ generic deduction on Linux.

## [0.7.0] - 2026-08-03

### Added

- Added the complete `Result<T>` combinator surface: `Map`, `MapError`,
  `AndThen`, `OrElse`, `Inspect`, `InspectError`, `Flatten`, `ToOption`,
  `Collect`, and `Sequence`.
- Added checked and saturating add/subtract/multiply operations for all ten
  integer types, plus explicit wrapping semantics for ordinary integer
  arithmetic and runtime diagnostics for division by zero.
- Added first-class enum raw-value conversion through member `Value()` and
  `IsValid()` plus generic `reflect::TryFromValue`, `FromValue`, and
  `IsValid` APIs, including safe handling of unknown native enum values.
- Added structured filesystem metadata, canonicalization, atomic replacement,
  native error codes, and clean installed-package path/filesystem qualification
  on Windows and Linux.

### Changed

- Canonical `std::fs` operations now return `Result<T>` with filesystem domain,
  portable code, native OS code, and actionable messages. Explicit `Try*` and
  `*Raw` compatibility helpers remain available for low-level use.
- Intrinsic array, string, and dictionary `Get` operations now return
  `Option<T>` while strict `At` operations retain failure-on-missing behavior.
- Queue, set, span, range, buffer, pool, array, string, and dictionary access,
  cloning, capacity, equality, removal, and iteration contracts are aligned.
- Implicit numeric conversion now permits safe widening only; narrowing in
  initialization, assignment, arguments, and returns requires explicit `fit`.

### Fixed

- Wrapping integer helpers remain valid in compile-time scalar and std-meta
  expressions.
- Self-hosted CLI filesystem call sites now preserve and propagate structured
  errors instead of collapsing failures into empty values or booleans.
- Distribution packaging, installed-package qualification, packaged file-run,
  and performance smoke tests no longer race while sharing the build tree.

## [0.6.0] - 2026-08-03

### Added

- Added an explicit `ref`/`view` lifetime model that tracks static, caller,
  local, and temporary borrow origins across members, arrays, spans, object
  handles, `self`, and `deref self`.
- Added deterministic compiler-pipeline fuzzing for malformed syntax, invalid
  UTF-8, import cycles, nested interpolation, dry-run/emission agreement,
  backend syntax, timeouts, and diagnostic/output budgets.
- Added optional Clang libFuzzer plus ASan/UBSan frontend integration and
  documented filesystem-error and reference-lifetime policies.

### Changed

- Numeric promotion is now operand-order independent, with semantic rejection
  for floating-point modulo, bitwise, and shift operations.
- `null` is restricted to nullable runtime categories instead of being
  accepted for primitive, component, array, and dictionary values.
- Function types lower to nested C++ `std::function` callables, including
  higher-order signatures.
- `use` parsing now follows a hardened state machine, import-all aliases retain
  both direct and namespaced symbols, and bitwise operator precedence follows
  conventional ordering.

### Fixed

- Local and temporary references can no longer escape through returns,
  storage, assignment, nested members, arrays, or spans; unsupported native
  reference returns now receive a targeted diagnostic.
- Imported parser failures and poisoned semantic types no longer produce
  derivative diagnostic cascades.
- Compiler filesystem failures now report deterministic diagnostics instead of
  throwing, including directory creation and short-read failures.
- Integer-looking floating literals now emit valid C++, null-reference
  comparisons lower correctly, backend diagnostics initialize deterministically,
  and interface reflection uses interface symbols in generated C++.

## [0.5.1] - 2026-08-03

### Added

- Added `std::process::ExecutablePath()` for resolving the current process image
  independently of the working directory and `argv[0]` spelling.

### Fixed

- PATH-dispatched `wio` invocations now find the adjacent self-hosted companion
  from the real executable location, including bare `wio` calls from `cmd.exe`.
- CLI-intended commands no longer fall through to the native compiler when the
  companion is missing, and compiler diagnostics no longer expose timestamped
  `[level] WIO LOG` envelopes in normal console output.

## [0.5.0] - 2026-08-01

### Added

- Added the conditional expression operator (`condition ? whenTrue : whenFalse`)
  with boolean-condition and compatible-branch diagnostics.
- Explicitly typed `let` and `mut` declarations may omit an initializer and
  receive deterministic value initialization.
- Keyword-shaped API names can be used as contextual identifiers in realms,
  imports, declarations, member access, and calls when the grammar is
  unambiguous.
- Added public `std::environment`, `std::platform`, and `std::statistics`
  modules for process/user environment management, persistent user PATH
  mutation, operating-system/architecture introspection, and measurement
  summaries.
- Added recursive filesystem copy/list helpers and PATH-based executable
  discovery to the public standard-library tooling surface.
- Added executable-permission queries and mutation to `std::fs`; POSIX project
  packages now emit directly runnable `run.sh` launchers.
- Added clean installed-package qualification that installs a staged package
  into an isolated root and validates the self-hosted CLI, bundled backend,
  project lifecycle, native interop, binding generation, and packaging.

### Changed

- Completed the self-hosted CLI migration. Project, file, environment,
  binding, package, performance, developer, help, and global dispatch behavior
  now execute in the Wio + Argonaut-Wio companion.
- Binding JSON-manifest generation and C/C++ header importing now execute in
  Wio through `std::json`, `std::regex`, `std::fs`, and `std::path`.
- Release package staging, portable backend discovery/copy, metadata,
  quickstart, archive, and installer orchestration now execute in Wio.
- Removed the generic native fallback, private compiler-service bridge, and
  obsolete C++ tooling, environment, file, performance, binding, package, and
  process CLI layers.
- Stage-0 companion launching now uses the shared host build of the public
  runtime process primitive.

### Fixed

- Self-hosted command routing preserves application arguments and child exit
  codes while raw source/compiler invocations continue to enter stage-0
  directly.
- Persistent environment setup/removal now handles duplicate process keys and
  user-scoped PATH updates consistently on Windows and POSIX.
- Contextual keyword parsing keeps packaged APIs such as
  `std::path::Extension` callable.
- Windows release packaging bundles a discoverable MinGW backend; POSIX
  packages use the host `g++` and `ar` without attempting to copy `/usr`.
- Standalone Windows package installers extract their adjacent ZIP, forward
  named installation options correctly, and prefer fast `tar.exe` extraction
  with an `Expand-Archive` fallback.

## [0.3.0] - 2026-07-25

### Added

- Added `std::hash` with default FNV-1a hashing and SHA-256 hex/byte digests.
- Added deterministic MT19937, xoroshiro128+, LXM, and Wichmann-Hill random
  generators, with MT19937 as `std::random::Random`.
- Added `ByteBuffer`, generation-checked `BytePool`, typed `Pool<T>`, and active
  `byte`/`bit` language aliases.
- Added queue, ordered/unordered set, heterogeneous tuple, regex, time,
  view-backed span-range, and explicit sorting modules.
- Array `Sort`/`Sorted` now use adaptive sorted/reversed/small/dense-integral
  paths before the general comparison-sort fallback.
- Expanded `std::traits` with category queries and constraints, including
  user-defined nominal generic-interface predicates in `@Apply`.
- Expanded reflection from enums/flagsets to component/object/interface type,
  field, method, access, base, size, and alignment metadata.
- Added the `std::convert` module with checked `Result<T>` parsers, `TryTo*`
  output-reference helpers, direct conversions, base-2-through-36 integer
  parsing/formatting, prefix auto-detection, and structured parse errors.
- Added `string.ToI8()` through `string.ToUSize()`, floating-point and boolean
  conversion members, plus generic `std::ToString<T>`/`convert::ToString<T>`
  formatting for standard values, containers, enums, and Wio objects with a
  `ToString()` method.
- Added `std::chars`, ASCII-aware string comparison/whitespace/composition
  helpers, and a generic algorithm utility wave covering distinct/filtering,
  chunking/windowing, flattening, ranges, repetition, sequence equality, and
  sums.
- Bare project actions discover manifests from the current directory and its
  ancestors, so `wio project describe`, `wio project build`, and
  `wio project run` work without `--project`.
- `wio run` is a shorthand for project execution.
- `wio help <command> [subcommand]` routes to contextual help across all CLI
  command families.
- Project execution accepts application arguments after `--`, repeated
  `--arg` values, `--no-manifest-args`, `--no-build`, `--cwd`, and
  `--print-command`.
- Command-specific `--version` and clearer group-specific help are available
  across the CLI.
- Bare command groups show successful help, and close subcommand misspellings
  receive suggestions for file, project, binding, environment, performance,
  and developer commands.

### Fixed

- File and project runners preserve spaces, flag-like values, shell-special
  characters, literal `--` values, and the child application's exact exit
  code.
- Project, package, and performance subprocesses use one direct cross-platform
  launcher instead of shell command strings.
- Empty command parsing, `--` positional parsing, parser reuse, built-in
  help/version handling, and conflicting argument declarations are hardened in
  Argonaut.
- Unknown top-level commands report a targeted error and suggest close command
  names instead of falling through to file compilation.

## [0.2.0] - 2026-07-19

### Added

- Explicit full specializations for generic objects and components with
  `@Specialize(...)`.
- Runtime and semantic support for returning `self` as `view`/`ref`, and for
  returning an object value through `deref self`.
- Release tests for generic specialization, constructor deduction, object
  self returns, SDK method overloads, and packaged file execution.

### Fixed

- Generic object and component constructor bodies are validated after their
  type arguments have been inferred.
- SDK descriptor validation now accepts legitimate method overloads while
  still rejecting duplicate signatures.
- `i8` and `u8` interpolation is formatted numerically instead of as a
  character.
- Windows subprocesses preserve all PATH entries when duplicate `PATH`/`Path`
  environment keys are present.
- Package smoke discovery ignores neighboring installer files and the package
  quickstart documents persistent environment setup.
- Windows GNU-host builds use wide filesystem APIs, and the deleted nested std
  import regression fixture is restored.

### Release

- CLI and compiler versions now derive from the CMake project version.
- Package, installer, documentation, and example surfaces are aligned on
  `0.2.0`.

## [0.1.0-rc.1]

- Initial public release candidate.
