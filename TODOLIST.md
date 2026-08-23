# Wio Active Backlog

This file contains only unfinished work. Completed milestones live in
`COMPLETED.md`.

Status markers:

- `[ ]` not started
- `[~]` partially implemented or implemented but not sufficiently hardened

The priorities below reflect the state of Wio at the `v0.13.0` release-candidate
freeze, including the P0/P1 language, std, native-interoperability,
structured-async, language-coherence, and SDK-parity sprints.
Completed work is recorded in `COMPLETED.md`; this file contains only the
remaining work. The
application/system and language-coherence direction is expanded in
`docs/WIO_LANGUAGE_EVOLUTION_PLAN.md`.

This backlog is intentionally broader than the `v1.0.0` release gate. Use:

- [`docs/WIO_V1_RELEASE_PLAN.md`](docs/WIO_V1_RELEASE_PLAN.md) for the finite
  pre-v1 release train and mandatory gates;
- [`docs/WIO_POST_V1_ROADMAP.md`](docs/WIO_POST_V1_ROADMAP.md) for work that is
  valuable but does not block v1;
- [`COMPLETED.md`](COMPLETED.md) for accepted work and its evidence.

Some numbered items contain both a pre-v1 correctness core and a post-v1
expansion. The release plan routes those parts explicitly; an unchecked item
here does not by itself mean that v1 cannot ship.

## P1 - Language Semantics and Type System

1. [~] Turn the draft into a versioned normative language specification.
   Versioned 0.8–0.11 delta specifications now freeze lifetime, generics,
   native components, attributes, matching, and applications. Publish formal
   lexical grammar, syntax grammar, name resolution, overload
   resolution, type compatibility, ownership/reference rules, evaluation
   order, initialization, destruction, generics, diagnostics, and feature
   status. Version the specification alongside releases.

3. [~] Complete generic defaults, partial specialization, and specialization
   ordering. Default type parameters, dependent trailing defaults, exact and
   partial specialization ordering, ambiguity diagnostics, cross-module
   visibility, and defaulted native instantiation are implemented. Default
   `string`/`text` const parameters and defaults now work on every generic
   declaration category with specialization and module visibility. Additional
   value kinds and generic component/object export remain under their
   respective language and native/export items.

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

7. [~] Strengthen pattern matching.
   Option/Result and exact-length array destructuring, guards, algebraic
   exhaustiveness, duplicate/unreachable diagnostics, and typed local bindings
   are implemented. Object bindings preserve shared identity, component
   bindings copy values, duplicate names are rejected, and guarded cases after
   an equivalent unguarded case are unreachable. Add payload enums and
   component/rest/nested patterns.

8. [~] Complete component extension ergonomics.
   Direct native component extensions now map free C++ functions through
   `view`/`ref` receiver reference-or-pointer dispatch. Add constrained generic
   extensions, extension properties, static extension functions, explicit
   conflict resolution, import-scoped visibility, documentation generation,
   reflection metadata, and editor completion.

