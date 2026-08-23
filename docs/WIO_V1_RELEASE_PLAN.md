# Wio v1 Release Plan

This document is the finite delivery plan from the released `v0.13.0` baseline
to `v1.0.0`. It answers a different question from the active backlog:

- [`../TODOLIST.md`](../TODOLIST.md) records every unfinished item;
- this document records which unfinished items block `v1` and when they are
  expected to land;
- [`WIO_POST_V1_ROADMAP.md`](./WIO_POST_V1_ROADMAP.md) records valuable work
  that does not block `v1`.

Release numbers are planning targets, not promises of calendar dates. A slice
may be split or combined when implementation evidence requires it, but the
`v1` gates in this document may not disappear silently.

---

## 1. Scope Rule

New ideas may be added to a planned release until that release enters code
freeze. An addition belongs in that release when it:

1. fits the release theme;
2. does not invalidate an already frozen language or ABI decision;
3. includes tests, documentation, diagnostics, and migration impact;
4. does not silently move an existing mandatory gate to a later release.

If an idea is useful but fails one of those tests, record it in the post-v1
roadmap instead. This keeps the plan open to good discoveries without making
`v1` an endlessly moving target.

Each pre-v1 release follows this progression:

1. **Open:** compatible ideas may be added.
2. **Scoped:** required outcomes and tests are agreed; additions need an
   explicit scope decision.
3. **Frozen:** only correctness, compatibility, diagnostics, documentation,
   packaging, and security fixes enter.
4. **Released:** unfinished non-blockers move forward explicitly; they are not
   hidden or silently declared complete.

---

## 2. What Actually Blocks v1

The following are mandatory before `v1.0.0`:

- one coherent, versioned language specification matching shipped behavior;
- a stable language/runtime/std surface with no known contradictory contracts;
- full host SDK parity for every host-observable stable v1 feature, or a
  compile-time diagnostic for a value that deliberately cannot cross the ABI;
- frozen ownership, callback, async, application, reflection, reload, and ABI
  rules at native boundaries;
- reproducible Windows, Linux, and macOS release artifacts and install flows;
- stable CLI behavior, machine-readable diagnostics, formatter support, and a
  compiler-owned language-service baseline;
- version alignment across compiler, runtime, std, CLI, SDK, VS Code extension,
  Visual Studio extension, Rider/CLion plugin, documentation, and release
  metadata;
- substantial console, native GUI, SDK-host, library, async/network, and package
  consumer projects passing as release gates;
- migration, compatibility, security, and release-process documentation.

Broad surface expansion is not a v1 gate. A debugger, advanced refactoring,
large official package catalog, async streams, payload enums, full HTTP stack,
and an independent backend can ship after v1.

---

## 3. Planned Release Train

### v0.13.0 - Language Coherence

Status: **released on 2026-08-22**. The 0.13 SDK foundation was intentionally
pulled forward so the compiler, runtime, std, CLI, SDK, editor, documentation,
and release metadata could share one product-version contract. The deeper
nested-value and native-lifetime work remains assigned to the later slices
below.

Goal: turn the work currently accumulated after `v0.12.0` into one internally
consistent language release.

Required scope:

- freeze the first-class `text` primitive, Unicode literals, `string`/`text`
  constants, and corresponding generic arguments;
- finish deterministic generic specialization and extension lookup edge cases;
- freeze the compact typed-attribute surface, including named arguments and
  clear legacy-syntax migration diagnostics;
- reconcile initialization, conversion, matching, enum/flagset, reflection,
  application, async, and native semantics across implementation and docs;
- update the versioned specification and stable/experimental feature tables;
- align the VS Code grammar, snippets, diagnostics, and product version;
- establish the shared editor/version contract used by the later Visual Studio
  and Rider/CLion clients;
- close stale documentation claims such as scalar-only `const` and obsolete
  async executor descriptions.

Exit gate: the changed language corpus passes on Windows and Ubuntu, sanitizer
coverage stays green, and every newly stable construct has a normative rule,
positive test, negative test, and generated-C++ validation where applicable.

### v0.14.0 - Standard Library and SDK Value Parity

Goal: make stable Wio values behave consistently inside Wio and across the host
boundary.

Status: released as `v0.14.0`; Windows, Ubuntu, and sanitizer qualification
passed.

Required scope:

- publish the SDK product version, ABI revision, capability query, and complete
  supported/unsupported type inventory;
- add SDK value parity for `text`, `Option`, `Result`, unit results, tuples,
  arrays, maps, sets, queues, spans/views, and nested combinations;
- expose concrete generic and const-generic descriptors and instantiation data;
- replace runtime surprises for unsupported public exports with compile-time
  diagnostics;
- finish stable std correctness work for Unicode normalization, JSON bounds and
  integer behavior, serialization traits, regex records/safety, numeric/hash/
  random/time vectors, and container invariants;
