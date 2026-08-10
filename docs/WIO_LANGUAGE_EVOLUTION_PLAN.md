# Wio Language Evolution Plan

This document records candidate language and runtime work after Wio `v0.10.0`.
It is a design plan, not a normative specification. Syntax and semantics marked
as proposed remain open until they are accepted and moved into a versioned
language specification.

The plan has two purposes:

- preserve the product and language decisions discussed while building Wio;
- turn those decisions into reviewable slices instead of disconnected syntax
  experiments.

The active implementation order remains in [`../TODOLIST.md`](../TODOLIST.md).

Implementation status after the P1 foundation sprint: typed attributes,
conflict groups, runtime attribute reflection, ordinary-call pipelines,
Option/Result/array matching, sequential applications, and stack-resident
systems are implemented and frozen by the 0.11 delta specification. Resource
injection, explicit/fixed schedules, parallel conflict analysis, controlled
derives, and attribute migration tooling remain proposals in this document.
The 0.11 freeze also includes value-capturing lambdas and the hot shared-task
async/coroutine model; generators, true async I/O, and automatic continuation
affinity remain future work.

---

## 1. Product Direction

The proposed primary profile is:

> Wio is a native application, tooling, and game language with a C++ ecosystem
> bridge and a data-oriented component model.

This profile does not exclude services, libraries, SDK-hosted modules, or
systems work. It does establish an order of priority:

1. native desktop applications and developer tools;
2. games, simulations, and real-time applications;
3. embeddable native modules and SDK consumers;
4. services and general-purpose software where the same runtime model fits.

Language features should strengthen this profile without turning Wio into a
collection of unrelated DSLs.

The implementation dependency order is:

1. typed, user-extensible attributes and their source migration;
2. the language-coherence/specification pass that uses the new attribute
   classification;
3. the application/system model, built on the finalized metadata, reflection,
   native-callback, and threading contracts.

Application/system implementation followed the stabilized attribute foundation;
future scheduler/resource layers must continue to preserve that dependency.

---

## 2. Application and System Model

### 2.1 Goals

The application model should own more than a hidden `main` plus `while` loop.
It should define:

- executable lifetime;
- stack-resident application and system state;
- deterministic startup, update, and shutdown;
- fixed and variable time steps;
- explicit resource access;
- main-thread affinity;
- testable exit behavior;
- a path to compile-time validated parallel scheduling.

An executable defines either an ordinary `Entry` function or one root
`application`, never both as active entrypoints.

### 2.2 Simple application

Proposed surface:

```wio
application Editor {
    on start {
        Console::WriteLine("Editor started");
    }

    on update(frame: view FrameContext) {
        if frame.Index() >= 100 {
            self.Exit(0);
        }
    }

    on close(reason: view CloseReason) {
        Console::WriteLine($"Editor closed: {reason}");
    }
}
```

When no explicit schedule exists, the compiler/runtime supplies the default
start-update-close loop. Inline handlers are lowered to ordinary checked Wio
functions; they do not form a separate expression language.

A handler may instead bind an existing function:

```wio
application Editor {
    on start: StartEditor;
    on update: UpdateEditor;

    on close(reason: view CloseReason) {
        SaveSettings();
    }
}
```

Both forms must share signature checking, error rules, reflection, generated
documentation, and debugging behavior.

### 2.3 State and resources

Application-local fields remain ordinary state. A `resource` is application-
owned state that may be injected into scheduled handlers:

```wio
application Editor {
    resource settings: Settings;
    resource documents: DocumentStore;

    mut frame_count: u64;
}
```

Resources are not hidden globals. The application runner owns them, and test
harnesses can substitute or inspect them.

### 2.4 Systems

A `system` is proposed as a schedule-aware specialization of Wio's component
model, not a heap object hierarchy:

```wio
system AutosaveSystem {
    mut elapsed: f64;

    on start {
        self.elapsed = 0.0;
    }

    on update(
        frame: view FrameContext,
        documents: ref DocumentStore
    ) {
        self.elapsed += frame.DeltaSeconds();
        if self.elapsed >= 30.0 {
            documents.SaveSnapshot();
            self.elapsed = 0.0;
        }
    }

    on close {
        Console::WriteLine("Autosave stopped");
    }
}
```

The intended lowering is:

- stack-resident component-like state;
- ordinary handler functions;
- compile-time lifecycle and scheduler metadata;
- no required virtual dispatch or per-system heap allocation;
- compatibility with extensions, reflection, default initialization, and
  native component rules where meaningful.

