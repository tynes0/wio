# Wio Active Backlog

This file contains only unfinished work. Completed milestones live in
`COMPLETED.md`.

The current consolidated defect/limitation/qualification register is
[`docs/WIO_KNOWN_ISSUES.md`](docs/WIO_KNOWN_ISSUES.md). It distinguishes observed
bugs from pending features and missing test evidence; it is not an error-code
catalog or a replacement for this backlog.

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
   Design note (not an approved syntax change): consider making the extension
   group name optional and using `extension for Type { ... }` as the concise
   form. The current name is an internal naming input, not a callable namespace.
   If adopted, derive deterministic internal identities from module, target
   type, and method signature; preserve named syntax for compatibility and the
   existing collision rules. Revisit names if selective imports or explicit
   group-based conflict resolution give them a user-facing purpose. No compiler
   or syntax change is scheduled by this note.

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
    implemented. Wio 0.16 adds explicit/fixed schedules, explicit scheduled
    `ref`/`view` resource injection, monotonic delta contexts, host-driven
    headless execution, deterministic partial-start rollback, and fixed-step
    acceptance. Add richer typed frame/start/close contexts and parallel
    execution only after `view`/`ref` conflict analysis is stable.
    The v0.17 candidate replaces the canonical member DSL with ordinary fields
    and functions: conventional or `[Start]`/`[Update]`/`[Close]` lifecycle
    binding, `[Fixed]`/`[After]`/`[Main]` stages, inferred source-module system
    fields, and normal helper methods now lower to the existing stable runner.
    Legacy `on`/`resource`/member-`system`/explicit-schedule syntax remains
    compatible. Finish imported-system identity through semantic/module
    metadata, typed resource access without legacy run clauses, scheduler
    inspection/reflection, and conflict-proven `[Worker]` execution.

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

18. [ ] Retire legacy attribute attachment syntax before v1.
    Keep declaration-leading `[Attribute]` as the sole attachment spelling;
    remove compiler compatibility for `@Attribute(...)` and postfix attribute
    `with ...` after source migration. Preserve scoped `using`, including
    `using cpp::header(...)`, typed composition/processors and reflection.
    Built-in canonical names and their existing short aliases remain supported
    inside brackets (for example Native/native and CppName/cpp::name). Alias
    normalization is not case-insensitive user-attribute lookup.
    Migrate std/CLI/SDK, tests, examples, generators, formatter and maintained
    IDE/docs surfaces; reject obsolete spellings with actionable diagnostics.
    This is an accepted v1 blocker, not implemented by this planning note.
    Scope and acceptance: `docs/WIO_V1_SURFACE_CONSOLIDATION_PLAN.md`, section 2.

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
    multipart, WebSocket, per-operation deadlines, proxies, and certificate
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
   Wio 0.16 propagates cancellation through directly awaited children, adds
   recoverable cancellation combinators, exposes typed scalar async tasks to
   C++ hosts, and adds cancellation-token overloads for filesystem, process,
   DNS, TCP, listener, and UDP operations. Polling, waiting, completion target,
   deadlines, main pumping, stale binding, and orderly shutdown are covered by
   the cross-platform acceptance matrix.
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

11. [~] Complete the Result-first std standardization, cleanup and performance
    pass before v1. Standard checked retrieval/fallible operations return
    `Result<T>` with standardized missing-value/key/index, invalid-value and
    other error contracts; ordinary calls, `!()` and `?()` share one model.
    Keep infallible APIs direct. Keep `Option<T>` only as an explicitly chosen
    user optional-value utility, not an automatic standard-operation return.
    Audit intrinsic methods and every std module, async payloads, both WIR
    backends, SDK descriptors, CLI consumers and examples together. Remove
    the `Try...` API family, redundant `...Result`/`...Raw` aliases, obsolete helpers/modules
    and duplicated implementations without deleting distinct capabilities.
    Standardize naming, overloads, ownership, Unicode/bounds/error behavior;
    profile and optimize allocations/copies, algorithms, containers, builders
    and byte/buffer I/O with reproducible native/VM performance gates.
    Publish migration diagnostics/docs and keep IDE/SDK interfaces aligned.
    Std cleanup also has nine accepted source-style/access requirements:
    multiline function/method bodies; one blank line after each block closing
    brace; private object defaults with explicit public/private signatures and
    members; separate-line grouped/stacked bracket attributes; width-based
    parameter wrapping; no Try API family and one Result-based fallible model;
    one blank line between all declarations, including native ones; braced
    control-flow bodies except a single-line `if (...) return value;`; and a
    space before control-flow parentheses. The initial proposed wrap width is
    120 columns, not yet frozen. These are std conventions, not user-language
    restrictions; source migration and syntax-aware formatter checks must agree.
    This is an accepted v1 blocker; final integrated optimization still follows.
    Scope and acceptance: `docs/WIO_V1_SURFACE_CONSOLIDATION_PLAN.md`, sections 1/3.
    On 2026-10-15 this work was integrated module by module into the first heavy
    generated-C++/runtime optimization pass. Builders and the binary consumer
    have the first Result/style migration and native/VM append-capacity slice;
    the rest of std, builtin checked retrieval and broad migration/tooling remain
    open. See `docs/WIO_STD_BUILDERS_CONSOLIDATION.md`.

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
   Scope decision (2026-10-15): the SDK is a separately delivered developer kit
   with its own product acceptance matrix. Full kit parity is a v1 blocker, not
   a prerequisite for closing the compiler/VM/backend milestone. Keep the kit
   version aligned with Wio; this is a scope separation, not a second ABI.
   Backend closure still owns the shared low-level ABI, native bridge, ownership,
   exception/thread boundaries, artifact production and their qualified runtime
   integration. Do not reopen that milestone for an unimplemented SDK convenience
   API, host-value adapter or reflection accessor.

   Outstanding kit work includes the VM adapter's complex/nested values,
   generic component/object/interface exports, safe host ref/view and callback
   surfaces, complex/nested async results and foreign task creation/transfer,
   application hosting, detailed reflection/accessors and reload/capability
   coverage. Those remain mandatory under this v1 SDK gate, including kit-specific
   conformance on both backends; existing scalar/session/task adapters are only
   the qualified foundation. Track them here rather than as backend packaging work.

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
   distinguishes supported, metadata-only, and rejected shapes. ABI v9 then
   added retained typed-attribute metadata. ABI v10 adds host-driven
   application state, typed scalar async tasks, retained native callbacks, and
   move-only native-resource descriptors. Remaining work is lifetime-safe
   `ref`/`view` export expansion, interface/Box/any adapters, general complex
   async results, concrete exported generic objects/components, and the
   independent platform ABI matrix.

   As part of that parity work, complete the current dynamic field surface.
   Remove the current runtime “not yet supported” paths for dynamic field
   access/assignment, broaden SDK-exportable field kinds, and test nested
   arrays, dictionaries, Option/Result, objects, components, enums, flagsets,
   any, Box, and opaque values.

6. [~] Add callback and event-loop interop.
   Wio 0.16 defines typed `WioHostCallback` descriptors with balanced retained
   userdata, declared foreign-thread entry, exception containment, and explicit
   main-executor pumping. Add generated adapters for native C callback
   signatures that capture Wio state, binding-importer ownership annotations,
   and maintained GUI/event-loop packages.

7. [~] Extend native resource ownership beyond the shipped common model.
   `std::resource::Owned<T>` / `Borrowed<T>` now provide deterministic,
   idempotent disposal, final-owner cleanup, release, and live-resource
   diagnostics. The 0.16 ABI adds `WioOwnedNativeResource`, borrowed views, and
   the move-only C++ `UniqueNativeResource` wrapper with exactly-once transfer.
   Add language-level move-only ownership, proven borrowed-handle lifetimes,
   closer traits/annotations generated by the binding importer, and cycle-aware
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
   compatibility, and daily command-flow gates before v1. Do not defer basic
   editor readability until the full language service is complete: immediately
   after the dual-backend runtime/bridge core closes, ship a Rider-first
   `wio-jetbrains` preview with `.wio` recognition, lexical syntax coloring,
   comments/strings/numbers/attributes, braces, folding, indentation, manifest
   discovery, and build/run/check/test commands. Reuse the same plugin core for
   CLion, then deliver the Visual Studio preview. Replace lexical fallbacks with
   compiler semantic tokens and language-service results as those stabilize.

