# Wio Async and Coroutine Model

Status: normative Wio 0.11 companion contract.

The section **Post-0.11 correctness candidate** records additive behavior on
the unreleased 0.12 branch. It does not rewrite the frozen 0.11 contract.

## Post-0.11 correctness candidate

`std::async::RunBlocking` now targets a distinct bounded blocking executor.
Continuation/timer workers therefore remain available while native or legacy
calls block. The pool is configured before first use with
`WIO_ASYNC_BLOCKING_WORKERS` (1..64) and `WIO_ASYNC_BLOCKING_QUEUE`
(1..1048576); invalid values use conservative defaults. Capacity is observable
through `BlockingWorkerCount`, `BlockingQueueCapacity`, and
`BlockingPendingCount`.

Cross-executor closures are checked before backend generation. Primitive and
structurally transfer-safe component values pass automatically. `ref`/`view`,
opaque handles, nested callables, and unsynchronized object/interface handles
do not. An object that owns appropriate synchronization may explicitly
implement the empty `std::async::Send` marker; `std::async::Sync` reserves the
corresponding shared-access contract. Claiming either marker is an unsafe
semantic promise by the type author, not automatic locking.

Cancellation now wakes `Sleep`/`Yield` suspension promptly and prevents the
cancelled coroutine from running beyond that boundary. Awaiter cancellation
detaches only that awaiter from a shared child; it does not implicitly cancel
the shared child for its other observers. Blocking native work remains
cooperative and is never forcibly killed.

`ShutdownRuntime` is explicit and idempotent. It stops accepting work, drains
accepted blocking callbacks first, then drains continuation/timer cleanup.
New work fails with a stopped-runtime exception. The ordering guarantees that
blocking jobs can publish their final continuation without racing a stopped
continuation pool. `Task<T>` is the friendly alias of the frozen
`coroutine<T>` shared-handle representation.

## 1. Source model

`async fn` declares a function whose call result is `coroutine<T>`. The return
statements inside the body still return `T`; `await` unwraps a
`coroutine<T>` to `T` and is legal only inside an async function or method.

```wio
use std::async as async;

async fn ReadScore() -> i32 {
    await async::Sleep(10u64);
    return 42;
}

async fn Entry() -> i32 {
    let score = await ReadScore();
    return score == 42 ? 0 : 1;
}
```

An async `Entry` is supported. The generated native entry point blocks on its
task and preserves the Wio exit-code contract. A synchronous caller can use
`std::async::BlockOn` explicitly.

Generic async functions and object methods are supported:

```wio
async fn Identity<T>(value: T) -> T { return value; }

object Counter {
    private value: i32;

    public async fn Next() -> i32 {
        await std::async::Yield();
        self.value += 1;
        return self.value;
    }
}
```

## 2. Task behavior

`coroutine<T>` is a copyable shared task handle. Calling an async function
starts it eagerly on the calling thread and it runs until its first suspension
point. Every copy observes the same completion, value, failure, and
cancellation state. Multiple awaiters are supported.

Continuation execution after a suspension is scheduled on Wio's process-wide
worker pool. Source code must not assume that code after `await` resumes on the
originating thread. Main-thread GUI/render work uses `std::async::Dispatcher`:
workers call `Post`, and the owning loop calls `Drain`. Automatic continuation
affinity capture is not part of Wio 0.11.

The runtime uses C++20 coroutine frames. A task keeps its frame and required
object receiver alive through completion. Object async methods retain a strong
`self` guard before they can suspend.

## 3. Lifetime boundary

The compiler rejects async surfaces whose current ownership model cannot make
suspension safe:

- `ref` or `view` parameters and return values;
- stack-resident component methods;
- extension methods with borrowed receivers;
- constructors, destructors, application/system lifecycle handlers, and
  operator overloads;
- exported/native ABI entry points that would expose a coroutine frame.

These are deliberate semantic diagnostics rather than generated-C++ errors.
They can be relaxed only after a borrow/effect model proves that the referenced
storage outlives every suspension.

## 4. Scheduler and timers