Applications explicitly own system instances:

```wio
application Editor {
    resource documents: DocumentStore;

    system autosave: AutosaveSystem;
    system renderer: RendererSystem;
}
```

### 2.5 Scheduling

Large applications may replace the default loop with a deterministic schedule:

```wio
schedule {
    stage input on main {
        run input_system.poll;
    }

    fixed stage simulation at 60hz after input {
        run physics.fixed;
        run gameplay.fixed;
    }

    stage update after simulation {
        run documents.update;
        run self.update;
    }

    stage render after update on main {
        run renderer.render;
    }
}
```

Explicit handler selection such as `run physics.fixed` is preferred over
choosing a handler implicitly from the stage name. Schedule dependencies form
a directed acyclic graph; cycles are compile-time errors. Within an ordinary
stage, declaration order is deterministic unless an explicit parallel group
is used.

Small scheduled actions may use an inline body:

```wio
stage update {
    run documents.update;

    run(frame: view FrameContext) {
        if self.documents.ShouldAutosave() {
            self.documents.Save();
        }
    }
}
```

### 2.6 Resource access and parallel work

Handler parameters declare scheduler-visible access:

```wio
on fixed(
    frame: view FixedFrameContext,
    input: view InputState,
    world: ref World
) {
    world.Step(input, frame.Step());
}
```

- `view T` is shared read access;
- `ref T` is exclusive write access;
- two readers may run together;
- a writer conflicts with every reader or writer of the same resource.

The initial scheduler should remain sequential. Parallel execution is an
explicit later opt-in and must be validated from these access declarations:

```wio
stage background after update {
    parallel {
        run metrics.collect;
        run search_index.refresh;
    }
}
```

Main-thread handlers and stages are never dispatched to workers. Automatic
parallel scheduling should be considered only after the explicit model is
stable and measurable.

### 2.7 Exit and shutdown

Inside an application handler, `self.Exit(code)` requests orderly shutdown.
Systems receive an explicit `ApplicationControl` resource instead of relying
on ambient global state:

```wio
on poll(app: ref ApplicationControl, window: ref Window) {
    if window.ShouldClose() {
        app.Exit(0);
    }
}
```

The proposed shutdown contract is:

1. record the first exit request and code;
2. allow the currently executing handler/safe stage boundary to complete;
3. begin no new frame;
4. close successfully started systems in reverse startup order;
5. invoke application `on close` exactly once;
6. destroy resources in reverse construction order;
7. return the exit code to the host.

Startup failure closes only the systems that successfully started. Lifecycle
handlers may return no value or the accepted unit-success Result alias
(`UnitResult` is the accepted spelling); an unhandled lifecycle error
follows the same orderly shutdown path with a failure reason.

### 2.8 Runtime boundary

Proposed core/contextual syntax:

- `application`, `system`, `resource`;
- `on`, `schedule`, `stage`, `run`, `after`;
- `fixed`, `parallel`, and `on main` scheduling clauses.

Proposed std/runtime facilities:

- frame/fixed-frame/start/close contexts;
- clocks, timers, frame limiting, and cancellation;
- event queues and event-loop host integration;
- thread pool and conflict-checked scheduler;
- headless runner, fake clock, and deterministic test harness;
- Raylib, SDL, OpenGL, Vulkan, and later Metal/platform adapters.

`every`, `during`, and `wait` should remain runtime/async experiments rather
than core keywords until their semantics are proven. `after` belongs to the
schedule grammar because it defines a static dependency edge.

### 2.9 Delivery slices

1. Sequential application runner, inline/bound lifecycle handlers, exit, and
   reverse shutdown.
2. Stack-resident systems, resources, explicit schedule, fixed stages,
   main-thread affinity, and headless tests.
3. Resource access graph, diagnostics, typed host/event integration, and
   deterministic scheduler inspection.
4. Thread pool, explicit parallel groups, cancellation, native callback/thread
   containment, and performance validation.

---

## 3. Attribute Syntax Modernization

The attribute overhaul is a prerequisite for the application/system runtime,
not merely a spelling cleanup. Wio should have typed user-defined attributes
that are actively consumed by reflection, tooling, serialization, native
interop, validation, derives, and later scheduler/host integrations.

The accepted design direction removes annotation sigils and bracket wrappers.
A declaration uses a postfix `with` clause:

```wio
fn AuditScore(metrics: view AuditMetrics) -> f64
    with native,
         cpp::header("audit_metrics.h"),
         cpp::name("audit_score");
```

The clause always appears after the declaration signature and before its body
or terminating semicolon:

```wio
fn LoadConfiguration(path: string) -> Result<Configuration>
    with telemetry::trace("configuration.load")
{
    // Ordinary checked Wio body.
}
```

The same attachment grammar applies to types, fields, parameters, generic
parameters, enum cases, methods, extension members, and later application
handlers:

```wio
component User with derive::json, derive::hash {
    id: u64;
    display_name: string with json::name("displayName");
    password_hash: string with json::skip, inspect::secret;
}

fn Connect(
    host: string,
    port: i32 with validate::range(1, 65535)
) -> Result<Connection>;

enum NetworkMode {
    Offline,
    Legacy with deprecated("Use Online instead"),
    Online
}
```

### 3.1 Scoped attributes

Attaching metadata to one declaration and activating inheritable metadata in a
lexical scope are distinct operations:

- postfix `with` attaches attributes to a target;
- `using` activates attributes in the current lexical scope.

The existing `use @CppHeader(...)` behavior becomes:

```wio
using cpp::header("audit_metrics.h");

component AuditMetrics with native, abi::pod {
    file_count: u64;
    warning_count: u64;
}

fn CalculateAuditScore(metrics: view AuditMetrics) -> f64
    with native, cpp::name("audit_calculate_score");
```

A bounded scope is also allowed:

```wio
using cpp::header("raylib.h") {
    component Vector2 with native, abi::pod {
        x: f32;
        y: f32;
    }

    fn DrawText(...) with native;
}
```

Only an attribute declared as `scoped` may be used through `using`. Import
`use` and attribute `using` remain grammatically and semantically separate.

### 3.2 Naming and namespaces

Built-in and library attributes use lowercase names and ordinary realm paths
instead of adding many PascalCase identifiers to one global attribute space:

```wio
with native;
with cpp::name("DrawText");
with abi::pod;
with json::skip;
with thread::main;
with test::ignore("requires a GPU");
```

The initial migration map includes:

- `@Native` -> `with native`;
- `@CppHeader("x.h")` -> `with cpp::header("x.h")`;
- `use @CppHeader("x.h")` -> `using cpp::header("x.h");`;
- `@CppName("Foo")` -> `with cpp::name("Foo")`;
- `@Export` -> an explicit export-family attribute such as `with export::c`;
- `@Deprecated(...)` -> `with deprecated(...)`;
- `@From` -> a conversion-family attribute or dedicated conversion syntax;
- `@MainThread` -> `with thread::main`.

Not every existing compiler annotation must survive as an attribute. Generic
constraints should prefer `where`; specialization and other core semantic
operations should be evaluated for dedicated syntax during the coherence pass.

### 3.3 User-defined attributes

An attribute is a typed compile-time declaration:

```wio
realm http;

attribute route(
    method: HttpMethod,
    path: string
)
    for fn
    retain runtime
    repeatable;
```

It is used through its ordinary realm path:

```wio
fn GetUser(id: u64) -> Result<User>
    with http::route(Get, "/users/{id}");
```

Attribute declarations must be able to specify:

- allowed targets;
- typed positional and named parameters with optional defaults;
- compile-time constant requirements;
- source/compile/runtime retention;
- repeatability;
- inheritance;
- scoped activation eligibility;
- exclusivity/conflict groups;
- reflection visibility.

The exact declaration grammar remains to be frozen, but these capabilities are
part of the required model rather than optional follow-up polish.

### 3.4 Active behavior without unrestricted macros

The attribute system grows in controlled layers:

1. Typed metadata available to compiler services, reflection, documentation,
   serializers, binders, tests, and application libraries.
2. Compile-time validation through a deterministic metadata API that can emit
   structured diagnostics.
3. Controlled derive/code-generation processors that produce declarations
   through a checked compiler API.

The first version does not permit unrestricted token or AST mutation. Such a
macro system would make compilation order-sensitive, weaken diagnostics,
complicate the LSP, and create build/security problems before the compile-time
execution model is ready.

Ordinary attribute list order is non-semantic. If ordered behavior is needed,
it must be represented explicitly by a typed pipeline attribute rather than by
silently executing `with A, B` differently from `with B, A`.

### 3.5 Compiler and tooling integration

Before stabilization, implement and specify:

