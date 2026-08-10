# Changelog

All notable user-facing changes to Wio are recorded here.

## [Unreleased]

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