- add cross-platform conformance vectors for the changed std and SDK surface.

Exit gate: every stable value category has round-trip C ABI and C++ SDK tests,
or a documented compile-time rejection. No exported field path reports a
runtime “not yet supported” error.

### v0.15.0 - Typed and Behavioral Attributes

Goal: replace the parallel built-in/user annotation mechanisms with one
powerful, inspectable attribute system before application hosting expands.

Required scope:

- canonical declaration-leading `[Attribute]` syntax, grouped/stacked lists,
  qualified names, named arguments, and the existing scoped `using` activation;
- one normalized typed model for built-ins and user declarations, including
  target policy, defaults, retention, repetition, inheritance, conflicts, and
  folded structural arguments;
- deterministic validation and checked derive processors without unrestricted
  token or AST mutation;
- separate typed pre, post, finally, and around processor interfaces, including
  entry guards, result mapping, guaranteed exit, explicit ordering, and effect
  declarations;
- user attributes composed from existing attributes, with normalized target,
  requirement, allow-list, conflict, exclusivity, cardinality, and ordering
  constraints;
- declarations for every compiler-known attribute and processor interface in
  `std::attribute`, including native/export and their compatibility rules;
- callback receiver-liveness contracts, including wrappers whose native peer
  has already been destroyed;
- sync/async phase, ownership, cancellation, thread-affinity, and ABI rules for
  every behavioral processor;
- runtime reflection and C++ SDK metadata for normalized applications and
  behavioral pipelines;
- formatter, migration, inspection, documentation, and editor support, with
  all maintained source/generated examples emitting bracket syntax.

Exit gate: built-in attributes no longer use a privileged parallel path;
metadata, validation, derive, and behavioral examples pass on Windows and
Ubuntu; expansions are deterministic and visible to diagnostics/tooling; and
ordinary project source needs neither `@Name(...)` nor postfix `with`.

The normative delivery contract is
[`WIO_ATTRIBUTE_SYSTEM_PLAN.md`](./WIO_ATTRIBUTE_SYSTEM_PLAN.md).

### v0.16.0 - Ownership, Callbacks, Async, and Application Hosting

Goal: freeze how real applications and native hosts own work and state.

Required scope:

- lifetime-safe `ref`/`view` export and callback userdata rules;
- native callback/event-loop entry, panic containment, thread entry, and
  main-thread affinity;
- a clear move-only/native-resource story for the stable boundary;
- host SDK task polling, completion callbacks, cancellation, deadlines, and
  main-executor pumping;
- application/system resources, explicit and fixed schedules, headless
  contexts, deterministic start/update/close, and host-driven lifecycle;
- cancellation-aware process/filesystem/network operations and clean runtime
  shutdown on the supported portable backend.

Exit gate: console, desktop/event-loop, game-loop, service/tool, and native-host
acceptance scenarios prove that waiting is explicit, frame polling never blocks
implicitly, cancellation is observable, and shutdown does not leak work.

### v0.17.0 - Reflection, Reload, ABI, and Platforms

Goal: make metadata and binary integration dependable outside the compiler's
own test process.

Required scope:

- method, parameter, generic, const-generic, attribute, field, and type metadata
  with stable identifiers;
- dynamic get/set/invoke for supported stable values;
- reload generation, stale-handle rejection, state migration policy, and
  capability negotiation;
- independent C/C++ layout and calling-convention probes;
- shared/static/PIC and debug/release qualification;
- Windows, Linux, and macOS coverage, with GCC/Clang/MSVC and x64/arm64 coverage
  where the supported toolchain permits it.

Exit gate: SDK consumers built independently from the compiler agree on layout,
calling convention, ownership, reflection, and reload behavior across the
supported release matrix.

### v0.18.0 - Packages and Release Engineering

Goal: make a Wio dependency installable and a Wio release reproducible.

Required scope:

- `wio add`, `remove`, `update`, and `restore` with lockfiles, path/Git/registry
  dependencies, checksums, an offline cache, platform variants, and deterministic
  resolution;
- the minimum binding-importer support needed by the first package set;
- initial maintained packages for the C ABI helper and representative raylib/
  SDL native application flows; further packages remain additive;
- normalized build inputs, manifests/SBOM, checksums, provenance, signed release
  metadata, upgrade/uninstall testing, and preview/stable channels;
- Windows, Linux, and macOS archives/installers with Unicode/space path and
  non-admin install coverage.

Exit gate: a clean machine can restore, build, test, package, install, upgrade,
and uninstall a pinned project without relying on the source checkout.

### v0.19.0 - Developer Experience and Product Closure

Goal: remove the daily workflow gaps that would make a stable language feel
unfinished.

Required scope:

- canonical deterministic `wio fmt` and baseline `wio lint`;
- stable diagnostic codes, primary/secondary spans, notes, fix-its, and
  terminal/JSON rendering;
