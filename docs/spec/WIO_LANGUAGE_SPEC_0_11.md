# Wio Language Specification 0.11 — Language and Standard-Library Foundation

Status: normative delta specification  
Language edition: 0.11  
Previous normative specifications: 0.8, 0.9, and 0.10

This document freezes language behavior introduced after Wio 0.10. Rules from
earlier specifications continue to apply unless this document replaces them.

## 1. Unit-success results

`std::ResultUnit` is the zero-state success payload. `std::UnitResult` is the
canonical alias for `std::Result<std::ResultUnit>`. `std::OkUnit()` constructs
success and `std::ErrUnit(error)` constructs failure. Recoverable public APIs
without a success payload use `UnitResult`; they do not special-case `void`.

## 2. Typed attributes

Postfix `with` attaches metadata. A declaration has exactly one non-empty
`with` clause; the clause does not accept a trailing comma. `using` activates
an attribute declared `scoped`. Legacy `@Name(...)` remains accepted during
its compatibility window, but new source and generators should emit
`with`/`using`.

```wio
using cpp::header("widget.h");

attribute route(method: string, path: string = "/")
    for fn retain runtime repeatable;

fn Health() -> string with route("GET", "/health") { return "ok"; }
```

`using marker(...);` applies from that statement through the remainder of the
current lexical realm or block. `using marker(...) { ... }` applies only to
the bounded declaration block and does not leak. An active scoped attribute is
inherited only by declarations included in its target set. Explicit and scoped
attributes participate in the same repetition and conflict checks.

Attribute declarations have typed positional parameters, an explicit target
set, retention, and optional policies. Supported targets are `fn`, `method`,
`component`, `object`, `interface`, `type`, `field`, `variable`, `parameter`,
`generic_parameter`, `enum`, `flagset`, `flag`, `enum_case`, `extension`, and
`realm`. Retention values are `source`, `compile`, and `runtime`.

Arguments are compile-time literals compatible with their parameter type.
Required parameters precede defaulted parameters. Non-repeatable attributes
occur at most once on a target. `conflicts groupA | groupB` joins named
exclusivity groups; two distinct attributes sharing a group cannot occur on
one target, independent of source order.

Runtime-retained attributes on components, objects, and fields are emitted to
deterministic reflection tables. Compile/source-only metadata is not visible
through `std::reflect`.

## 3. Pipeline operators

`value |> F` and `F <| value` lower to the ordinary call `F(value)`. When the
target already supplies arguments, `value |> F(a, b)` inserts the value as the
first argument and `F(a, b) <| value` appends it as the final argument.
Overload resolution, inference, conversion, evaluation, and diagnostics are
therefore identical to calls. A pipeline target must be callable and the piped
value is evaluated exactly once. `|>` associates left-to-right and `<|`
right-to-left. Arithmetic and logical expressions bind more tightly than a
pipeline; assignment binds less tightly.

## 4. Match destructuring

`match` supports ordinary values/ranges and these destructuring patterns:

- `Some(value)` / `None()` for `std::Option<T>`;
- `Ok(value)` / `Err(error)` for `std::Result<T>`;
- `[first, second]` exact-length array patterns.

Bindings are immutable case-local values with the payload/element type and
are visible in an optional `if` guard and the body. A pattern cannot bind the
same name twice. Guards are `bool` and are tested in source order. The match
input is evaluated exactly once. Object payload bindings preserve shared
object identity; component payload bindings own a value copy.

Duplicate unguarded variants and duplicate unguarded array lengths are
unreachable errors. A guarded case is also unreachable after an earlier
unguarded case that covers the same variant or exact array length.
Option/Result value matches are exhaustive when both unguarded variants exist.
Arrays and ordinary values require final `assumed` when producing a value.

## 5. Applications and systems

An executable may declare one module-top-level `application` root instead of
`Entry`. An application and an ordinary `Entry` cannot coexist, and an
executable cannot contain multiple application roots. Application and
`system` state is stack-resident component-like state. Lifecycle handlers are
parameterless; an application declares exactly one `on update`, and a
lifecycle handler name cannot be repeated in one application or system.

```wio
system Clock { mut ticks: u64; on update { self.ticks += 1u64; } }
application Tool {
    system clock: Clock;
    on update { if self.clock.ticks == 10u64 { self.Exit(0); } }
}
```

The 0.11 runner is sequential and deterministic: application start runs once;
systems start in declaration order; system updates precede application update;
the first `self.Exit(code)` wins and ends scheduling after the current handler;
later exit requests do not replace its code; started systems close in reverse
order; application close runs exactly once; the host receives the exit code.

Parallel schedules, injected resources, fixed stages, handler parameters, and
native event-loop hosts are experimental and outside this edition.

## 6. Lambda capture

An ordinary lambda captures referenced outer values by value when the lambda
is created. Primitive and component captures are snapshots. Object captures
copy the managed handle and therefore preserve shared object identity. A
captured `ref` or `view` copies the borrow itself, not the referred value; its
ordinary lifetime restrictions still apply. Lambda bodies are mutable with
respect to their owned capture copies.

A lambda created by an object method and using `self` retains the object for
the closure's lifetime. Returning or dispatching that closure therefore cannot
leave a raw receiver pointer after the originating object handle is released.

This value-capture default applies equally to synchronous algorithms, threads,
dispatch queues, and async worker callbacks. There is no implicit by-reference
capture mode in 0.11.

## 7. Async functions and coroutines

`async fn F(...) -> T` has callable result type `coroutine<T>` while `return`
inside the body is checked against `T`. `await expression` is legal only in an
async function or method, requires `coroutine<T>`, and yields `T`. Calls are
hot: execution begins immediately and proceeds to the first suspension. A
copied coroutine value is a shared task handle, not a second execution.

Top-level functions, interface methods, object methods, generic declarations,
and `Entry` support `async`. Async `Entry` retains the ordinary `i32`/`void`
source contract; the native entry point blocks on its task. Object async
methods retain their receiver through completion. Continuations after a
suspension may run on a worker thread and have no implicit main-thread affinity.

The compiler rejects async `ref`/`view` parameters and returns, component and
extension methods, constructors/destructors, application/system lifecycle
handlers, operators, and native/export ABI functions. These rejections prevent
a borrow or stack receiver from outliving its owner. Async generators and
source `yield` are outside 0.11.

The detailed scheduler, task, failure, cancellation, and structured-concurrency
contract is normative in [`WIO_ASYNC_MODEL.md`](../WIO_ASYNC_MODEL.md) and this
edition's [`WIO_STD_SPEC_0_11.md`](./WIO_STD_SPEC_0_11.md).

## 8. Diagnostics

Violations in this document fail during parsing or semantic analysis. A
conforming implementation does not rely on generated-C++ failure for target,
argument, conflict, pattern, lifecycle, async typing, or async lifetime
validation. Backend representation is not source ABI.