8. [ ] Unify compiler, tooling, and runtime diagnostics after the C++ backend,
   Unicode, VM, and SDK milestones are complete. Introduce one structured
   diagnostic model with stable domain codes, severity, primary/secondary
   source spans, notes, help, and machine-applicable fix-its. Migrate every
   user-facing legacy error and warning away from free-form logger calls while
   keeping operational logging separate. Generate the complete explanatory
   Markdown/web reference and editor-facing JSON catalog from the same source,
   add `wio explain CODE` plus human/short/JSON/SARIF renderers, preserve native
   compiler diagnostics through a Wio boundary record, and make CI reject
   duplicate, undocumented, or unregistered codes.

## P2 - Performance, Portability, and Security

1. [ ] Build a repeatable benchmark suite.
   Measure compiler phases, generated C++ compilation, startup, allocations,
   containers, strings, generics, reflection, JSON, native calls, and realistic
   applications. Track regressions per commit and release.

2. [ ] Reduce generated-C++ size and compile time.
   This is now a pre-v1 backend closure gate, not an optional later
   optimization. Move emitted runtime/helper templates into precompiled runtime
   artifacts, make includes and metadata capability-driven, precompile stable
   std/runtime packages, and partition Lowered WIR into cacheable C++
   translation-unit shards instead of blindly mirroring source files. Add a
   runtime PCH only after include minimization, and measure PCH, unity, and
   sharded modes rather than assuming one policy fits every project. Audit
   template duplication, reachable generic instantiation, reflection/export
   metadata, source directives, native object caching, and dead code.

3. [ ] Optimize runtime ownership costs.
   Measure and improve reference counting, Box/any allocations, component
   copies, string/container growth, bounds checks, temporary materialization,
   and native wrapper overhead without weakening semantics. Scalarize ordinary
   local/field/index places so they do not allocate `shared_ptr<optional<T>>`
   or carry `std::function` readers/writers, separate source `Option<T>` and
   nullable values from compiler initialization state, use niche layouts where
   legal, and emit concrete closure/callback types instead of type erasure on
   hot paths.

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

