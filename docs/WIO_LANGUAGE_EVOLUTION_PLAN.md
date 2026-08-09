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
handlers may return `void` or `Result<void, AppError>`; an unhandled lifecycle
error follows the same orderly shutdown path with a failure reason.

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

The current `@Attribute(...)` spelling is functional but does not fit the
preferred long-term Wio surface. This is a syntax design problem; attribute
semantics, targets, and lowering should not change merely because the spelling
changes.

The leading candidate is a single-bracket attribute list:

```wio
[Native, CppHeader("audit_metrics.h"), CppName("audit_score")]
fn AuditScore(metrics: view AuditMetrics) -> f64;
```

Attributes may also be stacked when that reads better:

```wio
[Native]
[CppHeader("audit_metrics.h")]
[CppName("audit_score")]
fn AuditScore(metrics: view AuditMetrics) -> f64;
```

The existing scope/module application form becomes:

```wio
use [CppHeader("audit_metrics.h")];
```

This preserves the existing meaning: the header metadata applies at the use
scope, while declaration-level attributes continue to attach metadata to one
mapping.

Other existing forms translate mechanically:

```wio
[Apply(T: Hashable)]
fn HashValue<T>(value: view T) -> u64;

[Specialize(i32)]
component NativeValue<T> {
    value: T;
}

enum ErrorKind {
    [From]
    Io,

    [Deprecated("Use Network instead")]
    Socket
}
```

Why this is the leading candidate:

- it removes the decorator-like `@` visual style;
- it supports one or many attributes without repeated sigils;
- it remains visually distinct from modifiers and ordinary calls;
- declaration position makes it distinguishable from array expressions;
- `use [Attribute(...)]` remains compact and mechanically consistent;
- it leaves room for user-defined and tooling-only metadata.

The grammar must reserve attribute lists only in unambiguous attribute
positions: immediately before supported declarations/members or after `use`.
An arbitrary expression statement beginning with `[` must remain an array or
index-related expression rather than being guessed as metadata.

Before stabilization, specify:

- valid targets for every built-in attribute;
- whether an attribute is repeatable;
- positional and named argument rules;
- constant-expression requirements for arguments;
- duplicate/conflicting attribute diagnostics;
- scope inheritance and declaration override behavior;
- reflection visibility and custom attribute declarations;
- formatter ordering and grouping;
- generated binding/importer output;
- deprecation and automated migration from `@...` syntax.

Suggested migration:

1. parse both forms behind one unchanged attribute AST;
2. make the formatter and binding generator emit bracket syntax;
3. add `wio migrate attributes` or a general source migration command;
4. warn on the legacy spelling for one compatibility window;
5. remove `@` spelling only at an edition/spec boundary.

Alternatives still available for review are Rust-like `#[Attribute]`, C++-like
`[[Attribute]]`, and keyword-led `meta[Attribute]`. The single-bracket form is
currently preferred because it is the least noisy while retaining a clear
metadata boundary.

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

The following choices should be explicitly accepted before implementation:

1. Attribute spelling: prefer `[Attribute(...)]` over the listed alternatives.
2. Lifecycle spelling: prefer `on start {}` plus `on start: Function;`.
3. Exit access: prefer `self.Exit()` in application bodies and explicit
   `ApplicationControl` injection in systems.
4. System representation: stack component plus compile-time descriptor.
5. Schedule behavior: sequential by default, explicit validated parallelism.
6. Handler selection: explicit `run instance.handler`.
7. Resource model: injection and conflict analysis through `view`/`ref`.
8. Default loop: generated when no explicit schedule is supplied.
9. Events: typed std event queues first, dedicated syntax only after evidence.
10. Lifecycle errors: `void` or common `Result<void, AppError>`.

Once accepted, each decision should move into the appropriate versioned
specification before its implementation is called stable.