12. [~] Replace the legacy annotation surface with typed, user-extensible
    attributes before expanding application/system hosting. The accepted
    canonical spelling is declaration-leading `[Attribute]`, with stacked or
    grouped lists. Existing `using cpp::header(...)`-style lexical activation
    remains canonical; `use` stays reserved for module imports. Postfix `with`
    and `@Attribute(...)` are legacy migration input,
    not the final surface. Typed arguments/defaults, target policies, retention,
    repetition, inheritance, conflicts, named arguments, folded scalar/string/
    text defaults, and runtime type/field reflection are partially implemented.
    Unify built-ins and user attributes in one typed model; then add controlled
    validation/derive processors, formatter/LSP/docs support, SDK metadata, and
    automated edition-aware migration. Add bounded, typed
    behavioral attributes for entry guards, pre/postconditions, guaranteed
    exit hooks, and eventually `around` interception. This includes
    user-defined receiver-liveness guards for callbacks whose native peer may
    have been destroyed while the Wio wrapper remains alive. Effects must be
    type-checked, ordered explicitly, visible to tooling, and preserve
    signatures, evaluation order, thread/cancellation semantics, and ABI.
    Begin with guards/contracts; do not expose unrestricted token/AST or
    arbitrary call-site mutation. Keep the grammar small: safe defaults should
    replace most policy keywords, uncommon policies should be namespaced
    meta-attributes, and behavioral processors should be ordinary typed
    functions/interfaces. Typed applications already fold scalar, `string`,
    and `text` const references and materialize trailing defaults into runtime-
    reflection metadata. The normative delivery plan is
    `docs/WIO_ATTRIBUTE_SYSTEM_PLAN.md`. Pre, post, finally, and around
    processors are separate interfaces. User attributes may compose existing
    attributes, with normalized target/require/require-any/conflict/only-with/
    exclusivity/cardinality/order constraints. Publish every compiler-known
    attribute, including native/export and their limits, as declarations in
    `std::attribute`; built-ins must not retain a separate hidden model.

13. [~] Complete a language-coherence pass before broad surface expansion.
    The first 0.11 stabilization slice aligned scoped attributes, pipelines,
    pattern bindings/reachability, and application lifecycle invariants across
    compiler, tests, and specification. Continue reconciling constraints,
    specialization, conversion, extension, native mapping, initialization,
    destruction, and diagnostics; remove stale or contradictory feature-status
    claims.

14. [~] Implement the accepted application/system lifecycle model after the
    attribute foundation. The current proposal uses one stack-resident
    `application` root, component-like `system` state, inline or function-bound
    `on` handlers, explicit resources, deterministic schedules, fixed stages,
    reverse shutdown, main-thread affinity, and headless testing. Keep `after`
    as a schedule dependency; keep `every`, `during`, and `wait` in runtime/
    async experiments until their semantics are proven. Sequential application
    lifecycle, stack-resident systems, orderly exit, and reverse shutdown are
    implemented. Add resources, explicit/fixed schedules, headless contexts,
    then parallel execution after `view`/`ref` conflict analysis is stable.

15. [~] Complete the first-class Unicode-semantic `text` model.
    `text`, validated UTF-8 `u"..."` and `u$"..."` literals, code-point count
    and slicing/indexing, grapheme counting/slicing, display width, case fold,
    byte count, concatenation, equality/ordering/hashing, pattern matching,
    generics, console output, and explicit `std::unicode` UTF-8 conversion now
    work end to end. `CodePoints()` and `Graphemes()` expose ordinary iterable
    arrays for `for` loops. Fallible UTF-8/UTF-16/UTF-32 decoding and
    platform-independent transcoding are available through `std::unicode`.
    Native static exports accepting and returning `text` are covered across the
    generated C++ boundary, and runtime reflection reports `text` as a named
    primitive with stable size/alignment metadata. SDK ABI v8 now publishes a
    dedicated text descriptor and shared modules round-trip text fields through
    typed/dynamic host access. NFC, NFD, NFKC, and NFKD normalization are now
    implemented with pinned Unicode 17 data and focused conformance vectors.
    Complete the broader Unicode category/case tables, locale-sensitive
    behavior, and the full upstream conformance corpus. Do not expose C++
    `wchar_t` width as a Wio language rule.

17. [~] Extend compile-time constants to `string` and `text`.
    Literal initialization, references to other constants, concatenation,
    comparison, matching, and global/local/component-static storage now work for
    both types. Interpolated strings are accepted when every embedded expression
    is also constant-evaluable; calls and runtime bindings remain rejected.
    Typed attribute arguments/defaults now consume folded constant values;
    functions, aliases, interfaces, components, and objects accept `string`
    and `text` const generic arguments/defaults, specialization patterns, and
    qualified module constants. Runtime reflection now exposes primary source
    parameter names and concrete type/const arguments for ordinary, exact, and
    partial generic types. Const evaluation rejects dependency cycles and
    enforces depth, node-count, and folded-text budgets. SDK ABI v7 retains
    canonical concrete generic argument identities. Complete storage
    interning, cross-module constant export, and the final
    Unicode normalization policy.

