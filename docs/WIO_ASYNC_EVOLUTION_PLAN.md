# Wio Async and Multithreading Evolution Plan

Status: post-0.12 evolution plan; non-normative until remaining slices move into
a versioned specification.

Progress: correctness, ergonomics, and structured-work slices are frozen in
Wio `v0.12.0`. This includes the runtime `Scope`, language
`spawn`/`async scope`, lexical cancellation/deadlines, explicit `detach`, and a
small homogeneous `Select<T>` surface. The owner/main executor slice is also
implemented: application runners bind and drain it at deterministic lifecycle
stages, `await main` performs a direct checked handoff, and headless hosts can
bind/drain the same queue explicitly. Linux, packaged-toolchain, sanitizer,
and real-app qualification passed for the v0.12.0 release.
The first platform-capability slice is implemented as a dedicated bounded I/O
executor plus Result-preserving asynchronous filesystem and process run/
capture operations, plus a cancellable portable first-change file watcher.
DNS/connect and existing TCP/UDP handles now have leased asynchronous I/O;
close interrupts work without freeing live operation state. Native completion-
port/watcher/process-pipe backends, TLS, and process signal/event subscription
remain open. Owned processes now expose separate streaming stdin/stdout/stderr,
async wait, termination, deterministic close/reap, and live-state diagnostics.
`Listener.AcceptAsync` is implemented through the dedicated bounded I/O executor
with a pre-scheduling native lease and owned `Result<Socket>` handoff; close
interrupts the readiness wait without freeing in-flight listener state.
Executor-qualified structured work is implemented as `spawn worker expression`
and `spawn blocking expression`. It retains ordinary lexical scope ownership,
returns the same `Task<T>`, and applies the existing `Send` capture check at the
source boundary.

The 0.11 contract remains in [`WIO_ASYNC_MODEL.md`](./WIO_ASYNC_MODEL.md). This
document records what comes next without reopening that frozen foundation.

## 1. Product rule

Wio's concurrency model must be powerful in real applications and small in the
user's head. Runtime complexity belongs behind a compact, explicit source
model. New primitives are accepted only when they remove common boilerplate or
make an unsafe operation visible.

The intended everyday vocabulary is:

- `async fn` creates asynchronous work;
- `await` suspends an async function without blocking its thread;
- `Task<T>` is the friendly shared-result handle;
- `spawn` explicitly schedules independent child work;
- `async scope` owns child-task lifetime and cancellation;
- `Poll` observes a task from a frame/update loop without blocking;
- `Block` is the deliberately loud synchronous boundary;
- deadlines/timeouts and cancellation are structured, typed operations;
- ordinary code uses the default scheduler; executor choice appears only when
  affinity or blocking behavior actually matters.

`thread`, coroutine frames, worker queues, timers, and platform completion
ports remain available implementation concepts, but they must not all become
mandatory everyday source concepts.

## 2. Frozen 0.11 foundation

The following decisions are retained:

- `async fn (...) -> T` produces one hot shared task and `await` yields `T`;
- execution begins on the caller and proceeds to the first suspension;
- copying a task handle does not start another execution;
- multiple awaiters observe the same completion;
- continuation thread affinity is never implicit;
- object async methods retain `self` through completion;
- suspension through unproven `ref`/`view`, component, or extension lifetimes
  is rejected;
- cancellation is cooperative;
- `All`, `Any`, `Race`, task groups, recoverable timeout, and explicit owner
  dispatch remain supported;
- blocking is permitted only through an API that says it blocks.

Post-0.11 work should normally be additive. A change to these meanings requires
a new versioned language contract and migration notes.

## 3. Canonical task surface

`Task<T>` should become the public spelling while `coroutine<T>` remains a
compatibility alias or lower-level compiler spelling:

```wio
async fn LoadProfile(id: u64) -> std::Result<Profile> {
    return await profiles::Load(id);
}

let task: Task<std::Result<Profile>> = LoadProfile(42u64);
let result = await task;
```

The common state operations should be discoverable on the handle:

```wio
task.IsReady();
task.IsCancelled();
task.IsFaulted();
task.Cancel();
```

