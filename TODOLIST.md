# Wio Active Backlog

This file contains only unfinished work. Completed milestones live in
`COMPLETED.md`.

Status markers:

- `[ ]` not started
- `[~]` partially implemented or implemented but not sufficiently hardened

The priorities below reflect the state of Wio after `v0.4.0`, including
real-world validation with packaged projects and a native raylib desktop
application.

## P0 - Release Blocking Correctness

1. [ ] Stop cascading diagnostics after a root parser/type error.
   One invalid token in a std module currently produces dozens of undefined
   symbol, invalid operand, and unknown type errors. Track poisoned nodes/types
   and suppress derivative diagnostics while preserving independent errors.

2. [~] Finish `ref` / `view` / temporary lifetime semantics.
   Rvalue component extension calls such as
   `Parse(...).Value().ToString()` now have a targeted backend fix, but the full
   matrix is not closed. Specify and test temporary components, nested member
   access, returned arrays/spans, object handles, `self`, `deref self`, mutable
   receiver rejection, and native borrow boundaries.

3. [ ] Add parser, lexer, semantic, and generated-C++ fuzzing.
   Include nested interpolation, malformed generics, deep type nesting, import
   cycles, invalid UTF-8, arbitrary token streams, and differential
   compile/check runs. Crashes, hangs, excessive diagnostics, and invalid
   generated C++ should be release blockers.

4. [ ] Audit and close known source TODOs that affect correctness.
   At minimum: `Type::...` returning an empty placeholder in
   `sema/type.cpp`, arithmetic result typing based only on the left operand,
   incomplete null checks, parser refactor/improvement markers, and filesystem
   exception policy. Convert each into a focused test before fixing it.

## P1 - Language Semantics and Type System

1. [ ] Turn the draft into a versioned normative language specification.
   Publish formal lexical grammar, syntax grammar, name resolution, overload
   resolution, type compatibility, ownership/reference rules, evaluation
   order, initialization, destruction, generics, diagnostics, and feature
   status. Version the specification alongside releases.

2. [ ] Design a strict nullability model.
   Separate non-null object handles, nullable handles, `Option<T>`, `null`,
   `ref`, and `view`. Define null flow analysis, narrowing, container
   interactions, native nullable pointers, and SDK behavior.

3. [ ] Complete generic defaults, partial specialization, and specialization
   ordering. Define ambiguity rules, constraint ordering, default type/value
   parameters, specialization visibility across modules, and native/export
   interactions.

4. [ ] Expand const generics beyond pack indexing.
   Support ordinary non-type parameters such as `Vector<T, N>`, value
   substitution, constraints, constant evaluation, generic static arrays,
   diagnostics, and ABI/mangling rules.

5. [ ] Expand variadic and compile-time metaprogramming.
   Add `Take`, `Drop`, `Zip`, `MapTypes`, filtering/folding, value transforms,
   pack concatenation, pack constraints, and usable compile-time iteration.

6. [ ] Add modern generic constraint syntax and associated types.
   Provide readable `where` clauses, constraint composition, associated types,
   default implementations, better inference, variance rules, and precise
   generic failure diagnostics.

7. [ ] Strengthen pattern matching.
   Add Option/Result destructuring, enum payloads, component/array patterns,
   guards, exhaustiveness checks, unreachable-case diagnostics, and binding
   ownership/reference rules.

8. [ ] Complete component extension ergonomics.
   Add constrained generic extensions, extension properties, static extension
   functions, explicit conflict resolution, import-scoped visibility,
   documentation generation, reflection metadata, and editor completion.

9. [ ] Define numeric promotion and conversion rules.
   Remove the current left-operand result-type shortcut. Specify mixed signed
   and unsigned arithmetic, literal inference, overflow behavior, checked and
   saturating operations, enum conversions, and narrowing diagnostics.

10. [ ] Add first-class enum value conversion.
    Provide `Value`, `TryFromValue`, `FromValue`, validity queries, unknown
    native value handling, generic enum constraints, and serialization without
    manual `match` tables.

11. [ ] Formalize initialization, copy, move, destruction, and resource
    lifetime behavior. Cover objects, components, arrays, dictionaries, Box,
    native resources, early returns, exceptions/panics, and assignment.

12. [ ] Decide exception and panic semantics.
    Specify whether Wio has recoverable exceptions, panic-only termination, or
    Result-only recoverable errors; define stack cleanup, native exception
    translation, diagnostics, and ABI behavior.

13. [ ] Evaluate pipeline/data-flow operators only after semantics are stable.
    Measure `|>` and `<|` against ordinary calls, method chaining, error
    propagation, inference, and debugging before reserving syntax.