## P1 - Standard Library Correctness and Consistency

1. [~] Build a real Unicode text model.
   UTF-8 validation, codepoints/runes, grapheme clustering, display width,
   basic case folding, codepoint/byte conversion, safe slicing, and builders
   now exist. The first-class Unicode-semantic `text` value and validated
   `u"..."`/`u$"..."` literals now provide ordinary string-like ergonomics,
   code-point indexing/slicing, grapheme operations, matching, hashable generic
   container use, code-point/grapheme iteration, console output, and explicit UTF-8
   UTF-8/UTF-16/UTF-32 conversion. The compiler/runtime owns validation and
   normalization policy, iteration, and native transcoding; it does not expose
   the platform-dependent C++ `wchar_t`/`std::wstring` representation as Wio
   semantics. Unicode 17 NFC/NFD/NFKC/NFKD normalization and focused canonical/
   compatibility conformance vectors are implemented. Complete full Unicode
   category/case data, locale-sensitive behavior, and the full upstream
   conformance corpus. GUI input must not require a native `AppendCharacter`
   workaround.

3. [~] Harden JSON into a production-grade module.
   Parsing/writing, nested values, errors, pretty output, exact numeric-token
   preservation, typed integer accessors, configurable duplicate-key policy,
   deterministic key ordering, depth/size limits, UTF validation, JSON Pointer,
   and Merge Patch are implemented. Typed `serialization::Codec<TValue,
   TWire>` adapters now provide checked JSON encode/decode composition. Add a
   streaming parser/writer, RFC 6902 Patch, schema hooks, and declarative codec
   derivation/registration.

4. [~] Add serialization beyond JSON.
   Versioned bounded binary frames, endian/varint readers and writers,
   Base64/hex, CSV, INI, and typed checked codecs exist. Add declarative codec
   derivation/registration, streaming codecs, unknown-field/migration policy,
   TOML, and enum policy.

5. [~] Harden time, random, hash, log, numeric, encoding, stream, UUID, and
    SemVer. These modules exist; add cross-platform vectors, deterministic
   contracts and cryptographic/non-cryptographic distinctions. Frozen vectors
   now cover FNV-1a, SHA-256, MT19937, xoroshiro128+, LXM, Wichmann-Hill,
   numeric parsing, and platform-independent UTC civil conversion. Secure random,
   UUID variants, full SemVer metadata/precedence, host time formatting/offsets,
   and structured console/file log sinks are implemented.
   Add time-zone database/calendar types, floating-point edge matrices,
   streaming encoders, and broader cross-platform vectors.

6. [~] Add regular-expression safety and completeness.
   Engine behavior, escaping, captures, replacement, iteration, and the error
   model are documented and implemented with conservative pattern/input safety
   limits. Bounded match/capture records with byte offsets and conservative
   rejection of backreferences, lookarounds, nested repeats, and repeated
   alternation are implemented. Add Unicode mode documentation and a backend
   capable of enforceable execution timeouts.

7. [~] Add networking foundations.
   DNS, URI, owned sockets, TCP/UDP, timeout, endpoint, and loopback behavior
   exist. Wio 0.12 adds bounded-executor DNS/connect, leased TCP/UDP
   async data I/O, ownership-safe async accept, close-vs-operation lifetime
   safety, and live-handle diagnostics. Add native completion-port backends,
   TLS,
   HTTP client/server primitives, headers,
    multipart, WebSocket, cancellation, timeouts, proxies, and certificate
    validation are required before `std::http` can be considered.