- compiler-owned language-service support sufficient for diagnostics,
  completion, hover, navigation, symbols, semantic tokens, formatting, and
  workspace imports;
- release-gate Visual Studio and shared Rider/CLion clients using that same
  language service rather than independent semantic implementations;
- consistent CLI exits, quiet/verbose/color policy, cancellation, help, config
  precedence, and machine output;
- maintained templates for console, library, native library, desktop app,
  service, tests, bindings, and SDK hosts;
- versioned documentation navigation and migration guides on `wio-web`;
- product-version alignment across all shipped repositories and artifacts;
- packaged extension/plugin install, upgrade, uninstall, and compatibility
  checks for VS Code, Visual Studio, Rider, and CLion.

Exit gate: the VS Code extension no longer needs an independent approximation
of Wio semantics for the baseline feature set, and each maintained template is
continuously smoke-tested from an installed toolchain.

### v1.0.0-beta.1 - Feature Complete

All mandatory v1 surface is implemented. Large new stable features stop here.
The beta period is for real-project qualification, migration feedback,
documentation completion, performance baselines, and fixing compatibility
mistakes while changes are still possible.

### v1.0.0-rc.1 - Frozen Candidate

Only release blockers enter: correctness, compatibility, security,
diagnostics, documentation, packaging, and installation fixes. The RC must pass
upgrade/reinstall tests, old-host capability negotiation, the full platform
matrix, and every validation-portfolio project.

More RCs may be cut when evidence requires them; the criteria do not weaken.

### v1.0.0 - Stable

The stable tag is created only after all gates below are green, published
artifacts match their checksums/provenance, the versioned site is live, and the
release notes accurately distinguish stable, experimental, deprecated, and
post-v1 work.

---

## 4. Cross-Release Gates

| Gate | Required by |
| --- | --- |
| Normative language/std contract matches implementation | `v0.13.0` onward |
| Stable value SDK round trips | `v0.14.0` |
| Typed metadata/validation/derive/behavioral attributes | `v0.15.0` |
| Ownership/callback/async/application host acceptance | `v0.16.0` |
| Reflection/reload/ABI/platform matrix | `v0.17.0` |
| Reproducible package and installed-toolchain workflow | `v0.18.0` |
| Formatter, diagnostics, language service, templates, docs alignment | `v0.19.0` |
| Real-world portfolio, compatibility and migration sign-off | beta/RC |
| Signed, checksummed, provenance-backed release artifacts | RC/stable |

Every release also requires:

- changed tests green on every platform the change affects;
- no unresolved P0/P1 correctness or security defect in its frozen scope;
- changelog, specification, reference docs, examples, and editor surface updated;
- compiler/runtime/std/CLI/SDK/editor/site version metadata checked together.

---

## 5. Backlog Routing

The active backlog contains mixed items, so a single item may have a required
v1 core and a post-v1 expansion:

| Backlog area | Pre-v1 routing | Post-v1 remainder |
| --- | --- | --- |
| Language 1, 3, 13, 15, 17 | `v0.13`-`v0.17` | value kinds beyond frozen v1 |
| Language 5, 6, 7, 8, 12, 14 | current behavior hardens pre-v1 | advanced meta, ADTs, extension properties, behavioral interception, parallel scheduler |
| Std 1, 3-6, 8 | correctness and stable contracts in `v0.14`-`v0.15` | streaming codecs, schema/patch expansion, time zones, advanced regex backend, streams |
| Std 7, 9, 10 | portable stable foundations qualify pre-v1 | TLS/HTTP/WebSocket, broad OS GUI services, databases/archives/TOML |
| Native/SDK 1-8 | minimal package ecosystem and complete stable SDK/ABI in `v0.14`-`v0.18` | larger package catalog and importer convenience surface |
| CLI 1-4, 7-12 | stable daily/release workflow in `v0.18`-`v0.19` | richer generated-doc portal features |
| CLI 5-6, 8 | correctness-critical cache/test pieces pre-v1 | REPL and advanced incremental/test UX |
| Editor 1, 3, 5 | baseline in `v0.19` | advanced language-service and templates |
| Editor 2, 4, 6 | not a v1 gate | debugger, package intelligence, advanced refactors |
| Performance 1, 4, 5-7 | release baselines and supported-platform safety pre-v1 | deeper optimization and broader matrices |
| Performance 2, 3, 8 | not a v1 gate | compile/runtime optimization and independent backend research |
| Product 1-5 | compatibility, site integration, governance minimum, and validation portfolio pre-v1 | package portal depth and long-term governance evolution |

The detailed post-v1 destinations are in
[`WIO_POST_V1_ROADMAP.md`](./WIO_POST_V1_ROADMAP.md). Completion state remains
in [`../TODOLIST.md`](../TODOLIST.md), not in this planning document.