14. [ ] Clarify whether `system`, `program`, `every`, `after`, `during`, and
    `wait` belong to the core language, a standard DSL, or should be removed
    from the planned surface.

## P1 - Standard Library Correctness and Consistency

1. [ ] Repair and regression-test `std::path` and `std::fs` in packaged builds.
   Rename or contextually parse `Extension`, then test every path/fs function
   through both repository and installed toolchains on Windows and Linux.

2. [ ] Make filesystem APIs consistently `Result`-based.
   `ReadText`, `WriteText`, directory enumeration, metadata, copy/move/remove,
   permissions, canonicalization, and atomic replacement must preserve domain,
   native code, and actionable messages instead of returning empty strings or
   booleans.

3. [ ] Finish `Result<T>` combinators.
   Add `Map`, `MapError`, `AndThen`, `OrElse`, `Inspect`, `InspectError`,
   `Flatten`, `ToOption`, collection helpers, and consistent integration with
   `?()` / `!()`.

4. [~] Finish `Option<T>` adoption.
   Option, its core combinators, and array/string/span lookup APIs have landed.
   Add intrinsic `Dict.Get(key) -> Option<V>` without requiring `V()`, audited
   queue/set/map lookups, iteration helpers, `zip`, transpose with Result, and
   consistent naming across std.

5. [ ] Build a real Unicode text model.
   Define UTF-8 validation, codepoints/runes, grapheme clusters, safe slicing,
   Unicode case folding, normalization, categories, width, iteration, and
   conversion. GUI text input must not require a native
   `AppendCharacter` workaround.

6. [ ] Add byte/codepoint/string builders.
   Provide allocation-conscious `StringBuilder`, byte writer/reader,
   codepoint append, formatting sinks, reusable buffers, and clear ownership
   rules.

7. [~] Consolidate container contracts.
   Array, Dict, queue, ordered/unordered set, tuple, span, range, and buffer
   exist. Align `Get`/`At`, `First`/`Last`, iteration, mutability, cloning,
   capacity, reserve/shrink, removal, equality, hashing, sorting, and error
   behavior.

8. [~] Harden JSON into a production-grade module.
   Parsing/writing, nested values, errors, and pretty output exist. Add exact
   integer preservation, configurable duplicate-key policy, deterministic key
   ordering, streaming parser/writer, depth/size limits, UTF validation,
   JSON Pointer/Patch, schema hooks, and generic encode/decode traits.

9. [ ] Add serialization beyond JSON.
   Provide stable generic serialization traits and at least binary,
   Base64/hex integration, CSV, and configuration-friendly formats. Define
   versioning, unknown fields, migration, and enum policy.

10. [~] Harden time, random, hash, log, numeric, encoding, stream, UUID, and
    SemVer. These modules exist; add cross-platform vectors, deterministic
    contracts, cryptographic/non-cryptographic distinctions, secure random,
    time zones/calendars, structured log sinks, overflow matrices, streaming
    encoders, UUID parsing/variants, and full SemVer prerelease/build metadata.

11. [ ] Add regular-expression safety and completeness.
    Document the engine, escaping, Unicode behavior, capture APIs, replacement,
    iteration, catastrophic-backtracking limits/timeouts, and error model.

12. [ ] Add networking foundations.
    DNS, sockets, TCP/UDP, TLS, HTTP client/server primitives, URI, headers,
    multipart, WebSocket, cancellation, timeouts, proxies, and certificate
    validation are required before `std::http` can be considered.

13. [ ] Add concurrency foundations.
    Threads, mutexes, atomics, channels, task scheduling, futures, async I/O,
    cancellation, structured concurrency, thread-local storage, and runtime/
    host integration need one coherent model.

14. [ ] Add OS/application facilities.
    Environment variables, process pipes, signals, filesystem watching,
    clipboard, notifications, dialogs, user/config/cache directories, dynamic
    libraries, and platform capability queries.

15. [ ] Add data and utility modules.
    Date/time formatting, decimal/big integer, compression/archive, MIME,
    TOML/INI, database primitives, statistics, geometry/color, localization,
    and command-line parsing should be evaluated and prioritized by real
    projects.

## P1 - Native Interop, SDK, and Ecosystem

1. [ ] Add a package/dependency manager.
   Implement `wio add/remove/update/restore`, lockfiles, semantic version
   resolution, checksums, registries, Git/path dependencies, offline cache,
   transitive native assets, platform variants, and reproducible builds.