8. [~] Add concurrency foundations.
   Threads, recursive mutexes, condition variables, atomics, channels,
   blocking channels, Promise/Future, TaskGroup, cancellation, sleep, and yield
   share one host model. Language-level async functions/methods, `await`,
   `coroutine<T>`, async `Entry`, a C++20 worker/timer runtime, task status,
   timeout/cancellation, worker offload, All/Any/Race, recoverable timeout,
   explicit dispatch, and structured async task groups are frozen for 0.11.
   The post-0.11 direction is frozen as a design plan in
   `docs/WIO_ASYNC_EVOLUTION_PLAN.md`: first add compiler-checked cross-thread
   safety, a separate blocking pool, cancellation-aware timers, and explicit
   shutdown; then add the `Task<T>` facade, `Poll`/`Block`, structured
   `spawn`/scope/deadlines, main-executor integration, true async I/O, and only
   afterward streams. The correctness, task ergonomics, structured scope, and
   application main-executor slices shipped in Wio 0.12. Structured
   scopes now also support compiler-checked `spawn worker` and `spawn blocking`
   without changing task ownership or result types. The
   platform slice now has a dedicated bounded I/O executor and Result-safe
   async filesystem/process run/capture operations and leased DNS/connect plus
   TCP/UDP data I/O plus pre-leased async accept; owned processes now provide
   separate async stdin/stdout/stderr, wait, terminate, close/reap, and native
   lease safety. Native completion-port backends, process signal/event
   subscription, native watcher backends,
   stream/generator syntax, and full cross-platform qualification remain. A
   bounded `AsyncChannel<T>` now supplies thread-free producer
   backpressure and close/drain semantics. A cancellable,
   debounced portable file watcher is available as the portable fallback.
   Keep the everyday vocabulary small and prove each slice in console,
   desktop, game, server/tool, and native-host scenarios.

9. [ ] Add OS/application facilities.
    Environment variables, process signal/event subscription, filesystem watching,
    clipboard, notifications, dialogs, user/config/cache directories, dynamic
    libraries, and platform capability queries. Basic OS/architecture,
    pointer-width, endian, hardware-thread, path-list separator, and native
    newline queries now exist in `std::platform`.

10. [~] Add data and utility modules.
    Host date/time formatting, bigint, RLE compression, MIME, INI, statistics,
    geometry/color, localization, and CLI parsing exist. Add decimal, archive,
    TOML, database primitives, and real-project qualification.

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

5. [~] Bring the host SDK to full Wio 1.0 parity and version it with Wio.
   Before the Wio 1.0 release, every host-observable stable language/runtime
   feature must have a documented C ABI descriptor and an ergonomic C++ SDK
   representation or an explicit diagnostic explaining why it cannot cross
   the host boundary. Complete dynamic field access/assignment, `ref`/`view`
   lifetime-safe exports, nested containers, tuples, spans, Option/Result,
   objects, components, interfaces, enums, flagsets, any, Box, opaque values,
   callbacks/events, async tasks/cancellation, attributes/reflection, and
   concrete generic/const-generic instantiations. Starting with the next Wio
   release, publish the SDK at the same product version as the compiler,
   runtime, std, CLI, and VS Code extension. Keep the low-level ABI revision
   independently feature-negotiated so compatible hosts can fail cleanly.
   Track the staged contract in `docs/WIO_SDK_EVOLUTION_PLAN.md`.

   The 0.14 value-parity baseline is complete: product version 0.14.0, ABI
   descriptor v8, descriptor-size/capability negotiation, stable type IDs,
   concrete generic/const-generic arguments, Unicode text, Option, Result/unit,
   tuples, nested arrays/maps, queues/sets, checked spans, and owned byte buffers
   cross the typed/dynamic host boundary. A machine-readable support inventory
   distinguishes supported, metadata-only, and rejected shapes. Remaining work
   is lifetime-safe `ref`/`view`, interface/Box/any/resource adapters, callbacks,
   async/application hosting, retained attribute metadata, and the independent
   platform ABI matrix.

   As part of that parity work, complete the current dynamic field surface.
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

12. [~] Add Windows, Linux, and macOS package/install matrices.
    Windows and Ubuntu release/package/install gates run in CI. Add macOS and
    multiple compilers/architectures where practical; further validate
    native GUI, static/shared libraries, SDK consumers, clean uninstall, paths
    with Unicode/spaces, and non-admin installs.

