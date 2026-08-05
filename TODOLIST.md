# Wio Active Backlog

This file contains only unfinished work. Completed milestones live in
`COMPLETED.md`.

Status markers:

- `[ ]` not started
- `[~]` partially implemented or implemented but not sufficiently hardened

The priorities below reflect the state of Wio after `v0.5.1`, the P0
correctness sprint, and the P1-A language/std correctness sprint. Completed
work is recorded in `COMPLETED.md`; this file contains only the remaining P1
and later work.

## P1 - Language Semantics and Type System

1. [~] Turn the draft into a versioned normative language specification.
   Publish formal lexical grammar, syntax grammar, name resolution, overload
   resolution, type compatibility, ownership/reference rules, evaluation
   order, initialization, destruction, generics, diagnostics, and feature
   status. Version the specification alongside releases.

3. [~] Complete generic defaults, partial specialization, and specialization
   ordering. Default type parameters, dependent trailing defaults, exact and
   partial specialization ordering, ambiguity diagnostics, cross-module
   visibility, and defaulted native instantiation are implemented. Default
   value parameters depend on the ordinary const-generic work below; generic
   Generic component/object export remains under the native/export item.

5. [~] Expand variadic and compile-time metaprogramming.
   `AllSame`, `IndexOf`, and `UniqueCount` now complement the existing pack
   count/index/storage helpers. Add `Take`, `Drop`, `Zip`, `MapTypes`,
   filtering/folding, value transforms, pack concatenation, richer pack
   constraints, and usable compile-time iteration.

6. [~] Add modern generic constraint syntax and associated types.
   Readable `where` clauses now lower to the existing predicate model, generic
   defaults participate in inference, and the 0.9 specification freezes
   generic compatibility as invariant. Same-slot predicate conjunction uses
   `where T: TraitA + TraitB`. Add richer boolean constraint composition,
   associated types, default implementations, explicit variance features, and
   further generic failure diagnostics.

7. [ ] Strengthen pattern matching.
   Add Option/Result destructuring, enum payloads, component/array patterns,
   guards, exhaustiveness checks, unreachable-case diagnostics, and binding
   ownership/reference rules.

8. [~] Complete component extension ergonomics.
   Direct native component extensions now map free C++ functions through
   `view`/`ref` receiver reference-or-pointer dispatch. Add constrained generic
   extensions, extension properties, static extension functions, explicit
   conflict resolution, import-scoped visibility, documentation generation,
   reflection metadata, and editor completion.

11. [ ] Evaluate pipeline/data-flow operators only after semantics are stable.
    Measure `|>` and `<|` against ordinary calls, method chaining, error
    propagation, inference, and debugging before reserving syntax.

12. [ ] Clarify whether `system`, `program`, `every`, `after`, `during`, and
    `wait` belong to the core language, a standard DSL, or should be removed
    from the planned surface.

## P1 - Standard Library Correctness and Consistency

1. [ ] Build a real Unicode text model.
   Define UTF-8 validation, codepoints/runes, grapheme clusters, safe slicing,
   Unicode case folding, normalization, categories, width, iteration, and
   conversion. GUI text input must not require a native
   `AppendCharacter` workaround.

2. [ ] Add byte/codepoint/string builders.
   Provide allocation-conscious `StringBuilder`, byte writer/reader,
   codepoint append, formatting sinks, reusable buffers, and clear ownership
   rules.

3. [~] Harden JSON into a production-grade module.
   Parsing/writing, nested values, errors, and pretty output exist. Add exact
   integer preservation, configurable duplicate-key policy, deterministic key
   ordering, streaming parser/writer, depth/size limits, UTF validation,
   JSON Pointer/Patch, schema hooks, and generic encode/decode traits.

4. [ ] Add serialization beyond JSON.
   Provide stable generic serialization traits and at least binary,
   Base64/hex integration, CSV, and configuration-friendly formats. Define
   versioning, unknown fields, migration, and enum policy.

5. [~] Harden time, random, hash, log, numeric, encoding, stream, UUID, and
    SemVer. These modules exist; add cross-platform vectors, deterministic
    contracts, cryptographic/non-cryptographic distinctions, secure random,
    time zones/calendars, structured log sinks, floating-point edge matrices,
    streaming encoders, UUID parsing/variants, and full SemVer
    prerelease/build metadata.

6. [ ] Add regular-expression safety and completeness.
    Document the engine, escaping, Unicode behavior, capture APIs, replacement,
    iteration, catastrophic-backtracking limits/timeouts, and error model.

7. [ ] Add networking foundations.
    DNS, sockets, TCP/UDP, TLS, HTTP client/server primitives, URI, headers,
    multipart, WebSocket, cancellation, timeouts, proxies, and certificate
    validation are required before `std::http` can be considered.

8. [ ] Add concurrency foundations.
    Threads, mutexes, atomics, channels, task scheduling, futures, async I/O,
    cancellation, structured concurrency, thread-local storage, and runtime/
    host integration need one coherent model.

9. [ ] Add OS/application facilities.
    Environment variables, process pipes, signals, filesystem watching,
    clipboard, notifications, dialogs, user/config/cache directories, dynamic
    libraries, and platform capability queries. Basic OS/architecture,
    pointer-width, endian, hardware-thread, path-list separator, and native
    newline queries now exist in `std::platform`.

10. [ ] Add data and utility modules.
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

4. [~] Complete generic native/export support.
   Native generic POD components, type/value C++ template aliases, and native
   component specialization now follow the versioned 0.10 ABI contract.
   Generic component/object export, concrete SDK instantiation tables, and
   cross-toolchain ABI conformance remain.

5. [ ] Complete SDK dynamic field support.
   Remove the current runtime “not yet supported” paths for dynamic field
   access/assignment, broaden SDK-exportable field kinds, and test nested
   arrays, dictionaries, Option/Result, objects, components, enums, flagsets,
   any, Box, and opaque values.

6. [ ] Add callback and event-loop interop.
   Support native callbacks with captured Wio state, lifetime-safe userdata,
   thread entry, exception/panic containment, and GUI/event-loop ownership.

7. [~] Extend native resource ownership beyond the shipped common model.
   `std::resource::Owned<T>` / `Borrowed<T>` now provide deterministic,
   idempotent disposal, final-owner cleanup, release, and live-resource
   diagnostics. Add language-level move-only ownership, proven borrowed-handle
   lifetimes, closer traits/annotations generated by the binding importer, and
   cycle-aware leak diagnostics.

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
   The self-hosted migration is complete: every recognized tooling command
   family enters a Wio + Argonaut-Wio companion. The complete
   `project new/describe/build/run/test/package` lifecycle, `wio run`,
   `file run/check/tokens/ast`, repository `build/test`, `dev build/test`, and
   `perf smoke`, global help/version routing, nested help, and typo suggestions
   are Wio-owned. The complete `env print/setup/status/remove/doctor` family,
   including persistent user environment/PATH management and backend smoke,
   is Wio-owned. `bind new/import` parsing, validation, diagnostics, JSON
   manifest generation, and C/C++ header importing are Wio-owned. Release
   `package` parsing, validation, distribution staging, portable backend copy,
   metadata, archive, and installer production are Wio-owned. The generic
   native fallback and old C++ project/env/file/perf CLI implementations have
   been removed, along with every private compiler-service bridge and C++
   tooling argument parser. Establish pinned stage-0/generated-C++ bootstrap
   reproducibility and continue the cross-cutting hardening above.

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