8. [x] Complete the backend-neutral WIR and independent backend functional path.
   Functional closure recorded 2026-10-15 within the supported/qualified shape
   boundaries below. Project CLI, declared native distribution policy and final
   platform/artifact qualification are complete: four new gates passed on clean
   Windows Debug, Linux Release and macOS Release builds (12/12 executions).
   NI-011 Unicode native output paths are fixed. This does not requalify the full
   older corpus or every CPU/configuration. Heavy optimization is item 9/the
   three accepted passes; full SDK kit parity remains Native/SDK item 5 before v1.
   See `docs/WIO_BACKEND_CLOSURE_QUALIFICATION.md` for pinned CI evidence.
   Typed WIR, canonical Lowered WIR, stable IDs/source spans, control flow,
   values, places/borrows, arrays, matching, component/object construction,
   lexical cleanup, receiver-aware methods, deterministic object/interface
   slots, direct/virtual/interface dispatch, object casts/type tests, and
   identity equality are implemented with verifier and printer coverage.
   Callable identity is also frozen: exact overload targets, concrete generic
   specialization keys, named function values, ordered closure environments,
   value/reference/retained-self captures, indirect calls, and extension calls
   survive verified Typed-to-Lowered WIR unchanged. Dictionary literals and
   keyed places, string/text interpolation and indexing, enum/flagset values,
   backend-neutral intrinsics, `any`, nullable wrapping, and the nominal
   Option/Result/Tuple/Span value models now survive the same verified path.
   Ownership and cleanup are backend-neutral as well: every type records
   trivial/owned-value/reference-counted/borrowed semantics, managed loads are
   explicit borrows, copy/move/replace/release/drop are verified operations,
   return moves and reverse lexical cleanup cover structured exits, and
   Lowered WIR separates intrusive retain/release from value copy/drop glue.
   Native interop is now backend-neutral too: native POD C++ identity,
   `opaque`, ref/view mutability, callbacks, exception boundaries, stable
   symbols, generic specialization keys, adapter strategy, `native-call` /
   `native-invoke`, deterministic thunk planning, and the C-shaped SDK handle
   contract survive verified Typed-to-Lowered WIR. Concrete C++ thunk bodies
   and the qualified VM native bridge are complete within the boundaries below.
   Module/import/export identity, concrete generic export signatures,
   detailed field/method/attribute reflection descriptors,
   application/system schedule and resource-access metadata,
   lifecycle/state-transfer hooks, and deterministic
   SDK call-table slots also survive through one verified canonical contract.
   The compatible SDK sidecar and generated sidecar emission are frozen.
   The independent C++ backend now consumes verified Lowered WIR by default,
   emits stable CFG/place/type/function identities, and passes host-compiler
   plus executable CLI gates without AST fallback. The former generator remains
   available only as a one-release compatibility oracle. Exact enum/flagset
   layouts and intrinsics plus canonical Option/Result variants, checked unwrap,
   and propagation now emit from concrete Lowered WIR; open generic declaration
   metadata is excluded from concrete output. Container/string/text intrinsics
   and range/array/dictionary iterators now execute through shared runtime
   helpers, with borrowed storage identity and focused boundary tests. Sprint
   17.1 now materializes non-variadic generic function/closure bodies from pinned
   type/const arguments, including recursive worklists, identity deduplication,
   const extents, and concrete cleanup decisions. Sprint 17.2 executes object
   hierarchy/interface dispatch, checked casts, generic owner methods, real
   constructor bodies, inherited field storage and intrusive object lifecycle.
   Its focused generated-code gate covers self/ref/view, any, scope cleanup,
   weak lifetime and malformed hierarchy metadata. Sprint 17.3 executes async
   state transitions, generic/void task payloads, executor handoffs, cancellation,
   async interface methods and retained self/frame cleanup; the focused gate
   covers generated code and malformed coroutine metadata. Sprint 17.4 now
   executes native/POD/generic/extension adapters and checked wire-v2 thunks,
   canonical SDK sidecars, v11 scalar compatibility and module-owned values.
   Independent DLL/host gates cover Unicode, ref aliasing/copy-back, callback
   lifetime/thread contracts, intrusive ownership and async shutdown. Sprint
   17.5 now executes application construction/lifecycle through one standalone
   and SDK-hosted runtime, and publishes callable constructor/field/method plus
   enum/attribute reflection through an additive native sidecar. Sprint 17.6
   adds standalone and clean-project legacy/WIR differential gates, compile-time
   measurements, Windows/Ubuntu release qualification, variadic pack expansion,
   concrete generic-owner dispatch, constructor identity, native template/string
   ABI adaptation, `Entry(string[])`, and executable behavioral-attribute
   weaving with canonical processor/hook identities. The compatibility
   generator remains the explicit rollback/oracle for the first default-WIR
   release. The bytecode VM consumes the same contract. Async functions now carry
   payload/frame/state contracts;
   awaits and executor handoffs lower to cancellation-safe suspend/resume state
   machines, and async exits lower to coroutine completion. Application,
   system, effective attribute processor, and detailed reflection metadata now
   use the same verified contract. The language-surface freeze adds stable
   globals and initializer functions, canonical range/array/dictionary
   iterators with structured loop exits, explicit Result unwrap/propagation,
   duration and range-containment operations, generic pack/constant types with
   per-operand expansion metadata, and pinned overloaded-operator dispatch. The
   canonical optimizer now performs checked constant folding, CFG simplification,
   trivial SSA propagation, conservative DCE, escape/storage classification,
   and proven fixed-array bounds-check elimination while preserving stable IDs,
   source maps, ownership, and observable failure behavior. Continue broadening
   long-running platform executor/exceptional cleanup and benchmark samples as
   release-hardening evidence; these no longer block the canonical C++ backend
   pipeline milestone. Sprint 18 adds the independent bytecode compiler: a
   checksummed sectioned `.wiob` v1 container, pinned opcode and semantic-enum
   encoding, deterministic Lowered-WIR compilation, complete nominal/native/
   coroutine/module-contract metadata, bounded decoding, SSA/CFG verification,
   disassembly, compiler and self-hosted CLI emission, atomic output replacement,
   and a libFuzzer loader target. Sprint 19.1 now adds the separately linkable
   `wio_vm` runtime, a compact tagged runtime value, load-once verification and
   block/register indexing, bounded register call frames, direct calls,
   constants/defaults, scalar unary/binary/conversion/range operations, SSA
   block-argument transfer, jumps/conditional jumps/returns, source-aware
   stable `WVM` diagnostics, execution/call-depth/register safety budgets, and
   mutable local/persistent-global places with init/load/store/replace/borrow/
   move/copy/drop semantics. Sprint 19 is now closed with intrusive aggregate,
   object/container, coroutine/executor, exceptional-cleanup, stack-trace, and
   instruction-observer execution plus thread-safe debugger pause/resume/abort
   control. Sprint 20.1-20.6 now load the shared wire-v2 native registry and
   execute scalar/string/text/nullable/ref/view/POD/opaque/object/runtime
   thunks, retained callbacks, and adapted native async tasks. Foreign-thread
   completion is rescheduled through the VM executor, cancellation propagates
   to the producer, and teardown releases each task once. Dynamic library
   discovery pins producer code across escaped values and tasks, and project
   build/run can select the VM with paired bytecode/native-bridge outputs.
   The native differential gate qualifies the implemented boundary and adds
   owned string/text callbacks, structural runtime-value identities, mixed
   ref/view aliases, and retained worker entry. Remaining native parity work:
   The native POD layout slice now materializes local components, unpacks native
   results into editable fields, preserves unexposed C++ fields and existing
   field borrows, and qualifies nested/concrete-generic/empty/async PODs against
   the C++ backend. Fixed-array field adapters now cover scalar/native-enum,
   nested-POD, multidimensional, concrete-generic, and empty arrays, preserving
   element/field borrows during copy-back and using producer-specific empty
   storage extents. Opaque and nullable-opaque fields now use checked borrowed
   `void*` adapters, including pointer arrays, aliases, concrete generics, nested
   components, copy-back and async results. Nullable scalar/component wrappers
   are deliberately not POD fields. Concrete const-generic/default/zero-extent,
   partial/explicit specialization and variadic/empty-pack component shapes now
   use the same adapters. Template arguments are checked for concreteness
   separately from physical fields; unused string/text arguments are allowed,
   but pack-storage fields are not part of the POD contract. Native defaults now
   use an optional producer-owned factory sidecar: C++ `T{}` runs once, visible
   and hidden defaults survive copies, and explicit arguments override visible
   fields. Nested/fixed-array defaults and producer-library lifetime are covered
   by focused differential gates. Overlapping aggregate/field/array-element
   ref/view arguments now use producer-offset projections into one outer POD
   slot, preserving address identity, reversed argument order, hidden fields,
   mutable-field updates through a view root and existing borrows. Old producers
   lacking the additive capability bit reject this shape explicitly.
   Dynamic array adapters now materialize VM-created arrays in producer storage
   and decode producer results back to editable VM arrays. Empty/nested arrays,
   string/text/bool and native POD elements, by-value isolation, view/ref aliases,
   and same-shape ref copy-back have focused differential coverage. Structural
   ref copy-back now permits dynamic array growth/shrink/clear with invalidation
   of replaced element storage. Overlapping dynamic-array/element arguments now
   use capability-gated logical index projections into one producer root slot;
   registered POD field/fixed-array paths compose with those indices. Dictionary
   value paths now use typed, capability-gated key lookups; key references and
   bool-proxy paths remain unqualified.
   Dictionary adapters now cover ordered/unordered maps, portable key identities,
   string/text/native-POD/nullable-opaque values, nested dictionaries and mixed
   array/dictionary shapes. Bulk producer reads use one traversal; copy-back
   matches by key and updates existing cells despite unordered rehashing.
   Same-key-set ref updates and escaped producer-library lifetime have focused
   coverage. Structural key-set updates now remap surviving key/value places and
   invalidate removed keys, including nested sequence changes. Dictionary/value
   arguments now share one producer slot, including mixed array/map/POD paths.
   Shared iterator/snapshot structural storage and dictionary key references
   remain explicitly unqualified.
   Any adapters now materialize VM-created value payloads and decode producer
   any results using stable payload identities. Producer cell backing preserves
   shared-copy mutation, while whole-value ref replacement leaves old aliases
   intact. Scalar/string/text, supported array/dictionary, opaque, native POD,
   null defaults, and escaped producer-library lifetime have focused coverage.
   Payload discovery uses concrete AnyBox/is/fit types; unknown payloads and
   ambiguous C++ aliases reject explicitly rather than guessing identities.
   Object bridge foundations now define an additive host-proxy sidecar, stable
   object/type/generation identities, typed producer Ref/WeakRef adapters, and
   caller-thread method dispatch back into a live VM. Focused qualification
   covers shared identity, native-held host lifetime, weak-cache expiry,
   construction/dispatch failure cleanup and transactional registration.
   The compiler now emits producer proxies and object sidecars for concrete
   method-only objects with private scalar/string/text fields and supported
   synchronous method signatures. Both WIR dispatch slots and native-visible
   public wrappers route by stable method ids; hosted instances skip duplicate
   Wio lifecycle bodies. Real generated executable/DLL plus live-VM host
   qualification covers scalar/string/text/void methods and retained Ref/weak
   lifetime. Machine now automatically materializes eligible VM objects, reuses
   live proxies through a weak identity cache, and recovers original objects on
   native return/ref replacement. Final release schedules Wio cleanup at VM safe
   points; shutdown invalidates retained proxies. Focused C++/VM differential
   coverage includes scope lifecycle, retained Ref, wrong-thread/stale-host
   failures, overlap rejection, cross-machine/generation validation and safe
   producer code-lease retirement. WIR object borrow/retain/place-transfer rules
   now prevent hidden register claims from deferring lexical destruction.
   Hosted methods now accept/return qualified concrete VM object values,
   including self/argument identity and freshly constructed method-only types.
   Signature dependency discovery uses a work queue with transitive rejection;
   unsupported dependencies cannot leave partial proxies. Stack argument guards
   release owned object/string/text codec wires on success and failure. Focused
   C++/VM qualification covers 50 lifecycle rounds, signature-only adapters,
   same-machine identity, cross-machine/producer-owned rejection, VM panic,
   shutdown, partial conversion cleanup, and exactly-once returned-object
   destruction when proxy materialization fails. The new method-values gate
   passed locally on Windows (1/1, 24.38 seconds).
   Object ref/view method parameters now use alias-preserving tagged handle
   cells, with staged transactional copy-back and panic-unwind copy-back.
   Distinct cells holding one object remain independent. Unqualified borrowed-result origins,
   open generic object methods and foreign-object field/method
   synchronization remain pending. Focused qualification covers 30 lifecycle
   rounds, ref/ref and ref/view aliases, malformed references and partial
   factory failure while staging one or multiple replacement cells.
   The reference-method gate passed locally on Windows (1/1, 18.55 seconds).
   Nullable qualified-object values now cross hosted methods and free-native
   calls without an ABI box or a wrapper-specific proxy identity. Null wires
   have checked flags/type ids, and nullable-only method dependencies receive
   adapters. Typed WIR now unwraps flow-narrowed nullable storage for identity
   comparison and releases consumed wrap/unwrap temporaries promptly. Focused
   qualification covers 40 lifecycle rounds, C++/VM/free-native identity,
   malformed null rejection and partial proxy factory output cleanup.
   The nullable-method gate passed locally on Windows (1/1, 10.76 seconds).
   Nullable object ref/view method parameters now preserve aliases and copy
   null/object transitions back through tagged cells. Mixed nullable/non-nullable
   alias declarations reject. Two-cell copy-back rolls back staged null and
   object replacements when a later factory fails; panic copy-back preserves
   null updates. The new focused C++/VM/host gate covers 30 lifecycle rounds,
   view-first aliases, readonly/malformed cells, partial factory failures,
   direct/virtual/function-value owned arguments and final-handle null cleanup.
   VM synchronous calls now transfer owned SSA arguments to the callee; C++
   indirect calls consume owned arguments too. Async task-launch ownership needs
   separate lifecycle qualification before applying the same transfer rule.
   The nullable-reference gate passed locally on Windows (1/1, 24.28 seconds).
   Receiver/compatible-parameter-rooted object ref/view method results now
   preserve caller cell identity, including nullable cells and assign-and-return
   copy-back. Receiver wires validate host identity and map back to raw self
   without a persistent strong pin. Separate proxies of one VM object work;
   readonly results cannot rebind. Conservative WIR provenance checks reject
   global/local/projection/nested-call/cyclic origins and readonly escalation.
   Lexical object receivers avoid unnecessary owned SSA copies, and synchronous
   VM receiver formals do not introduce hidden strong claims. The new focused
   C++/VM/host gate covers 30 lifecycle rounds, aliases, nullable/read-only paths,
   branch selection, separate proxy identity and malformed result rejection.
   The borrowed-result gate passed locally on Windows (1/1, 14.94 seconds).
   Concrete fixed-arity type/const object specializations now receive producer
   proxies with distinct stable identities and specialized reflected methods.
   Generic declaration self compatibility accepts matching symbolic arguments
   while rejecting different type/const specializations. Open generic bodies
   remain signature-only WIOB 1.1 metadata; concrete bodies retain their origin
   identity. Focused C++/VM/host qualification covers 30 rounds per specialization,
   scalar/string/default-const values, owned/ref/view self returns, clones,
   lifecycle counts, reflection/origin metadata, wrong-type handles, malformed
   declarations, recursive openness and format compatibility. Variadic/partial
   specialization and generic method templates remain unqualified at this boundary.
   The concrete-generic gate passed locally on Windows (1/1, 15.25 seconds).
   Concrete hosted object hierarchies now preserve inherited private scalar/
   string/text storage and route native C++ base/interface pointer calls to the
   concrete VM override. The Base/Derived/Leaf gate covers repeated interface
   paths, inherited method identity, direct super calls, constructor/destructor
   order, weak expiration and VM partial-construction cleanup. Owner-local WIR
   fields map to cached VM offsets; base initialization plans are cached, while
   flat construction retains its existing fast path. Hosted C++ placeholders
   suppress lifecycle bodies throughout the hierarchy. Non-deducible native
   template parameters retain their explicit selected arguments.
   The hierarchy gate passed locally on Windows (1/1, 16.35 seconds), comparing
   normal generated C++/VM execution and exercising the live host. Constructor
   failure injection is VM-host qualification, not differential C++ coverage.
   Qualified hosted objects now cross by-value Base/interface native parameters
   and results, including nullable handles. Concrete adapters are discovered
   from base/interface-only boundaries. Generated codecs perform checked
   producer-local upcasts while preserving concrete wire identity, handle
   operations and storage; the VM recovers the original concrete object rather
   than slicing it. Native-held base and interface claims keep one shared host
   alive until final release. The new C++/VM/host gate covers 30 lifecycle rounds,
   dynamic type tests/downcasts, shared identity, nullable/null round trips,
   native retention, incompatible siblings/results, missing concrete adapters
   and forged handle operations. Producer-owned foreign subclass synchronization,
   field synchronization and
   SDK/reflection routing remain pending; this does not close object parity.
   The polymorphic gate passed locally on Windows (1/1, 12.53 seconds).
   Qualified Base/interface ref/view handle cells now retain their concrete
   object identity across native arguments and hosted method copy-back. Methods
   can return caller-rooted Base/interface borrows, including nullable cells;
   the result aliases caller storage, not a temporary VM frame. Same-contract
   ref/view aliases share storage in either argument order; different static
   or nullable/non-nullable contracts are rejected. Producer cells distinguish
   static C++ storage shape from concrete wire identity. Method-only signature
   discovery includes descendant adapters and the VM caches supported contracts.
   The new differential gate covers concrete rebinding, independent cells,
   borrowed results, nullable transitions, readonly/shape/type/ops rejection and
   final lifecycle counts. Inherited Base self ref/view methods also preserve
   the concrete Leaf receiver, without rebind permission or a retained claim.
   The polymorphic-reference gate passed locally on Windows (1/1, 20.30 seconds).
   Explicit covariant receiver borrows and broader foreign ownership remain
   unqualified; caller-bound free-native borrowed results are qualified below.
   Direct synchronous native any/ref-any/view-any payloads now preserve
   qualified concrete/base/interface object identity and concrete generic
   specializations. Any-only signatures discover the required object adapters;
   producer cells retain exact static payload shape while object wires retain
   concrete identity. Borrowed Any loads and owned place transfers no longer
   defer lexical cleanup. Owned fit temporaries release after owned casts.
   The focused C++/VM/host gate covers shared mutation, replacement and
   self-assignment, native retention, escaping values, missing/forged adapter
   rejection and 30 rounds of exactly-once lifecycle checks. This does not
   qualify std::Box, arbitrary nullable/nested Any payloads or Any containers.
   The new object-Any gate passed locally on Windows (1/1, 25.29 seconds).
   Concrete native POD Any/array/dictionary adapters now reuse physical-layout
   eligibility instead of rejecting generic arguments. Type/const/default,
   zero-extent, selected specialization, concrete variadic/empty-pack and nested
   POD shapes preserve hidden native state and producer-library leases. Open
   parameters and non-POD fields remain excluded. A focused 30-round C++/VM/host
   gate checks shared Any mutation, ref replacement, copy isolation, aliases,
   borrowed field updates, adapter selection and final library release. Nested
   generic construction also fixes stale type-table pointers in Typed WIR
   contextual conversion; no format change or broad native closure is implied.
   The new generic-POD Any/container gate passed locally on Windows
   (1/1, 30.78 seconds).
   Nullable owning object/interface/concrete generic object and opaque handles
   now cross direct native Any boundaries with the existing C++ erasure rule:
   null becomes null Any, while present handles use ordinary payload descriptors.
   Base/interface storage shape and concrete object identity remain distinct.
   Nullable type tests and checked casts accept qualified handle targets;
   a null Any still fails checked casts rather than inventing a typed null cell.
   Nullable reference/callback storage remains rejected. The focused gate covers
   30 C++/VM lifecycle rounds, rebinding, native retention, escaping handles,
   opaque pointers and live-host rejection checks. It also fixes flow-narrowed
   object fit operands and nullable/inner opaque equality in Typed WIR.
   The new nullable-Any gate passed locally on Windows (1/1, 25.39 seconds).
   Direct synchronous dynamic arrays/dictionaries now materialize qualified
   Any/object/interface/concrete generic object values, including nullable object
   array elements and nested Any arrays. Object/Any discovery recurses through
   container-only native boundaries; conversion carries call-local object access.
   Any/container sidecars validate as one atomic candidate graph. Container copies
   stay independent while their Any cells/object handles remain shared; native ref
   copy-back preserves same-shape element borrows. Array/dictionary literal operand
   claims release after construction, and VM owned container place writes transfer
   rather than retain an extra register claim. Borrowed loads and caller-rooted
   projections no longer keep containers alive until frame teardown; root cleanup
   invalidates projections through an allocation-free linked list. Nullable object
   equality uses the existing identity opcode in both WIR verification stages.
   The new handle-container gate passed locally on Windows (1/1, 35.08 seconds),
   including generated C++, actual VM CLI and live-host negative/lifecycle checks.
   Direct synchronous free-native ref/view results now alias an exact-compatible
   caller reference parameter. Generated C++ checks the returned native address;
   producer thunks return caller token identity, and VM validation maps it back
   to the original caller Place without owning/copying its value. Forwarding
   through Wio frames preserves the cell. Semantic matching rejects implicit
   numeric/nullable/nominal contract changes and readonly-to-mutable upgrades;
   lifetime analysis conservatively includes every compatible native/indirect
   call argument, preventing later local candidates from escaping.
   The new qualification covers scalar/string/text/POD/Any/container, hosted
   object/interface/generic and nullable object/opaque cells, rebinding, field-only
   arguments, expired caller roots and malformed foreign result pointers/flags.
   At this stage, returning projected overlap tokens, arbitrary producer/global/local storage,
   asynchronous borrows and exported SDK ref/view results remain unqualified.
   Only the ten new focused tests were run on Windows: 10/10 passed in 18.32
   seconds total (C++/VM/live-host gate: 17.12 seconds).
   Explicitly boxed Any-in-Any payloads now preserve outer/inner cells through
   native boundaries. `is any` tests for a nested payload and `fit any` unwraps
   one layer; ordinary Any assignment stays a shared-cell copy. The generated
   producer emits the self-payload adapter even at container-only boundaries.
   Final native-backed Any cleanup releases producer proxy claims before child
   cleanup, preventing object destruction from slipping beyond scope exit.
   A new 30-round C++/VM/live-host gate covers mutation/rebind, nested null,
   Unicode/containers/POD/object payloads, native retention, shared-cell races,
   bounded/cyclic decoding, partial failure and escaped child DLL leases.
   Only the new focused Windows gate was run: 1/1 passed in 14.76 seconds
   (14.83 seconds total); no full-suite or local Linux pass is claimed.
   Native mutable containers now support structural copy-back: arrays may resize
   and dictionaries may change key sets. Same-shape cells and surviving dictionary
   key/value borrows are preserved; removed/reallocated descendant places reject
   stale reads. All mutable outputs are decoded and planned before any commit.
   Copy-back drains retired hosted proxies before old-value cleanup so removed
   objects run exactly-once destruction and destructor failures are reported.
   A focused C++/VM/live-host gate covers scalar/string/text/bool/POD/Any/object
   elements, nested containers, independent heap-field borrows, malformed second
   outputs, native failures, cleanup failures and recovery. Shared iterator/
   snapshot structural updates and broader overlapping projection arguments remain
   rejected; this does not close Native Step 8. Only the new focused Windows
   gate was run: 1/1 passed in 23.63 seconds (23.71 seconds total); no full-suite
   or local Linux qualification is claimed.
   Dynamic-array projection arguments now preserve producer address identity
   across reversed parameter order, repeated/readonly/mutable element borrows,
   and nested row/cell/root calls. Logical indices are resolved only by the
   producer's C++ Frame; no vector layout or VM address crosses the ABI.
   Outermost root copy-back handles nested growth; independent projected tokens
   drop redundant object/Any claims before invocation. Nullable object elements
   keep their exact storage type while using the static native handle contract ID.
   At this stage, root-bound native borrow results remain supported; projected-element results,
   dictionary key paths and bool proxies reject. Container-to-POD-field paths
   are qualified by the subsequent sequence/POD composition gate below.
   Missing producer capability and malformed paths fail before native body entry.
   Only the new 30-round C++/VM/live-host Windows gate was run: 1/1 passed in
   21.56 seconds (21.64 seconds total). Native Step 8 remains open; no full-suite
   or local Linux pass is claimed.
   Sequence/POD projections now compose dynamic-array indices with validated
   native POD field offsets, including nested generic components and fixed-array
   elements/rows. Generated typed field accessors reject undeclared/hidden fields,
   padding, wrong identities/types, invalid extents and misaligned paths. Array,
   POD element and field arguments share one outermost producer slot even in
   reverse order; mutable fields dirty readonly roots. Existing ABI structures
   keep their sizes and old producers require a new capability bit. Projected
   results and dictionary/bool paths remained excluded at this stage; the next
   slice qualifies dictionary value paths. Native Step 8 stays open.
   A new 30-round C++/actual-VM/live-host gate also covers multiple Wio surfaces
   of one C++ POD, 18 malformed ABI variants, old capability rejection, expired
   descendants, unsupported standalone fixed arrays, native failures and recovery.
   Only this new Windows gate was run: 1/1 passed in 12.83 seconds (12.90 total).
   The old POD-field expectation was updated without running that old gate.
   No full-suite or local Linux qualification is claimed.
   Dictionary value projections now resolve portable keys in typed producer maps,
   never unordered entry positions. Mixed array/dictionary/nested-map/POD/fixed-array
   paths preserve one outermost root, repeated aliases, reversed order and mutable
   values under readonly roots. Typed child identities and key codecs validate
   every step; owned key wires outlive source mutation and release with the frame.
   Structural key updates preserve surviving VM borrows and expire removed cells.
   At this stage, root-bound borrowed results remain supported; projected results, map-key
   references, bool proxies and broader callback/foreign ownership stay excluded.
   A new 30-round C++/actual-VM/live-host Windows gate covers scalar/NUL/string/
   Unicode/enum/u64/char/bool keys, mixed paths, Any, generic/nullable object values,
   hidden POD state, copy-back destruction failures, balanced ownership, 32 forged
   path variants, old capability rejection and recovery. Only this new gate was
   run: 1/1 passed in 33.27 seconds (33.34 total). The old overlap negative gate
   expectation was updated but that gate was not run. Native Step 8 stays open;
   no full-suite or local Linux qualification is claimed.
   Synchronous projected borrowed results now return an exact input-argument
   identity, never a native address or copied projection descriptor. A new
   additive result flag/capability preserves existing ABI/sidecar/bytecode sizes.
   Producer thunks match the original compatible ref/view and re-resolve its
   live path after the call. VM validation checks the advertised capability,
   exact argument wire, type, marshalling, mutability and post-copy-back Place
   lifetime. Dictionary rehash/key-set updates may preserve surviving values;
   structural arrays conservatively retire descendant results. Foreign/global
   storage and newly selected relocated elements cannot masquerade as inputs.
   Wio forwarding now transfers projection metadata into the caller frame,
   without owning the borrowed value or losing root/expiry links. Frame-local
   projections cannot escape directly through the public VM host result.
   VM structural array intrinsics also retire descendants before relocating
   cells; no-op reserve/remove/extend preserve them. A new 30-round C++/actual-VM/
   live-host gate covers scalar, string, POD/generic fields, fixed/dynamic rows,
   readonly/repeated aliases, dictionary/mixed paths, forwarding, rebind/expiry,
   14 malformed/mutability variants, missing capability, 11 array structural
   operations and failure recovery. Only this new Windows gate was run:
   1/1 passed in 17.01 seconds (17.08 total). Four older result expectations
   were updated but their gates were not run. Native Step 8 stays open;
   no full-suite or local Linux qualification is claimed.
   Dictionary intrinsic mutation now remaps surviving direct key/value aliases
   by key, preserves repeated/owner-backed aliases and expires removed or
   replaced container descendants. Set/GetOrAdd/Extend/TryGet/Remove/Clear
   cleanup is qualified with mapped object and nullable-object borrowed results,
   call-boundary argument release, exactly-once destruction, destructor failures
   and recovery in a new 30-round C++/actual-VM/live-host gate. Place storage and
   projection bookkeeping now live in named internal classes outside Machine.
   Only this new Windows gate was run: 1/1 passed in 23.36 seconds (23.47 total);
   no full-suite or local Linux qualification is claimed.
   Projected Any borrowed results are now qualified through the existing caller
   argument-alias contract, without a new ABI form. A new 30-round C++/actual-VM/
   live-host gate covers array/map/mixed paths, reversed readonly and repeated
   aliases, distinct cells sharing a payload, scalar/string/text/POD/nested-Any/
   object/nullable-object/null payload changes, forwarding, independent owning
   escape, expiry, destructor failure, 18 forged/mutability variants, missing
   capability and recovery. Only this new Windows gate was run: 1/1 passed in
   14.18 seconds (14.25 total); no full-suite or local Linux pass is claimed.
   Projected Wio object, concrete-generic object, interface and nullable handle
   results are qualified across arrays/maps/mixed paths, wrapper rebind and
   null transitions. The new 30-round C++/actual-VM/live-host gate checks distinct
   cells holding one object, reverse readonly/repeated aliases, forwarding,
   owning escape, native member entry, cleanup failures, 19 malformed/mutability/
   nullable-contract result variants, missing capability and recovery with
   balanced destruction after each failure. A new analyzer-negative gate pins
   required-vs-nullable reference incompatibility. Expired object/interface
   dispatch now diagnoses the uninitialized Place as WVM1043, not a null receiver.
   Only these two new Windows gates were run: 2/2 passed in 30.08 and 0.09 seconds
   (30.31 total); no full-suite or local Linux qualification is claimed.
   Native callbacks now admit by-value native POD component signatures when
   their layout sidecar is registered. Concrete callback argument/result type
   identities are preserved by generated C++ codecs; same-symbol callbacks with
   different lease/thread contracts no longer deduplicate into one thunk.
   POD returns use VM-owned bytes and survive callback/machine/producer teardown.
   A new 30-round C++/actual-VM/live-host gate covers nested/generic/fixed-array/
   padded/hidden-field/empty POD, multi-argument callbacks, independent value
   copies, retained/worker calls, malformed identities, panic/recovery, missing
   layouts and continued ref/view/nullable-object signature rejection.
   Only this new Windows gate was run: 1/1 passed in 16.61 seconds (16.69 total);
   no full-suite or local Linux qualification is claimed.
   By-value callbacks through native free functions now also admit Wio objects,
   concrete type/value generic objects, interfaces and their nullable variants
   when object proxy contracts are registered. Adapter discovery walks callback
   signatures, including types used only as callback results. A dedicated
   callback lifetime domain queues final lease release for VM-safe Wio cleanup;
   shutdown drops captures even when native code still retains stale leases.
   Typed WIR now releases capture staging copies after closure construction,
   fixing object lifetime extension in both C++ and VM execution. A new 30-round
   C++/actual-VM/live-host gate covers identity/mutation, retained/worker calls,
   foreign-thread final release, multiple leases, six malformed arguments,
   cross-machine rejection, wrong count/affinity, nullable tags, panic/recovery,
   destructor failure and outstanding callback/result wires at machine shutdown.
   Only this new Windows gate was run: 1/1 passed in 23.35 seconds (23.45 total);
   no full-suite or local Linux qualification is claimed.
   By-value array/dictionary/Any callback signatures now use registered container
   sidecars, including callback-only structural types, nested POD/object/interface
   and nullable object elements. Cached Any results acquire their own owning
   handle; escaped results pin loaded producer libraries until a host safe point
   after final release. Recursive decoding rejects malformed nullable tags and
   objects from another machine. Typed WIR preserves receiver dispatch kind
   across type-table growth while lowering complex receiver expressions.
   A new 30-round C++/actual-VM/live-host gate covers empty/bool/nested arrays,
   ordered/unordered maps, Any nesting, retained worker calls, missing sidecars,
   malformed inputs, cross-machine rejection, panic/recovery, balanced lifecycle
   and result reads after machine/callback/manual-loader shutdown. Only this new
   Windows gate was run: 1/1 passed in 43.22 seconds (43.43 total); no full-suite
   or local Linux qualification is claimed.
   Hosted Wio object methods now accept typed native callable values through
   the same scalar/string/text/POD/object/nullable/container codecs. Native
   callable backing uses intrusive ownership without enlarging ordinary Wio
   closures; direct calls, Wio closure capture and forwarding to free native
   functions (including retained caller-thread leases) preserve its claim.
   Callback method-only structural identities and container adapters are
   discovered automatically. Shared codec spelling now pins exact concrete
   callback argument/result IDs for both free-function and hosted-method paths.
   A new 30-round C++/actual-VM/live-host gate covers direct/captured/retained
   forwarding, string/text/POD/array/map/Any/nullable object values, malformed
   callback contracts, exceptions/result mismatches, recovery, receiver thread
   affinity, stale shutdown and continued rejection of ref/view callbacks.
   Only this new Windows gate was run: 1/1 passed in 18.17 seconds (18.25 total);
   no full-suite or local Linux qualification is claimed.
   Callback arguments now additionally accept invocation-scoped scalar and
   qualified native POD ref/view cells in both directions, including hosted
   methods, forwarding and retained callbacks invoked with fresh cells.
   Exact pointee identities and mutability are retained in generated codecs.
   Repeated aliases share materialized storage; mutable copy-back is staged
   before publication and completed writes survive a contained callback panic.
   Native callbacks cannot write through view places. A new 30-round C++/
   actual-VM/live-host gate covers reversed aliases, hidden/padded POD storage,
   forwarding, native exceptions, malformed tokens, concrete-type disagreement,
   scalar range failure, unsafe VM reference-capture rejection, panic/recovery
   and stale machine shutdown. Only this
   new Windows gate was run: 1/1 passed in 13.07 seconds (13.16 total); no full
   suite or local Linux qualification is claimed.
   Callback ref/view arguments now also accept byte strings and Unicode text in
   both directions, including hosted methods and retained fresh-cell calls.
   Owning slice copy-back carries matching wire/owner identities. Both C++ and
   VM stage returned values before publication, rejecting invalid UTF-32 or a
   mismatched later pointee identity without partially changing earlier cells.
   A new 30-round C++/actual-VM/live-host gate covers aliases, append, empty/large
   values, embedded NUL, emoji/ZWJ/combining text, malformed cells, unaligned
   UTF-32 data, panic copy-back, producer ownership and escaped result lifetime.
   Only this new Windows gate was run: 1/1 passed in 17.52 seconds (17.61 total);
   no full-suite or local Linux qualification is claimed.
   Qualified callback arguments now additionally accept invocation-scoped
   array/dictionary/Any and hosted object/interface/nullable-object ref/view
   cells. Recursive sidecars preserve nested object identity; polymorphic
   tokens keep dynamic object identity separate from the static cell contract.
   Borrow materializations use normal Wio cleanup and survive until result
   encoding; copy-back pins producer code for escaping container wires.
   Hosted placeholders now allow private empty-default dynamic container/Any
   fields without running element lifecycle or exposing native projections.
   A new 30-round C++/actual-VM/live-host gate covers value replacement and
   in-place mutation, nested POD/object/nullable containers, object/interface
   rebinding, nullable transitions, Any payload changes, readonly-only and
   retained fresh-cell calls, malformed/missing sidecars, cross-machine
   rejection, staged copy-back, unsafe borrow capture, panic/recovery and
   balanced object destruction. Only this new Windows gate was run: 1/1 passed
   in 50.73 seconds (50.84 total); no full-suite or Linux qualification is claimed.
   Concrete fixed arrays now use structural sequence sidecars at standalone
   native and callback boundaries, including value and invocation-scoped ref/view
   arguments/results by value. Recursive nested/dynamic arrays, dictionaries,
   Any, string/text, native POD, hosted object/interface and nullable-object
   elements are qualified; zero extents preserve their fixed identity.
   Native POD fixed-array fields retain their physical byte ABI through a
   separate storage codec. Structural packing initializes elements directly
   without replaying native POD member initializers. Fixed bool elements now
   borrow real C++ cells rather than vector<bool> proxies. Both producer and VM
   reject wrong fixed extents.
   A new 30-round C++/actual-VM/live-host gate covers array/element aliasing,
   hosted native callbacks, hidden/reordered POD fields using the same array
   types, malformed handles, absent sidecars, extent mismatches, recovery,
   balanced object lifecycle and escaped owning results after loader shutdown.
   Only this new Windows gate was run: 1/1 passed in 31.21 seconds (31.28 total).
   The older row-only projection expectation was updated but not rerun; no
   full-suite or local Linux qualification is claimed.
   Callback closure packages 1 and 2 now qualify recursively typed callable
   arguments/results, callable and nullable-callable hosted method results,
   and caller-cell-bound ref/view callback results. C++ direct calls, generated
   ABI callbacks and VM calls share the explicit caller-cell rule: no readonly
   upgrade, frame-local/projected result escape, or borrowed closure capture.
   Direct/nested lexical-reference captures cannot cross the native boundary;
   changed callable environments are checked again at native invocation.
   Retained callbacks accept fresh invocation cells, expire after shutdown,
   and preserve caller/any-thread policy. Owning callables are qualified across
   native worker calls and VM coroutine suspension/cancellation; async ref/view
   parameters/results remain forbidden. See the focused closure gate and the
   two new async borrow/result negative gates.
   VM capture cells preserve reference indirection and persistent mutable value
   captures. Completion/cancellation balance captured object lifecycle. Only
   these new Windows tests were run: 3/3 passed in 42.29 seconds total.
   Native closure packages 3 and 4 qualify consumed invocation claims, owned
   foreign resource/container handles, std::Box method surfaces and canonical
   native projection identity. Consumed arguments preserve caller value/copy
   semantics and intrusive Ref ownership; failure/cancellation and producer
   shutdown are covered by the focused ownership gate. Box qualification covers
   scalar/string/text, dynamic container/Any and safely defaultable object-handle
   payloads; it does not expose raw placeholder fields as a native API.
   Exact callback cell aliases remain supported. Aggregate/descendant callback
   overlap is explicitly rejected before invocation; use one aggregate borrow.
   Native task-input/helper qualification is recorded in the closure below.
   Native-only dependency selection now removes unrelated Wio C++ bodies while
   preserving registry/POD/hosted-object, task and dynamic Any adapter roots.
   The full bytecode and stable identities remain unchanged; project VM builds
   use the new opt-in compiler mode automatically. Final header/PCH/sharding
   cost work stays in Backend item 9. VM host/executable/shared/static artifact
   packaging remains; task-parameter/helper closure is recorded below.
   Canonical Any identity for inferred/direct/nested literal arrays is now
   normalized at boxing, including generic substitution, without erasing
   explicit fixed-size array identities or changing the ABI. The two new gates
   qualify C++/full-VM/pruned-VM parity and reject forged payload metadata.
   Direct/nested inferred-array arguments now materialize through owned
   Copy/CopyValue instead of invalid numeric conversion. Two new verifier and
   C++/full-VM/pruned-VM gates qualify that fix; forged element identities reject.
   Owning native task parameters now use a typed task-source capability,
   cancellation/completion operations and cooperative main-thread waits. Start
   and collection copies preserve VM task identity, and older producers without
   the capability reject before entry. Qualified scalar/string/text/container,
   native POD/object/Any and nested task payloads, task callbacks and free std
   helpers have focused native/full-VM/pruned-VM coverage. Real std Poll/Select
   and timeout wrappers exercise enum field defaults and pending producer inputs.
   Ref/view async storage and consumed task arguments remain forbidden.
   The runtime-only bytecode core is now separated from the compiler. Standalone
   wio-vm embeds verified bytecode into an executable and deploys relative native
   producers beside it; pure applications need no compiler, source or .wiob at
   runtime. New codec/publication and pure/native/async/Unicode deployment gates
   cover this slice. Project CLI build/run/package now use the embedded image,
   content-versioned native copies and verified replacement. Compilation and
   packaging reuse are separate; no-build run/package needs no source or .wiob
   intermediates. Source/native/toolchain output collisions are rejected, and
   packaging verifies before cleaning an existing distribution. VM-backed
    shared/static Wio modules now have compiler-independent CMake wrappers,
    stable SDK call tables and owning scalar/string/text/enum/flagset exports,
    including concrete generic functions and explicit embedding lifetime owners.
    Two new focused gates cover installed-prefix relocation, shared/two-static
    hosts, invalid modules, panic recovery, wrong-thread calls and escaped values.
    Independent SDK sessions now support start/update/unload, API version,
    scalar/string/text save/restore, failed-start rollback and owner-thread
    retirement outside DLL loader teardown. Two new focused gates cover
    reentrant view closure, exactly-once cleanup and independent shared/static
    globals. Async exports now return canonical owning task sources with
    scalar/string/text/enum/flagset/void payloads and concrete generic identities.
    Additive task sidecars preserve session ABI, enforce session/thread provenance
    and support explicit poll/wait/cancel/main pumping and once-only subscriptions.
    Session close quiesces workers/timers before unload; escaped wires retain code
    and immutable metadata without retaining a live VM. New focused gates cover
    installed shared/static async hosts and 80 shutdown race repetitions.
    Owning task parameters now preserve the same session/task identity across
    synchronous relay and suspended async calls, including concrete generics.
    Canonical generic-export checks are aligned with ordinary exports while
    legacy C ABI rejection remains. New focused import and installed-host gates
    cover cancellation direction, pending/fault/void/Unicode inputs and raw ABI
    rejection before body execution. Foreign task creation, cross-session task
    transfer and complex/nested SDK task payloads remain unqualified SDK-kit
    capabilities, tracked by Native/SDK item 5 as v1 blockers, not backend closure.
    Explicit native registry/private runtime dependencies now use installed
    CMake helpers, image-relative resolution and SHA-256 bundle generations.
    Shared modules deploy beside their image; static modules deploy beside the
    final host. Session open rolls back partial registration before publication
    or user Load. New focused Windows gates cover relocated Unicode paths,
    private DLLs, missing/invalid producers, final-session shutdown and immutable
    bundle hash/collision/traversal/symlink policies. Opt-in component install
    rules now deploy the containing shared image/final static host with its exact
    native generation. Explicit repair targets restore missing build-tree bundles
    without relinking; reinstall restores missing distribution files. A new
    installed-host gate covers invalid API inputs, immutable conflicts, install
    manifests and real private-library rebuilds with retained old/new images.
    Project CLI now builds, runs native hosts and packages shared/static VM
    libraries using the installed runtime's CMake project. An explicit library
    sidecar kind preserves module identity rather than producing a program.
    Recorded hashes protect no-build image/dependency selection; missing bundles
    repair without relinking. Bare archives are qualified through an independent
    C++ consumer. Two new Windows Debug gates passed 2/2 in 91.65 seconds,
    including Unicode relocation and executable sidecar compatibility.
    Distribution policy now requires explicit producer/private dependency lists,
    with no automatic transitive discovery. Executable private dependencies use
    checked v2 cohorts preserving import filenames; old generations are retained
    intentionally and fresh packages copy only the selected cohort.
    Bundle/framework support is excluded by the current policy; broader platform
    acceptance remains open. See `docs/WIO_NATIVE_DISTRIBUTION_POLICY.md`.
    Full SDK complex-value/async/application/reflection parity is outside this
    backend milestone and remains mandatory under Native/SDK item 5 before v1.
    The final focused artifact/runtime platform matrix passed Windows Debug,
    Linux Release and macOS Release. The three agreed heavy optimization passes
    remain open; live-generation garbage collection is not promised by the
    frozen retention policy.
    See `docs/WIO_VM_ARTIFACTS.md`; bounded SDK wrappers are not a claim of full
    kit parity, nor a reason to merge its separate v1 gate into backend closure.
   Unconstrained
   T-to-Any boxing remains semantically unsupported. See
   `docs/WIO_VM_NATIVE_BRIDGE.md` for the qualified surface and negative cases.

