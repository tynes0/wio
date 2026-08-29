# Wio Language Specification 0.17 Delta

Status: release-candidate normative text for the Wio 0.17 release line.

This document replaces the source spelling of the 0.16 application/system
surface while retaining its runtime, ownership, host ABI, and compatibility
rules. Earlier specifications remain in force unless a rule below replaces
them.

## 1. Canonical application members

An `application` body accepts ordinary fields and functions. A field may omit
`mut`; omitted mutability means mutable. The value is default-initialized when
no initializer is present.

The conventional functions `Start`, `Update`, and `Close` bind lifecycle roles.
The same roles may bind descriptive function names with `[Start]`, `[Update]`,
and `[Close]`. Each role may be bound at most once and an application must bind
exactly one update role.

Application lifecycle functions are synchronous, non-generic, and return unit.
Start and close take no parameters. Update takes zero parameters or one `f64`
delta in seconds. An omitted update parameter is normalized internally; it does
not alter source-level name lookup. Non-lifecycle functions obey ordinary Wio
function rules and become extension methods on the stack-resident application
component.

## 2. Canonical system members

A `system` body uses the same ordinary-field rule and lifecycle binding rules.
System lifecycle functions are synchronous, non-generic, and return unit.
Non-lifecycle functions remain ordinary extension methods.

An application field whose nominal type is a system in the source module is an
owned system. It starts in field order, updates in deterministic schedule order,
and closes in reverse successful-start order. System values remain stack
resident and do not imply object identity, virtual dispatch, or heap ownership.

## 3. Attribute-driven schedule

The following compile-time attributes target application handlers:

- `[Fixed(hz)]`: `hz` is a positive numeric constant. The stage accumulates
  host delta and runs with a fixed `1 / hz` step zero or more times per frame.
- `[After(stage)]`: `stage` is an identifier or string naming another
  application function stage or owned system field stage.
- `[Main]`: records required main-thread affinity. The sequential 0.17 runner
  satisfies it without dispatch.
- `[Worker]`: reserved. Use is a compile-time error until cross-thread borrow
  conflict analysis is part of the language contract.

A scheduled function is synchronous, non-generic, returns unit, and accepts
zero parameters or one `f64` step/delta parameter. Stage dependencies form a
directed acyclic graph. Unknown dependencies, duplicate names, and cycles are
compile-time errors. Equal-ready stages preserve declaration order.

`After` is intentionally the same public compile-time ordering attribute used
by attribute processor declarations. Its allowed target determines whether it
orders processors or application stages.

## 4. Default schedule

Without schedule attributes, owned systems update in field order and the
application update lifecycle runs afterward. With schedule attributes, owned
systems first become named variable-step stages, attributed functions become
their declared stages, and the unscheduled application update lifecycle remains
a variable-step stage. A scheduled update lifecycle is not inserted twice.

Every executable/application host rule from 0.16 remains: one root, monotonic
clamped delta, first exit request wins, no new frame after exit, partial-start
rollback, reverse system close, one application close, and main-thread-affine
host calls.

## 5. Compatibility

The 0.16 `on start/update/close`, `resource`, member `system`, and explicit
`schedule`/`stage`/`run` grammar remains accepted in 0.17. It lowers to the same
component, extension, and application host ABI as the canonical form. New
templates, documentation, and examples use ordinary members plus attributes.

Legacy explicit schedule syntax remains the only 0.17 surface for passing
application resources as `ref`/`view` arguments to system updates. A later
semantic scheduler will replace that compatibility gap with typed access
metadata before legacy removal is considered.