- one typed attribute AST shared by built-ins and user declarations;
- target checking, constant argument evaluation, duplication, conflicts, and
  scope inheritance diagnostics;
- compile-time and runtime reflection queries;
- formatter layout for short and multiline `with` clauses;
- LSP completion, hover, target filtering, rename, and navigation;
- generated binding/importer output using the new spelling;
- documentation rendering and attribute search;
- deterministic validation/derive processor discovery and execution;
- edition-aware deprecation and automated source migration.

Migration order:

1. introduce the typed attribute model and parse both legacy and new forms;
2. migrate built-in attributes to namespaced definitions without changing ABI;
3. make formatter, docs, and generators emit `with`/`using` syntax;
4. add `wio migrate attributes` with a check/diff mode;
5. warn on `@Attribute(...)` for one compatibility window;
6. remove the legacy spelling only at an edition/spec boundary;
7. begin application/system implementation on the stabilized attribute API.

---

## 4. Language Coherence Work

The next language phase should reduce irregularity before adding many more
features:

- reconcile the draft, versioned specifications, freeze snapshot, tests, and
  shipped compiler behavior;
- establish one proposal/RFC and stabilization process;
- normalize attributes, constraints, specialization, conversion, extension,
  and native declaration spelling;
- complete pattern matching with payload enums, Option/Result destructuring,
  guards, exhaustiveness, and ownership/reference binding rules;
- complete associated types, constrained extensions, generic diagnostics, and
  the remaining pack/meta operations;
- define serialization traits shared by JSON and later binary/config formats;
- keep evaluation order, default initialization, destruction, and reference
  lifetime rules normative and test-backed.

---

## 5. Standard Library and Runtime Direction

Priority foundations:

- complete UTF-8/Unicode text, builders, normalization, case folding, and safe
  slicing/iteration;
- production JSON with exact integers, streaming, limits, deterministic
  writing, Pointer/Patch, and generic encode/decode;
- stable serialization traits plus binary, Base64/hex, CSV, TOML/INI-oriented
  configuration support;
- networking, TLS/HTTP, URI, WebSocket, timeouts, and cancellation;
- coherent threads, atomics, channels, tasks, futures, async I/O, and
  structured concurrency;
- OS/application services including processes, watchers, dialogs, clipboard,
  notifications, and standard user directories;
- harden time, secure random, hashing, regex, logging, UUID, SemVer, numeric,
  encoding, and stream contracts across platforms.

Application scheduling must build on the shared concurrency and ownership
model rather than inventing a second threading runtime.

---

## 6. Tooling and Ecosystem Direction

Language maturity also requires:

- canonical formatter and linter;
- production LSP and compiler-backed refactoring;
- stable diagnostic codes, structured output, fix-its, and generated-C++
  source mapping;
- package/dependency manager with lockfiles, checksums, native assets, and
  reproducible/offline restore;
- official Raylib, SDL, ImGui, SQLite, HTTP/TLS, OpenGL, Vulkan, and eventually
  Metal packages/adapters;
- ABI conformance matrices across compilers, operating systems, architectures,
  calling conventions, and static/shared builds;
- incremental module/type/codegen/native-object caching;
- reproducible releases, signed artifacts, provenance, install matrices, and
  nightly/preview/stable channels;
- a real-world release-gate portfolio covering CLI, desktop GUI, game/real-
  time loop, native library, SDK host, networking service, and package use.

---

## 7. Decisions Still To Freeze

The following choices should be frozen before their implementation slice:

1. Attribute application direction is accepted: postfix `with`, scoped
   `using`, lowercase realm paths, and no annotation sigil/bracket wrapper.
   Exact custom declaration, retention, validation, and derive APIs remain to
   be frozen before coding begins.
2. Lifecycle spelling: prefer `on start {}` plus `on start: Function;`.
3. Exit access: prefer `self.Exit()` in application bodies and explicit
   `ApplicationControl` injection in systems.
4. System representation: stack component plus compile-time descriptor.
5. Schedule behavior: sequential by default, explicit validated parallelism.
6. Handler selection: explicit `run instance.handler`.
7. Resource model: injection and conflict analysis through `view`/`ref`.
8. Default loop: generated when no explicit schedule is supplied.
9. Events: typed std event queues first, dedicated syntax only after evidence.
10. Lifecycle errors: no return value or the standard unit-success Result
    alias, using the existing structured `ResultError` model.

Once accepted, each decision should move into the appropriate versioned
specification before its implementation is called stable.
