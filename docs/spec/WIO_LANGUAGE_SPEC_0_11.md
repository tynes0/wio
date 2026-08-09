# Wio Language Specification 0.11 — Attributes, Matching, and Applications

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

## 6. Diagnostics

Violations in this document fail during parsing or semantic analysis. A
conforming implementation does not rely on generated-C++ failure for target,
argument, conflict, pattern, or lifecycle validation. Backend representation
is not source ABI.