The source model must distinguish four forms clearly:

```wio
let value = await task;          // suspend; do not block a thread
let value = task.Block();        // synchronously block the caller
let state = task.Poll();         // never block; inspect frame-loop state
let value = await task.Within(2s); // async deadline result
```

`wait` should not become an ambiguous synonym. In async code the operation is
`await`; at a synchronous boundary the deliberately explicit operation is
`Block`; in a frame loop it is `Poll`.

`Poll` needs a typed result capable of distinguishing all states:

```wio
match (task.Poll()) {
    Pending(): {}
    Ready(value): Use(value);
    Failed(error): Report(error);
    Cancelled(): RemoveRequest();
}
```

Polling never consumes or restarts the task. A later awaiter may still observe
the same stored completion.

## 4. Structured work

Independent child work should use one simple construct:

```wio
async fn LoadSession() -> std::Result<Session> {
    async scope {
        let user = spawn LoadUser();
        let settings = spawn LoadSettings();

        return Session::Create(await user, await settings);
    }
}
```

An async scope owns every child started within it. Normal exit joins required
children; error, cancellation, or abandoned exit cancels unfinished children
and waits for cleanup. Detached work remains possible but must be an explicit
exception:

```wio
detach SendBestEffortTelemetry();
```

Deadlines and cancellation flow through the scope instead of being manually
threaded through every intermediate function:

```wio
async scope with deadline(2s) {
    let metadata = spawn LoadMetadata();
    let content = spawn LoadContent();
    return BuildDocument(await metadata, await content);
}
```

A compact `select`/race construct may be added for genuinely competing events,
but it should build on the same task, cancellation, and deadline semantics
rather than introduce another task category.

## 5. Executor model

Most code uses no executor annotation. The runtime chooses the ordinary async
scheduler. Four roles are nevertheless required internally and at explicit
boundaries:

1. continuation/CPU worker pool for short non-blocking work;
2. separately bounded blocking pool for legacy/native blocking calls;
3. owner/main executor for window, render, and UI affinity;
4. platform I/O executor for true asynchronous file, socket, process, and
   watcher operations.

Explicit use should stay narrow:

```wio
let parsed = spawn worker ParseLargeDocument(bytes);
let legacy = spawn blocking ReadLegacyDatabase();

let image = await DecodeImage(bytes);
await main;
window.Upload(image);
```

`RunBlocking` must move to the blocking pool. Blocking work must never consume
all continuation workers and starve the tasks needed to complete it. Dedicated
`thread` remains an expert tool for long-lived OS/native loops, not the default
way to start async work.

Surface shipped in Wio 0.12:

```wio
let decoded = await DecodeImage(bytes);
await main;
window.Upload(decoded);
```

`await main` queues the suspended caller itself, rather than awaiting a nested
task whose completion could legally resume the caller on a worker. Application
code gets automatic binding/draining; custom and headless hosts use
`BindMain()` and `DrainMain()`.

## 6. Application and game-loop integration

Frame/update loops must never need `Block`:

```wio
system AssetLoader {
    mut load: Task<std::Result<Asset>>?;

    on start {
        self.load = spawn assets::Load("ui/theme");
    }

    on update {
        match (self.load.Poll()) {
            Pending(): {}
            Ready(Ok(asset)): resources.Set(asset);
            Ready(Error(error)): events.Emit(AssetLoadFailed(error));
            Cancelled(): {}
            Failed(fault): application.Panic(fault);
        }
    }
}
```

The application runner owns the main executor and drains it at a deterministic
stage. Main-thread handoff should therefore become `await main` or an equally
small checked operation instead of requiring manual dispatcher plumbing in
ordinary application code. Headless tests must be able to drive the same
executor deterministically.

This slice shipped in Wio 0.12. The generated runner drains
after start, before and after every update, immediately before close, and after
close. `Dispatcher` remains useful for owner-owned arbitrary callbacks, while
coroutine continuation affinity uses `await main`.

## 7. Thread-safety contract

Lifetime safety is not data-race safety. Before general parallel application
scheduling, Wio needs compiler-visible equivalents of `Send` and `Sync`:

- a value sent to a worker must be safe to move/copy across threads;
- a shared object used concurrently must expose synchronized or immutable
  access;
- closure captures are checked when crossing an executor boundary;
- raw `ref`/`view` captures cannot escape into unproven asynchronous work;
- mutable application resources declare access so the scheduler can reject
  conflicting parallel systems.

Most safe components should derive the applicable traits automatically.
Unsafe/native overrides must be explicit, narrow, and reviewable. Shared object
identity alone never implies synchronized fields.

## 8. Cancellation, failure, and shutdown

Cancellation remains cooperative but becomes structured:

- a scope owns a cancellation source;
- child tasks inherit its token and deadline;
- cancellation-aware timers register directly with the scheduler instead of
  polling small sleep slices;
- blocking/native adapters receive a token when the host API supports one;
- timeout is represented as an expected typed outcome, while invariant faults
  remain faults.

Recoverable work should normally return `Task<Result<T>>`:

```wio
async fn Save() -> std::Result<std::Unit> {
    // Expected filesystem/network errors remain values.
}
```

Process shutdown should cancel detached timers/tasks, drain cleanup, and join
runtime workers. A distant sleep must neither delay exit nor resume ordinary
user work as if its deadline elapsed successfully.

## 9. Real-world acceptance scenarios

Every slice must prove itself in representative programs, not only isolated
unit tests:

- console: concurrently load configuration and data, then return an exit code;
- desktop: background file/index work, progress, cancellation, and main-thread
  UI completion without freezing the event loop;
- game: asynchronous asset streaming with deterministic per-frame polling and
  main/render affinity;
- server/tool: thousands of timers/socket operations without one thread per
  operation;
- native interop: bounded blocking calls, callback completion, cancellation,
  and clean module/runtime shutdown;
- failure tests: worker starvation, task-tree cancellation, executor shutdown,
  cross-thread capture rejection, data-race rejection, and deterministic
  virtual-time scheduling.

## 10. Implementation order

1. Correctness (implemented and release-qualified in 0.12):
   `Send`/`Sync`-style traits, cross-executor capture analysis, separate
   blocking pool, cancellation-aware timers, and explicit shutdown.
2. Ergonomics (shipped in 0.12): `Task<T>` facade, member state
   operations, typed non-blocking `Poll`, explicit `Block`, and recoverable
   `Within`.
3. Structure (shipped in 0.12): heterogeneous owning `Scope`, language
   `spawn`/`async scope`, nested ownership, return/fallthrough join, sibling
   cancellation, millisecond deadlines, executor-qualified `spawn worker` and
   `spawn blocking`, explicit `detach`, and `Select<T>`.
   Typed-duration sugar and ambient tokens for native adapters remain additive.
4. Application integration (shipped in 0.12): owned main executor,
   deterministic drain stage, `await main`, and headless stress tests.
5. Platform capability (portable 0.12 slice shipped): dedicated
   bounded I/O executor and Result-preserving async file plus process run/
   capture operations, a portable cancellable watcher, DNS/connect, and leased
   TCP/UDP data I/O plus ownership-safe async accept. Add native completion-
   port/watcher/process-pipe backends, TLS, and process signal/event adapters.
6. Streaming: the bounded `AsyncChannel<T>` backpressure primitive is an
   shipped in 0.12; async iterators/generators and source `yield` follow
   only after its cancellation, close, and fairness semantics are frozen.

Each step is frozen only after Windows, Linux, packaged-toolchain, sanitizer,
stress, and at least one real application qualification pass.

## 11. Simplicity guardrails

Do not add:

- multiple source-level task types for the same ownership behavior;
- implicit thread switches;
- a generic `wait` whose blocking behavior is unclear;
- executor annotations on ordinary async functions by default;
- automatic parallelism without access/conflict proof;
- cancellation that forcibly kills native threads;
- convenience APIs that hide detached lifetime or swallowed failure.

The target is not the largest concurrency API. It is the smallest model that
can express console, desktop, game, tooling, server, and native-host workflows
without hidden blocking or unsafe cross-thread state.