9. [ ] Close generated-native cost and build scalability before v1.
   Fixed pass-1 substeps N01-N14 and coverage are tracked in
   `docs/WIO_NATIVE_RUNTIME_PLAN.md`. N01 starts the native cost map, not runtime
   transformations. N02's bounded string/text/concat implementation is complete:
   transactional append, pure-tree flattening and last-use owned buffers; see
   `docs/WIO_CONCAT_OPTIMIZATION.md`. N01 residual profiling and N14 qualified
   timing acceptance stay open; all three N02 timing sessions remain noisy.
   N03 formatting/builders is in progress: finite float stack formatting and
   direct builtin-container interpolation are implemented, with VM display
   conformance fixes and frozen related four-implementation measurements.
   Direct primitive builder Append/AppendLine now use canonical formatting fusion
   and typed C++/VM append; see `docs/WIO_NUMERIC_BUILDER_OPTIMIZATION.md`.
   Scalar synchronous user ToString now lowers to shared WIR virtual/interface
   calls, including concrete generic callers, views and temporaries; see
   `docs/WIO_FORMATTING_CALL_LOWERING.md`. Object-container/async/native/open-
   generic hooks (KI-008), richer formatting and legacy Format remain open.
   Bounded call-scoped native string borrows now use proven stack cells and
   private C++ reference adapters; ordinary ABI and exception boundaries remain.
   The shared-WIR streaming experiment was rejected for added VM cost. See
   `docs/WIO_NATIVE_STRING_BORROW_OPTIMIZATION.md`. Indirect native function
   values still fail in VM (KI-009); this is not N03 or VM parity closure.
   Proven bounded scalar field projections now avoid general Place transport,
   and final interpolation of already-rendered string parts pre-reserves checked
   byte capacity without expanding WIR/VM instructions. See
   `docs/WIO_SCALAR_FIELD_OPTIMIZATION.md` for before/intermediate/final speed,
   heap/RSS/code-size evidence and the still-open timing acceptance/C++ gap.
   The pre-existing mutating-RHS/borrowed-LHS
   native/VM parity defect (KI-007) remains a separate correctness fix before v1.
   `std::benchmark` and
   the Wio-written `wio-bench` supply the required Wio/native/VM + C++ + C#
   before/after and final-Wio comparison workflow; see `docs/WIO_BENCHMARKS.md`.
   Std consolidation includes deleting unnecessary/duplicate
   APIs and merging suitable modules (candidate: sort into algorithms), with
   explicit dependency/import/API migration and native/VM qualification.
   First measured slice (`722e836`): proven closed scalar cells use inline
   storage, primitive SSA loses optional wrappers and synchronous CFG uses
   direct edges. Mandelbrot compute-only is 33.44x faster than original Wio,
   1.027x the C++ median; this does not close string/format/managed/container/
   async/application or compile-time budgets. See
   `docs/WIO_NATIVE_SCALAR_OPTIMIZATION.md` for raw evidence and scope.
   Follow-up `79568bd` promotes closed synchronous string cells and removes
   proven adjacent append snapshots: 5.095 to 3.221 ms versus the previous std
   slice, 90,020 to 30,020 allocation calls, still 5.84x the paired C++ reference.
   See `docs/WIO_MANAGED_STRING_OPTIMIZATION.md`; managed/formatting costs and
   the separate KI-006 borrow diagnostic boundary remain open.
   Subsequent direct integer/interpolation and general observing-string-copy
   slices are recorded in `docs/WIO_INTEGER_INTERPOLATION_OPTIMIZATION.md` and
   `docs/WIO_NATIVE_STRING_FAST_PATHS.md`. The latter improves the separate long-
   string workload by 19.30% and halves its allocation calls, but establishes no
   meaningful new Mandelbrot gain. Both adverse/favorable native rechecks, fresh
   Release VM samples and the fixed dated language references are retained.
   Treat the native C++ path as a product backend with enforced size, compile
   time, memory, runtime, and binary-size budgets. A trivial program must not
   emit unused async/reflection/native infrastructure or thousands of lines of
   duplicated support code; ordinary locals must not require `std::optional`,
   `std::function`, or heap ownership. Add capability-driven emission,
   precompiled std/runtime artifacts, Lowered-WIR dependency/SCC sharding,
   parallel compilation, content-addressed object reuse, debug/release build
   policies, retain/release elision, and generated-code regression dashboards.
   Keep source-file boundaries out of backend semantics: shard by canonical
   dependency and cost while preserving stable IDs and source maps. The v1 gate
   compares generated native code with equivalent hand-written C++ and rejects
   material regressions. JIT compilation remains post-v1, but the canonical
   calling convention, source maps, profiling counters, and backend-neutral
   intrinsics must leave a clean JIT insertion point.

