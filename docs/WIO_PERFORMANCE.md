# Wio Performance And Memory Notes

This document gives the practical `v1` cost model for the parts of Wio that
users are most likely to care about.

It is not a benchmark report. The goal is to make the broad rules visible.
For the controlled local Mandelbrot comparison and its limitations, see
[`WIO_MANDELBROT_PERFORMANCE_AUDIT.md`](./WIO_MANDELBROT_PERFORMANCE_AUDIT.md).

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

## 8. Final Pre-v1 Optimization Stage

A dedicated heavy optimization and performance acceptance stage follows backend
closure and IDE/tooling/v1 correctness work, before beta/RC. It complements the
earlier backend cost work; serious slowdowns should be investigated immediately,
not deferred until the final stage. This work is mandatory before v1, while JIT
remains post-v1.

### 8.1 Mandelbrot Qualification Workload

The current user-owned `playground/wio/mandelbrot.wio` is a motivating workload,
not yet a controlled benchmark. The inspected version renders 800 x 800 pixels
with at most 100 iterations per pixel using `f64`. It formats 640,000 RGB lines
through interpolation and `StringBuilder.AppendLine`, then writes an ASCII PPM
(`P3`) file. Its manifest selects Debug and supplies no backend optimization
arguments. Inspection of the native compiler command construction found no
default `-O2`/`-O3`; configuration-to-backend flag forwarding must be qualified
rather than assuming the manifest selects optimized generated code.

`StringBuilder.AppendLine` currently uses `self.value += value + "\n"`. The
builder is an object wrapping a string, without a dedicated buffer/capacity API.
Typed WIR currently represents compound assignment as load, binary addition,
and replace/store; the C++ backend emits string addition as `+`, rather than a
dedicated in-place append. This is not an optimized builder contract. Audit
full-prefix copies, temporary allocations, capacity growth, and final-output
copies. The local Mandelbrot audit confirms full-prefix addition in generated
`AppendLine` code, making accumulated copying quadratic for this workload;
the exact contribution relative to other runtime overhead still needs profiling.

Require an amortized-linear builder with reusable capacity, reserve/clear
behavior, direct append/append-line, and numeric formatting into its buffer.
Avoid assembling a temporary string for each pixel merely to append it, and
specify whether finalization returns a copy or transfers storage. Also qualify
ordinary string `+=` so builder users are not the only ones with efficient
appends. Preserve observable value/ownership semantics and correctness for
self-append and aliased inputs in both native and VM paths.

Keep a controlled benchmark fixture independent of user playground edits and
output files. Measure these separately:

1. compiler/front-end and native compilation, excluding program execution;
2. compute-only Mandelbrot, with an observable iteration/pixel checksum;
3. RGB formatting and builder growth over precomputed pixel data;
4. file output over an already-created buffer;
5. full compute/format/write execution, including startup.

Compare Wio native with hand-written C++ using the same dimensions, iteration
limit, arithmetic/precision, output format, compiler, flags, and I/O scope.
Validate image/checksum equality. Do not compare an unoptimized full ASCII-output
run with optimized compute-only C++, or enable fast-math on only one side. VM
results are a separate series with explicit budgets, not a promise of native
speed. The reported minutes and suggested half-second C++ time are not measured
baseline results in this document.

### 8.2 Optimization Order And Exit Criteria

1. Capture reproducible baselines and profiles; record hardware, toolchain,
   configuration, commands, inputs, cache state, repetitions, median, and spread.
2. Optimize measured semantic-analyzer, Typed-WIR, and Lowered-WIR hotspots;
   remove repeated traversal, lookup, construction, and allocation work.
3. Optimize C++ emission and build scalability, then emitted runtime code:
   unnecessary dynamic places/wrappers, copies, call indirection, retain/release,
   helper duplication, dependency pruning, caching, and measured precompilation.
4. Optimize std/Unicode and native/SDK paths, including formatting, builders,
   containers, ownership transfer, and boundary marshalling.
5. Optimize VM execution, value storage, dispatch, calls, cleanup, and scheduler
   overhead under the same correctness and ownership contracts.
6. Repeat integrated qualification with realistic installed-toolchain projects
   and supported platforms; lock numeric budgets into regression/release gates.

Track cold, warm, and incremental builds separately; track runtime/startup,
generated source and binary size, peak memory, allocation counts/bytes, copied
bytes, and retain/release counts where instrumentation permits. Qualify Debug
and Release independently, including effective flags for generated code and
linked runtime libraries. Optimization must not hide broken flag forwarding.

Set explicit numeric budgets after establishing the controlled baselines, with
native/C++ ratios and VM-specific targets where appropriate. Do not substitute
an unmeasured speed estimate for an acceptance threshold. Close this stage only
when the agreed budgets and correctness/ownership tests pass, representative
workloads have no unexplained catastrophic scaling, and before/after results
are published. Performance improvements must not weaken bounds, nullable,
overflow, borrow, cleanup, or Unicode behavior.
