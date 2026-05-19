# Wio Performance And Memory Notes

This document gives the practical `v1` cost model for the parts of Wio that
users are most likely to care about.

It is not a benchmark report. The goal is to make the broad rules visible.

For quick local measurements, the CLI now provides:

```powershell
wio perf smoke --iterations 3
```

That command measures a small set of real user-facing flows such as:

- `wio file check`
- `wio file run`
- cold and warm `wio project build`
- warm `wio project run`

## 1. Value, View, Ref

### 1.1 Value

Ordinary values behave like ordinary value copies unless the specific type
documents a different runtime wrapper behavior.

Practical reading:

- plain scalar values are cheap,
- POD-like `component` values are intended to behave like normal value data,
- copying larger aggregates may cost real memory movement.

### 1.2 `view`

`view` is a readable reference-like borrow.

Practical reading:

- use `view` when you want to avoid copying,
- do not expect `view` to own the target,
- and do not assume it outlives the referenced storage.

### 1.3 `ref`

`ref` is a readable/writable reference-like borrow.

Practical reading:

- use `ref` for mutation and write-through behavior,
- prefer `view` when you only need read access,
- remember that value contexts may auto-read a readable reference.

### 1.4 `deref`

`deref` removes exactly one reference layer.

That means:

- `deref ref T -> T`
- `deref ref ref T -> ref T`

The runtime intent is explicit layer control rather than magical full collapse.

## 2. `std::Box<T>`, `any`, And `opaque`

### 2.1 `std::Box<T>`

`std::Box<T>` is an owned boxed runtime value.

Practical reading:

- it implies heap-backed ownership,
- it is useful when the language/runtime needs stable boxed storage,
- and it should not be treated like a free scalar copy.

### 2.2 `any`

`any` is an erased Wio-owned runtime payload.

Practical reading:

- storing into `any` is not a zero-cost no-op,
- extraction requires runtime checking,
- and it is best used when runtime type-erasure is actually needed.

### 2.3 `opaque`

`opaque` is a foreign/native handle carrier.

Practical reading:

- it represents a handle boundary, not a boxed Wio value,
- it should be treated like interop payload rather than a normal value family,
- and the cost model depends mainly on the native resource behind the handle.

## 3. Containers

Arrays, dictionaries, and trees are not intended to be read as zero-cost magic.

Practical reading:

- mutating through `ref values[i]` avoids unnecessary copy-back patterns,
- nested `ref` chains are the intended write-through path,
- inferred locals in value contexts may auto-read from readable references,
- container growth may allocate and move storage depending on the underlying
  container behavior.

If code cares about avoiding repeated growth cost, capacity-oriented buffer
types are a better fit than assuming all containers are cheap to grow.

## 4. Native Passing

The broad `v1` rule is:

- POD-like `component` data is the structural native-bridge category,
- `object` values behave like handles / wrappers rather than shared POD memory,
- `opaque` is the explicit foreign-handle category,
- exported calls and native wrappers should be treated as ABI boundaries.

Practical reading:

- crossing the native boundary is not free,
- structural POD-like values are the cheapest intended bridge category,
- runtime wrapper concepts such as `any` and `std::Box<T>` are not POD layout
  promises.

## 5. Generated Code And Backend Costs

Generated `.wio.cpp` files are backend intermediates.

Practical reading:

- they should not be treated as the public semantic model,
- they may change shape as implementation hardens,
- and source-adjacent generated output is intentionally not the default path.

## 6. What Users Should Optimize First

For `v1`, the best default heuristics are:

1. prefer ordinary values for small plain data,
2. use `view` for read-only non-owning access,
3. use `ref` for mutation and write-through access,
4. use `std::Box<T>` only when boxed ownership is actually needed,
5. use `any` only when runtime type-erasure is actually needed,
6. treat native crossings as meaningful boundaries,
7. avoid assuming large container growth is free.

## 7. What This Document Does Not Promise

This note does not freeze:

- exact backend-generated instruction quality,
- exact allocation counts for every std surface,
- or benchmark-level guarantees.

It freezes the intended user-facing reading of the performance and memory
model, not every low-level implementation detail.