### Final Pre-v1 Optimization And Performance Acceptance

- [ ] Execute the accepted three heavy passes after functional backend closure:
  (1) generated C++/runtime, targeting 5-10% overhead versus equivalent optimized
  C++ where feasible; (2) best practical current non-JIT VM runtime performance;
  (3) full compilation/preparation throughput from lexer/parser/analyzer through
  Typed/Lowered WIR, C++/bytecode emission and VM load/verify/index work, including
  native compilation/linking. Keep original/previous/current measured baselines.
  See `docs/WIO_BACKEND_OPTIMIZATION_PLAN.md`. The final pre-v1 checkpoint below
  requalifies these budgets after later kit/std/IDE work; it does not postpone
  these passes or add a fourth heavy optimization package.

- [ ] After the three passes, rerun the same Mandelbrot case across C++, Wio
  native, Wio VM, C# and Python, with separate compute/format-buffer/full-PPM
  measurements and matching hashes. Preserve original/previous/final evidence,
  versions/configurations and timing variability; do not equate historical
  timeouts with exact durations. This is an explicit final comparison gate,
  not an additional optimization pass.

- [~] Every optimization update also reports fresh Wio native and Release VM
  measurements on that same Mandelbrot case, with fixed, explicitly dated
  C++/C#/Python reference rows. Keep compute versus full-output metrics separate,
  raw samples/checksums and timeouts visible, and rebaseline references when
  workload/toolchain policy changes; do not silently defer all comparison until
  the three passes end. See `tests/performance/mandelbrot_compute/progress.ps1`.

