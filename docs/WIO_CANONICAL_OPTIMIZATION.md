# Canonical Lowered WIR Optimization

Status: implemented backend contract on `feature/wir-pipeline`.

The canonical optimizer is the final shared transformation layer before C++ or
bytecode emission. It may improve representation and attach backend decisions,
but it may not rediscover language semantics, overloads, ownership, native ABI,
or application metadata.

## Invariants

- Every input module has already passed the Typed WIR verifier.
- Every output module passes the Lowered WIR verifier.
- Function, global, block, type, and surviving value IDs remain stable.
- Surviving instructions retain their original source spans.
- A pass cannot remove calls, cleanup, retain/release, stores, checked access,
  coroutine operations, or another potentially observable effect.
- Optimization does not change intrusive reference-counting boundaries.

The optimizer is deterministic and exposes `OptimizationStatistics` for tests,
benchmarks, and future `--explain-optimization` tooling.

## Constant Folding

`unary`, `binary`, numeric `convert`, and `range-contains` operations fold when
all required operands are constants. Integer addition, subtraction,
multiplication, division, remainder, and shifts are folded only when the host
calculation is defined and representable in the WIR literal model. Overflow,
division by zero, invalid shifts, and out-of-range checked conversions stay as
runtime operations. Pointer-sized integer arithmetic/conversion also remains
explicit until a concrete backend target fixes its width.

```text
%v0: !i32 = const 20
%v1: !i32 = const 22
%v2: !i32 = add %v0, %v1
```

becomes:

```text
%v2: !i32 = const 42
```

Dead `%v0` and `%v1` definitions are removed later if they have no other uses.

## CFG and Value Simplification

A boolean constant `cond-jump` becomes one unconditional `jump`. Forwarding
blocks without parameters are threaded, then blocks unreachable from the entry
block are removed. The optimizer may replace a trivial block parameter when
every live predecessor supplies the same value. Cleanup-bearing and borrowed
ownership transfers are not propagated by this pass.

Dead-code elimination is intentionally conservative. Constants, arithmetic,
numeric conversions, function references, type tests, and other explicitly pure
trivial results may disappear when unused. Operations capable of allocation,
panic, native entry, mutation, suspension, or cleanup remain observable.

## Escape and Storage Decisions

Allocation candidates receive two independent facts:

- `escape`: `local`, `call`, `store`, `return`, or `coroutine`
- `storage`: `stack`, `heap`, or `coroutine-frame`

Objects, runtime containers, interpolated storage, iterators, escaping closures,
and other reference-counted allocations remain heap-backed. Non-escaping
component values and local places prefer stack storage. Values that must survive
an async suspension use coroutine-frame storage unless their backing allocation
is intrinsically heap-based.

This is a backend decision, not an ownership rewrite. A heap object stored in a
coroutine frame still has one heap allocation and one intrusive handle in the
frame.

## Bounds-Check Proofs

Every `array-get` and `array-place` reaches Lowered WIR with `bounds=required`.
The optimizer changes that field only for a non-negative constant index when it
can prove one of these cases:

- `eliminated-static`: the array type has a fixed extent and the index is inside
  that extent
- `eliminated-proven`: the base is a directly known `array-create` and the index
  is smaller than its element count

Unknown, negative, dynamic, or out-of-range indexes keep their runtime check.
The Lowered WIR verifier rejects missing bounds metadata and metadata attached
to non-array operations.

## Backend Rule

C++ and bytecode backends consume these decisions but do not strengthen them.
A backend may conservatively keep an eliminated bounds check or place a stack
candidate on the heap, but it may not remove a required check, stack-allocate a
heap decision, or change the declared cleanup protocol.