2. [ ] Publish official native packages.
   Start with `wio-raylib`, SDL, ImGui, SQLite, HTTP/TLS, and a C ABI helper.
   Packages should carry headers, libraries, platform metadata, Wio bindings,
   licenses, examples, smoke tests, and debug/release variants.

3. [ ] Improve the binding importer to production quality.
   Support macros/constants, callbacks, function pointers, unions, bitfields,
   opaque handles, ownership annotations, nullability, arrays, strings,
   overload naming, documentation, conditional compilation, and incremental
   regeneration without destroying manual edits.

4. [ ] Complete generic native/export support.
   Resolve the current explicit rejections for native component
   specialization and generic component/object export. Define concrete
   instantiation lists, mangling, SDK generation, and ABI stability.

5. [ ] Complete SDK dynamic field support.
   Remove the current runtime “not yet supported” paths for dynamic field
   access/assignment, broaden SDK-exportable field kinds, and test nested
   arrays, dictionaries, Option/Result, objects, components, enums, flagsets,
   any, Box, and opaque values.

6. [ ] Add callback and event-loop interop.
   Support native callbacks with captured Wio state, lifetime-safe userdata,
   thread entry, exception/panic containment, and GUI/event-loop ownership.

7. [ ] Define native resource ownership.
   Add safe wrappers/traits for handles requiring close/free/unload, move-only
   resources, borrowed handles, finalization, deterministic disposal, and
   leak diagnostics.

8. [ ] Establish ABI conformance testing.
   Compare generated layouts/calling conventions against C/C++ probes across
   compilers, architectures, optimization modes, shared/static builds, and
   SDK versions.

## P1 - CLI, Build, and Release Engineering

1. [~] Continue general CLI hardening.
   The primary command families and argument forwarding are substantially
   improved. Finish consistent exit codes, JSON/machine output, quiet/verbose
   modes, color policy, progress, cancellation, typo suggestions, help
   examples, config precedence, and error formatting across every subcommand.
   The self-hosted migration has started: every recognized tooling command
   family now enters a Wio + Argonaut-Wio companion and uses an explicit
   native fallback bridge. The complete
   `project new/describe/build/run/test/package` lifecycle, `wio run`,
   `file run/check/tokens/ast`, repository `build/test`, `dev build/test`, and
   `perf smoke`, global help/version routing, nested help, and typo suggestions
   are Wio-owned. `env` parsing, help, rendering, and non-interactive previews
   are Wio-owned, while persistent mutation and detailed platform inspection
   remain behind the native service boundary. `bind new/import` parsing,
   required-option validation, help, and diagnostics are Wio-owned while the
   header/manifest generator remains a native backend service. Release
   `package` parsing and validation are Wio-owned while distribution/archive/
   installer production remains a native backend. Finish these backend tails,
   establish pinned stage-0/generated-C++ bootstrap reproducibility, and
   remove the bridge and C++ argument parsers after parity is complete.

2. [ ] Add `wio fmt` and a canonical formatter.
   It must be syntax-aware, deterministic, idempotent, configurable only where
   necessary, safe on invalid files, and usable by editors/CI.

3. [ ] Add `wio lint`.
   Cover unused imports/symbols, shadowing, suspicious conversions, ignored
   Results, unreachable code, expensive copies, unsafe native boundaries,
   naming, deprecated APIs, and configurable warning levels.

4. [ ] Add `wio doc`.
   Generate searchable API documentation with realms, generics, constraints,
   extensions, reflection, examples, source links, package versions, and
   stable URLs.

5. [ ] Add an interactive REPL and scratch workflow.
   Reuse compiler state, support imports and multiline declarations, preserve
   history, expose generated types, and provide a safe native boundary.

6. [ ] Add incremental and parallel builds.
   Cache parsed/typed modules, generic instantiations, generated C++, native
   objects, bindings, and package resolution. Explain cache hits/misses and
   guarantee correct invalidation.

7. [ ] Make builds reproducible.
   Normalize paths/timestamps, pin toolchains/dependencies, emit build
   manifests/SBOMs, support offline verification, and compare artifact hashes
   in CI.

8. [ ] Improve test tooling.
   Add watch mode, parallel execution, sharding, retries for explicitly flaky
   tests, timeouts, fixtures, temporary directories, snapshots, coverage,
   benchmarks, JUnit/JSON output, and package-installed integration suites.

9. [ ] Add project migration and doctor commands.
   Detect obsolete manifests/std APIs, explain environment problems, verify
   native libraries/architectures, migrate versions, and produce shareable
   diagnostics.

10. [ ] Remove or formally deprecate compatibility wrappers.
    Finish moving useful PowerShell behavior into the CLI/Wio tools, announce
    deprecation windows, and test wrapper-free installation.