- [ ] Requalify the three heavy passes' budgets after the IDE/tooling/SDK/v1
  correctness gates, before beta/RC. Keep the existing backend and v1 gate
  identities intact; this final checkpoint does not defer the three passes or
  create a fourth heavy optimization package. Fix severe performance defects
  when discovered rather than postponing them to this checkpoint.
  Profile semantic analysis, Typed/Lowered WIR, C++ emission/build/runtime,
  VM, std/Unicode, native bridges, and SDK hosts. Measure compiler-stage time,
  cold/warm/incremental builds, generated/binary size, startup/runtime, peak
  memory, allocation/copy volume, and intrusive retain/release traffic.
  Include the playground Mandelbrot workload and matching C++ baselines with
  separate compute-only, formatting-only, and end-to-end measurements; audit
  Debug/Release flag forwarding and growing StringBuilder append behavior.
  Replace the string-wrapper-only builder implementation with amortized-linear
  capacity reuse, direct append/numeric formatting, and explicit finalization
  costs; qualify ordinary string `+=` and aliased/self-append correctness too.
  The compute-only Mandelbrot audit also finds a 35.03x native `-O2` kernel gap
  versus equivalent C++ and heap-backed storage for inner-loop scalar `xtemp`.
  Prioritize non-escaping scalar stack/SSA promotion, plain temporaries, direct
  CFG emission, and proven-load/conversion simplification. Reuse the versioned
  `tests/performance/mandelbrot_compute` checksum-qualified benchmark; see
  `docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md` for baseline measurements and limits.
  Establish and enforce numeric performance budgets without weakening language
  semantics or ownership correctness. See `docs/WIO_PERFORMANCE.md`, section 8.

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
