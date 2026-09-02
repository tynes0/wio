# Async, Coroutine, and Thread WIR Contract

This document freezes the backend-neutral representation of Wio async
functions. The existing C++ generator remains the production backend during
migration, but neither the future Lowered-WIR C++ backend nor the bytecode VM
may reinterpret these rules.

## Typed WIR

An `async fn` keeps its public `coroutine<T>` callable return type and also owns
a `CoroutineLayout` whose result type is `T`. A source `return value` therefore
returns `T` in Typed WIR; it never pretends that the function body constructs a
`coroutine<T>` value directly.

`await task` is represented by `await` with one `coroutine<T>` operand and a
`T` result. For `coroutine<void>` it has no result. Executor handoff is a
separate `executor-switch` operation, so `await main` cannot be confused with
waiting for a task.

Known scheduling calls retain backend-neutral operation metadata such as
`spawn-worker`, `spawn-blocking`, `spawn-io`, `join`, `cancel`, `detach`,
`yield`, `sleep`, and `wait`. Cross-executor safety remains a semantic-analysis
decision; WIR records the selected executor rather than repeating overload or
capture analysis in each backend.

## Canonical Lowering

Each typed suspension point becomes:

```text
cancellation-check state=N
coroutine-suspend state=N ... resume=block-M

block-M:
  value: T = coroutine-resume state=N
```

The resume instruction is absent for `void` awaits and executor switches. An
async body exit becomes `coroutine-complete`, with a payload matching `T`.
Ordinary `return` is forbidden in a lowered async function. Every state records
its suspend block, resume block, awaited task, resumed value, result type,
executor, and cancellation behavior.

The coroutine frame publishes stable slots with WIR type, ownership, and
cleanup metadata. The first implementation deliberately uses a conservative
frame: parameters and value definitions are retained so no backend can lose a
value across suspension. Later escape/liveness optimization may remove slots,
but it cannot change ownership behavior. Awaited task handles remain
intrusive-reference-counted and are released exactly once by the normal
lowered cleanup operations after resumption.

## Cancellation and Threads

Cancellation is cooperative and is checked immediately before every suspend or
executor-switch point. A verifier rejects a suspend without its matching
check. `maySwitchThreads` is set when a state or scheduling operation selects
main, worker, blocking, or I/O execution. It is metadata for code generation
and validation, not permission to bypass the analyzer's `Send` and borrowed
lifetime checks.

## Backend Requirements

Both backends must implement the same observable contract:

- C++ emits a coroutine frame/state machine and maps WIR ownership to the
  existing intrusive runtime protocol.
- The VM stores the same frame slots and state indices in VM-owned task frames.
- Cancellation, completion, panic propagation, and exactly-once cleanup have
  identical boundaries.
- Executor switches resume on the executor fixed in the WIR state.
- Native async calls use the native ABI adapter contract and return the same
  task-handle ownership as Wio-created tasks.

Exceptional/panic cleanup edges and executor runtime emission belong to later
backend sprints. Their representation must extend this contract rather than
recovering information from the AST.