## P2 - Editor and Developer Experience

The shared architecture and pre-v1 delivery gates for VS Code, Visual Studio,
Rider, and CLion are defined in
[`docs/WIO_EDITOR_ECOSYSTEM_PLAN.md`](docs/WIO_EDITOR_ECOSYSTEM_PLAN.md).

1. [ ] Build a production Language Server.
   Diagnostics, completion, signature help, hover, go-to-definition, find
   references, rename, symbols, semantic tokens, inlay hints, code actions,
   formatting, workspace imports, generic constraints, and extension methods
   must share compiler logic rather than reimplement it. The version-aligned
   `wio-vscode 0.12.0` extension now provides a tested editor-side baseline
   for diagnostics, completion, navigation, symbols, commands, grammar, and
   snippets; replace its lightweight source index with this compiler-owned
   language service instead of growing a second semantic implementation.

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

7. [ ] Ship version-aligned Visual Studio and JetBrains clients before v1.
   Build `wio-vs` for Visual Studio and one shared IntelliJ Platform plugin for
   Rider and CLion. Both must use the compiler-owned language service, share
   conformance fixtures with `wio-vscode`, and pass packaged install, upgrade,
   compatibility, and daily command-flow gates by `v0.18.0`.

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

4. [~] Add sanitizers and dynamic analysis.
   Ubuntu CI runs the async native runtime stress test under ASan/UBSan and the
   compiler corpus under libFuzzer/ASan/UBSan. Add TSan where applicable, leak
   checks, Windows diagnostics, broader native-boundary stress, and long-running
   soak tests.

5. [ ] Harden supply-chain and package security.
   Signed metadata/artifacts, checksum enforcement, dependency provenance,
   registry authentication, namespace ownership, malicious package isolation,
   vulnerability reporting, and reproducible verification are required.

6. [ ] Define a security model for native and untrusted code.
   Document that native code is privileged, evaluate sandboxed tooling/build
   scripts, constrain package hooks, protect credentials/environment, and
   provide safe defaults for network/filesystem operations.

7. [~] Deepen backend portability.
   Release CI covers Windows/MSVC plus MinGW backend and Ubuntu/GCC, with Clang
   sanitizer builds on Ubuntu. Add macOS, x64/arm64 coverage, static/shared/PIC
   qualification, endian/alignment audits, and alternative-backend evidence.

8. [ ] Evaluate an independent backend/IR only with evidence.
   Keep C++ as the production backend while measuring whether a Wio IR,
   LLVM-based path, or interpreter would materially improve diagnostics,
   compile time, tooling, or portability.

## P3 - Product Direction

1. [~] Define and validate Wio's primary product profile. The current proposal
   is a native application, tooling, and game language with a C++ ecosystem
   bridge and data-oriented component model, followed by embeddable modules
   and services where the same model fits. Freeze this direction through the
   application/system decision and validate it with release-gate projects.

2. [ ] Define long-term compatibility and deprecation mechanics in tooling.
   The written compatibility policy exists; add compiler deprecation
   attributes, warnings, migration tooling, edition/spec selection, and
   package compatibility constraints.

3. [ ] Establish governance for language and std evolution.
   Add proposal/RFC templates, acceptance criteria, feature gates,
   experimental namespaces, stabilization checklists, and removal policy.

4. [~] Complete the documentation website and searchable package portal.
   The `wio-web` React/Vite documentation and download site already consumes
   canonical Markdown from this repository. Add versioned language/std/SDK
   selection, tutorials, migration guides, release compatibility tables,
   full-text/API search, and package metadata/search without duplicating the
   canonical technical content.

5. [ ] Create a real-world validation portfolio.
   Maintain substantial applications covering CLI, native desktop GUI,
   static/shared library, SDK host, networking/service, data processing, and
   package consumption. Use them as release gates rather than demos only.