11. [ ] Establish release channels.
    Automate nightly/preview/stable channels, signed artifacts, checksums,
    provenance, release notes, rollback, upgrade testing, and compatibility
    verification from previous releases.

12. [ ] Add Windows, Linux, and macOS package/install matrices.
    Include multiple compilers/architectures where practical and validate
    native GUI, static/shared libraries, SDK consumers, clean uninstall, paths
    with Unicode/spaces, and non-admin installs.

## P2 - Editor and Developer Experience

1. [ ] Build a production Language Server.
   Diagnostics, completion, signature help, hover, go-to-definition, find
   references, rename, symbols, semantic tokens, inlay hints, code actions,
   formatting, workspace imports, generic constraints, and extension methods
   must share compiler logic rather than reimplement it.

2. [ ] Add debugger support.
   Source breakpoints, stepping, locals, watches, Wio stack traces, object/
   component/container visualization, panic mapping, generated-C++ source maps,
   and native mixed-mode debugging are needed.

3. [ ] Improve compiler diagnostics.
   Add stable error codes, primary/secondary spans, notes, fix-its, candidate
   ranking, generic substitution traces, import chains, lifetime explanations,
   terminal/JSON rendering, and documentation links.

4. [ ] Add package-aware editor features.
   Dependency completion, version information, source navigation, package
   docs, vulnerability/license notices, restore status, and native platform
   diagnostics should be visible in the IDE.

5. [ ] Provide project templates.
   Console app, library, native library, raylib/SDL desktop app, service, test
   package, binding package, and SDK host templates should be maintained and
   continuously smoke-tested.

6. [ ] Add refactoring support.
   Extract function/type, organize imports, change signature, move symbol,
   generate interface implementation, wrap with Option/Result handling, and
   convert free functions to extensions.

## P2 - Performance, Portability, and Security

1. [ ] Build a repeatable benchmark suite.
   Measure compiler phases, generated C++ compilation, startup, allocations,
   containers, strings, generics, reflection, JSON, native calls, and realistic
   applications. Track regressions per commit and release.

2. [ ] Reduce generated-C++ size and compile time.
   Audit template duplication, generic instantiation, headers, reflection
   metadata, source directives, unity/PCH/module strategies, and dead code.

3. [ ] Optimize runtime ownership costs.
   Measure and improve reference counting, Box/any allocations, component
   copies, string/container growth, bounds checks, temporary materialization,
   and native wrapper overhead without weakening semantics.

4. [ ] Add sanitizers and dynamic analysis.
   ASan, UBSan, TSan where applicable, leak checks, Windows diagnostics, native
   boundary stress, malformed input, and long-running runtime tests should run
   regularly.

5. [ ] Harden supply-chain and package security.
   Signed metadata/artifacts, checksum enforcement, dependency provenance,
   registry authentication, namespace ownership, malicious package isolation,
   vulnerability reporting, and reproducible verification are required.

6. [ ] Define a security model for native and untrusted code.
   Document that native code is privileged, evaluate sandboxed tooling/build
   scripts, constrain package hooks, protect credentials/environment, and
   provide safe defaults for network/filesystem operations.

7. [ ] Deepen backend portability.
   Continuously test GCC, Clang, MSVC where supported; Windows/Linux/macOS;
   x64/arm64; static/shared/PIC; endian/alignment assumptions; and alternative
   backend feasibility.

8. [ ] Evaluate an independent backend/IR only with evidence.
   Keep C++ as the production backend while measuring whether a Wio IR,
   LLVM-based path, or interpreter would materially improve diagnostics,
   compile time, tooling, or portability.

## P3 - Product Direction

1. [ ] Define Wio's primary product profile.
   Decide the supported priority among systems programming, native application
   development, games, scripting/tooling, embeddable modules, and services.
   Use that decision to control language and std scope.

2. [ ] Define long-term compatibility and deprecation mechanics in tooling.
   The written compatibility policy exists; add compiler deprecation
   attributes, warnings, migration tooling, edition/spec selection, and
   package compatibility constraints.

3. [ ] Establish governance for language and std evolution.
   Add proposal/RFC templates, acceptance criteria, feature gates,
   experimental namespaces, stabilization checklists, and removal policy.

4. [ ] Build a documentation website and searchable package portal.
   Publish versioned language/std/SDK docs, tutorials, examples, migration
   guides, package metadata, API search, and release compatibility tables.

5. [ ] Create a real-world validation portfolio.
   Maintain substantial applications covering CLI, native desktop GUI,
   static/shared library, SDK host, networking/service, data processing, and
   package consumption. Use them as release gates rather than demos only.
