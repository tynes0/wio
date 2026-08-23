# Wio Post-v1 Roadmap

This document holds valuable work that does not block `v1.0.0`. It prevents
the v1 release train from growing without limit while preserving ideas for
later releases.

The roadmap is thematic rather than a binding calendar. A feature may move
earlier only through the scope rule in
[`WIO_V1_RELEASE_PLAN.md`](./WIO_V1_RELEASE_PLAN.md); after v1, minor-release
compatibility rules decide whether it fits a `v1.x` release or needs `v2`.

---

## 1. v1.1 - Language and Metadata Ergonomics

Candidate focus:

- extension properties and static extension functions;
- import-scoped extension visibility and richer conflict selection;
- additional processor libraries and policy ergonomics beyond the frozen
  metadata/validation/derive/behavioral attribute system;
- advanced attribute-aware documentation and ecosystem integrations;
- deprecation attributes, editions, and compiler-assisted migrations;
- further generic diagnostics without changing frozen compatibility rules;
- richer `wio doc` output and stable API URLs.

Unrestricted AST/token macros and arbitrary call-site rewriting are not implied
by typed behavioral attributes. Any effect system must remain ordered,
inspectable, type-checked, and ABI-safe.

---

## 2. v1.2 - Data, Serialization, Networking, and OS Services

Candidate focus:

- streaming JSON readers/writers, RFC 6902 Patch, and schema hooks;
- generic serialization derives, streaming codecs, unknown-field evolution,
  TOML, archives, decimal, and database primitives;
- time-zone database and calendar types;
- native completion backends, TLS, HTTP client/server primitives, multipart,
  WebSocket, proxies, and certificate policy;
- process signal/event subscriptions and native filesystem watchers;
- clipboard, notifications, dialogs, user/config/cache directories, dynamic
  libraries, and capability queries;
- expanded official packages such as ImGui, SQLite, and HTTP/TLS.

These modules must earn stable status independently. Being present in `std`
does not automatically make a backend or platform contract stable.

---

## 3. v1.3 - Async, Scheduling, and Real-time Systems

Candidate focus:

- async generators and streams;
- suspension rules that can safely interact with borrowed component/extension
  receivers;
- native I/O completion backends and deeper event-loop adapters;
- application scheduler resources, explicit dependency graphs, conflict-aware
  parallel stages, deterministic fixed-step execution, and profiling hooks;
- advanced task inspection, tracing, leak detection, and long-running soak
  qualification.

The everyday surface should remain centered on `Task<T>`, `await`, structured
`spawn`, explicit blocking, cancellation, and application main-executor
integration. Advanced backends must not make basic usage more complicated.

---

## 4. v1.x Tooling and Ecosystem Track

These can ship incrementally when compatible:

- interactive REPL and scratch projects;
- advanced incremental and parallel compilation with explainable cache state;
- test watch mode, sharding, snapshots, coverage, benchmarks, and richer CI
  outputs;
- package-aware editor navigation, restore state, license and vulnerability
  information;
- debugger support and generated-C++/native mixed-mode source mapping;
- refactors such as extract, organize imports, change signature, move symbol,
  generate implementation, and Option/Result wrapping;
- searchable package portal integrated into `wio-web`;
- larger maintained template and real-application portfolios;
- package registry governance, namespace ownership, authentication, and
  malicious-package defenses.

The existing `wio-web` repository remains the documentation frontend. Work in
this track is site version selection, search, package metadata, API docs,
migration views, and release compatibility tables—not recreating the site.

---

## 5. v2 Language Research

The following changes are intentionally outside the v1 compatibility promise
and may require a new language edition or major version:

- const generic value kinds beyond integers, `string`, and `text`;
- pack `Take`, `Drop`, `Zip`, `MapTypes`, filter/fold, value transforms,
  concatenation, and compile-time iteration;
- associated types, explicit variance, richer boolean constraints, and default
  interface implementations;
- payload enums and a broader algebraic-data-type design;
- nested/component/rest patterns beyond the frozen v1 matching model;
- pattern specialization beyond the deterministic v1 partial model;
- generalized implicit user-defined conversions;
- user-defined `operator->`;
- a broader compile-time metaprogramming model beyond the frozen meta wave;
- unrestricted token/AST macros or call-site rewriting beyond the bounded,
  typed Wio 0.15 behavioral processor model;
- an independent Wio IR, interpreter, or LLVM backend, but only if measurements
  show a concrete diagnostics, tooling, compile-time, or portability benefit.

These are research candidates, not assumed commitments. Each needs a proposal,
compatibility analysis, implementation experiment, and measurable acceptance
criteria before entering a release plan.

---

## 6. Ongoing Quality Work

Some work does not belong to one feature release and continues throughout v1.x:

- benchmark and compile-time regression tracking;
- generated-C++ size reduction;
- runtime ownership and allocation optimization;
- TSan, leak checks, fuzzing, native-boundary stress, and soak tests;
- architecture/compiler/platform expansion;
- dependency provenance and reproducibility verification;
- security guidance for privileged native code and untrusted package hooks;
- RFC templates, stabilization checklists, feature gates, and removal policy.

Optimization must preserve the v1 semantics. Backend replacement remains a
measured product decision, not an automatic milestone.

---

## 7. Moving an Idea Into a Release

When a new idea appears:

1. record the problem and real-world use case;
2. decide whether it fixes a v1 contract or expands the product surface;
3. place it in the active pre-v1 release only if it passes that plan's scope
   rule; otherwise place it in the appropriate post-v1 theme;
4. define syntax/API, compatibility, tests, docs, editor, SDK, and platform
   impact before implementation;
5. move it to `TODOLIST.md` when scheduled and to `COMPLETED.md` only after its
   acceptance evidence is green.

This process is intentionally flexible: the roadmap can grow, but release gates
remain explicit and reviewable.