The default scheduler owns a worker pool sized from host hardware concurrency
with a minimum of two workers. `WIO_ASYNC_WORKERS` may select 2 through 256
workers before the scheduler is first used; invalid values fall back to the
host default. Immediate work and timers share one synchronized priority queue
in the frozen 0.11 runtime.
`Sleep` does not create a detached thread per timer. Equal-time work is ordered
by an internal monotonic sequence number.

Scheduler actions are isolated so an uncaught native callback cannot terminate
the worker pool. During process shutdown, queued timer continuations are drained
without honoring their remaining wall-clock delay so coroutine frames and
captured values are released. Structured work is still joined explicitly;
detached work does not keep shutdown waiting for a distant timer.

In the unreleased correctness candidate, `RunBlocking` moves synchronous work
to the separate bounded pool described above. `Run` remains the continuation
pool compatibility operation and is also subject to transfer-safe capture
analysis. Neither spelling claims that filesystem or network APIs have become
true non-blocking operating-system I/O.

## 5. Standard-library surface

Core task operations:

- `Sleep`, `Yield`, `Start`, `Detach`, `BlockOn`;
- `IsReady`, `IsCancelled`, `IsFaulted`, `WaitFor`;
- `Cancel` and `CancelAfter`;
- `RunBlocking` (`Run`) for worker-pool execution;
- `All`, `Any`, `Race`, `Timeout`, and recoverable `TimeoutOption`;
- `TaskGroup<T>` and `VoidTaskGroup`;
- `CancellationToken` and `CancellationSource`;
- `SleepCancellable` and the owner-drained `Dispatcher`;
- `WorkerCount` for diagnostics and capacity-aware code.

`All` preserves input order. If one child fails, remaining children are marked
cancelled. `Any` returns the lowest ready index observed by the scheduler.
`Race` returns the winning value and marks losers cancelled. `Timeout` marks an
unfinished child cancelled and fails with an async timeout. `TimeoutOption`
returns `None` for the expected deadline path and cancels the unfinished child;
it still propagates a real child failure. Task groups reject new work after
join/cancel and cancel unfinished children during destruction.

Cancellation is cooperative. It marks shared task state and makes a later
`await`/`BlockOn` fail; it never kills a native thread or forcibly unwinds user
code. Long-running code should accept a shared cancellation token and check it
at useful boundaries. A task can briefly continue toward its next suspension
or completion after cancellation.

## 6. Failure boundary

Native exceptions raised by task bodies are captured in the coroutine state
and rethrown at `await` or `BlockOn`. An unhandled failure reaching `Entry` is
reported by Wio's native-exception boundary and produces a non-zero exit code.

Wio does not expose source `try`/`catch` in 0.11. Consequently, `Timeout`,
awaiting a cancelled task, and native callback failures are terminal when the
caller chooses to await them. Expected timeout uses `TimeoutOption`; ordinary
recoverable library failures continue to use `Result<T>` (and therefore may
use `coroutine<Result<T>>`).

## 7. Outside the 0.11 contract

- async generators/streams and source `yield`;
- true platform async filesystem, socket, and process I/O;
- executor selection, priorities, work stealing, and automatic main-thread
  continuation dispatch;
- structured cancellation-token inheritance across arbitrary task trees;
- async component/extension borrows;
- source exception handling;
- application lifecycle handlers that suspend.

## 8. Freeze evidence

The 0.11 freeze matrix covers async `Entry`, generic functions, object and
interface dispatch, object receiver retention after the caller scope exits,
multiple awaiters, generic and void task groups, worker configuration,
recoverable and terminal timeout paths, cancellation, dispatcher handoff,
detached process exit, invalid borrowed/lifecycle surfaces, and a native
high-volume frame-lifetime stress test. Windows and Ubuntu run the matrix in
release CI; Ubuntu additionally runs the native runtime test under ASan/UBSan
and the frontend corpus under libFuzzer/ASan/UBSan.

Additive scheduler and I/O work may continue after 0.11, but changing hot-task
behavior, shared-handle identity, `await` typing, the lifetime rejections, or
the cancellation/failure meanings requires a new versioned specification.
