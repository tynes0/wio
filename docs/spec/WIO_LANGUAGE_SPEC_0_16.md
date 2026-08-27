# Wio Language Specification 0.16 Delta

Status: normative for the Wio 0.16 release line.

This document defines the ownership, callback, async-task, and application
changes since the 0.15 specification. Earlier rules remain in force unless a
section below explicitly replaces them.

## 1. Borrow and native lifetime boundary

- `ref T` and `view T` remain call-scoped borrows. They cannot be retained by
  native code, cross an async suspension, or escape through an unsupported
  export return.
- `return self;` is the borrowed object/component return. An owning return uses
  `return deref self;`.
- `opaque` carries pointer identity only. It never implies destruction.
- A Wio wrapper that owns a native handle is an object with deterministic
  `OnDestruct` release. The stable host ABI represents transferred ownership
  with `WioOwnedNativeResource`; C++ uses the non-copyable
  `UniqueNativeResource` wrapper. Borrowing produces
  `WioBorrowedNativeResource` and does not extend lifetime.

## 2. Native callback contract

`WioHostCallback` contains typed scalar parameter/return metadata, userdata,
balanced retain/release operations, an invocation entry, flags, and a
last-error query. A borrowed descriptor is valid only for its call. Native code
that stores it must retain exactly once and later release exactly once.

Callback entry may occur on a native thread only when the descriptor declares
thread safety. Host exceptions are contained and reported as
`WIO_CALLBACK_FAULTED`; they do not unwind through the ABI. `ref`/`view`
payloads cannot be stored as callback userdata.

## 3. Async functions and host tasks

An `async fn` call eagerly creates one shared `Task<T>` and runs until its first
suspension. `await` never blocks an operating-system thread. `Block`, `get`, or
host `wait_for` are explicit synchronous boundaries.

Cancellation is observable and cooperative:

- cancelling a coroutine cancels the child it is directly awaiting;
- cancellation-aware timers wake promptly;
- `TryWithCancellation<T>` returns `Option<T>` and uses `None` for
  cancellation;
- its void form returns `false` for cancellation;
- cancellation-token I/O overloads return an ordinary `Result` cancellation
  error;
- a portable in-flight blocking syscall may finish privately, but its native
  lease and result storage remain safe until cleanup.

ABI v10 exposes stable-scalar exported async functions through retained task
handles. Polling is non-blocking. Deadlines, cancellation, completion callback
target, and main-executor pumping are explicit host operations. Module unload
invalidates generation-bound entry bindings while already-created tasks retain
the library needed to finish safely.

## 4. Application and system model

One top-level `application` is the program root. Systems are stack-resident,
component-like state owned by that application. The runner calls:

1. application `on start`, then systems in declaration order;
2. scheduled update stages in deterministic dependency order;
3. successfully started systems in reverse order, then application `on close`.

The first exit request wins. No new frame begins after an exit request.
Executable and host-driven loops use a monotonic `f64` delta in seconds,
clamped to the portable runner limit. Host update is non-blocking and
main-thread-affine.

If startup fails, only systems whose start handler completed are closed, in
reverse order. Application close still runs. The host state becomes faulted and
terminally closed; restart is rejected.

## 5. Schedule and resources

Stages form a directed acyclic graph through `after`. Equal-ready stages retain
source order. A normal stage runs once per frame. A fixed stage declares a
positive `at N hz` frequency, accumulates delta, and runs zero or more fixed
steps deterministically.

Application resources are declared with `resource` and injected explicitly:

```wio
system Physics {
    on update(delta: f64, world: ref World, input: view Input) {
        world.Step(input, delta);
    }
}

application Game {
    resource world: World;
    resource input: Input;
    system physics: Physics;

    schedule {
        fixed stage simulation at 60hz on main {
            run physics.update(ref self.world, ref self.input);
        }
        stage finish after simulation { run self.update; }
    }

    on update { }
}
```

The first system update parameter is `f64`; subsequent parameters must be
`ref` or `view`. Scheduled arguments must be explicit `ref self.resource`
borrows. Unknown, non-resource, or duplicate per-run resources are rejected.
The handler type decides mutable versus read-only access. The 0.16 scheduler is
sequential; these declared accesses are the future conflict-analysis input and
do not enable hidden parallel execution.

## 6. ABI and compatibility

Module descriptor version 10 adds application and async-host tables. Older
module capabilities retain their meaning. Hosts must validate descriptor size,
version, flags, signatures, and capability bits before use. All application,
task, callback, and native-resource cleanup operations have explicit ownership
and failure-containment rules; none rely on a process-global hidden owner.
