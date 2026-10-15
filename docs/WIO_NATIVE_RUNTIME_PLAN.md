# Native Runtime Optimization: N01-N14

Accepted 2026-10-15. These are fixed substeps of optimization **pass 1**, not a
renumbering of historical backend sprints, v1 blockers, or the three main passes.
After native generated code/runtime/coordinated std comes pass 2 (non-JIT VM),
then pass 3 (compiler and build throughput). SDK remains a separate v1 blocker.

## Fixed execution order

| ID | Scope | Completion evidence |
| --- | --- | --- |
| N01 | Measurement and cost map | Qualified workloads, raw samples/checksums, clear coverage gaps; distinguish timing, allocation, copies, RC and memory metrics |
| N02 | String/text/concat | Reuse last-use owned buffers, avoid repeated growing-prefix copies, preserve alias/snapshot/NUL/evaluation semantics |
| N03 | Formatting/builders | Direct numeric append, float/locale costs, capacity reuse; preserve formatting and snapshot contracts |
| N04 | Byte/buffer/I/O | Avoid encoding/string/byte round trips, batch writes, reuse buffers; same-format comparisons stay separate from format changes |
| N05 | Value/place/component representation | Proven stack storage, direct fields, fewer temporaries and unnecessary optional/shared/type-erased cells; exact lifetimes and cleanup |
| N06 | Containers | Growth/reserve/bulk operations, iteration/index copies, map/set key/hash/rehash costs, span/view and sorting callbacks |
| N07 | Objects/intrusive RC | Remove redundant retain/release and handle transfers with lifetime/thread proofs; measure weak refs and destruction too |
| N08 | Calls/native ABI | Value argument transport, direct calls/devirtualization, closures and adapters/marshalling; retain boundary validation and exception behavior |
| N09 | Numeric loops/checks | Invariants/conversions/branches, proven redundant bounds/null checks, optimizer-friendly loops; preserve overflow and FP contracts |
| N10 | Result/Option/nullable/any | Success-path allocation/copies, propagation/boxing and cold errors; Result-first std migration without silently changing identity/ABI |
| N11 | Async/threading | Frames/captures, ready-task paths, executor hops, scheduler contention, cancellation/cleanup and batching |
| N12 | Startup/reflection/runtime support | Cold start, registration/global initialization, metadata lookup, unused features and cold panic/log paths |
| N13 | Remaining std closure | JSON/config/parsing, hash/random/regex/sort, Unicode/time/fs/process and remaining inventory, with representative cost checks |
| N14 | Native acceptance | Real applications plus microcases; correctness/platform/regression gates; original/previous/current evidence; measure 1.05-1.10x C++ where comparable semantics permit |

The order is fixed; dependency-related work can be shared without renumbering.
Do not close a whole item because a single narrow case improved. No universal
5-10% overhead guarantee and no permission to weaken safety, arithmetic or ABI.

## Std consolidation policy

The std pass includes **removing unnecessary APIs/modules and merging overlapping
specialized surfaces**, not only formatting existing code. For example, consider
moving the sorting algorithms from `std::sort` into `std::algorithms`, rather
than keeping a separate sorting-only module. This is an accepted candidate,
not a completed move or a mandate to merge unrelated responsibilities.

For each consolidation, inventory declarations, imports, call sites, tests,
native bindings and docs first. Select one canonical implementation/API and
state the migration/removal policy explicitly. Do not preserve duplicate
implementations indefinitely or delete behavior just because names resemble
one another. Apply Result-first API cleanup and the user's std formatting,
visibility, `[]` attribute and block rules in the same module slice. Coordinate
native/VM semantics; SDK/docs/IDE compatibility work remains visible.

No std module, production runtime, compiler or generated-code policy changes in
the N01 instrumentation slice. Existing dirty std/playground files are preserved.

## N01 inventory and honest coverage

| Workload/cost | Existing or new fixture | Current measurement boundary |
| --- | --- | --- |
| Numeric compute and full PPM | `mandelbrot_compute/compute.wio`, frozen full-render snapshot, `progress.ps1` | Native/Release VM, checksums; compute vs full wall time; existing snapshot provenance |
| Long-string interpolation | `string_interpolation/append.wio` | Paired generated snapshots, ordinary new/new[] count and internal time |
| Append and builder snapshot | `std_builders/append.wio` | Native paired code, allocations, fixed fingerprint |
| Ordinary `+=`, builder and integer output | New `native_costs/costs.wio` | Instrumented kernel duration, ordinary new/new[] calls and requested bytes |
| ByteWriter and dynamic array | Same new fixture | Growth/write/snapshot/read-sum composite costs, not isolated I/O |
| Component return and object alias/method | Same new fixture | Composite construction/transport/dispatch/destruction costs; no RC-event counter |
| Native calls and Result success | Same new fixture | Call/unwrap loop costs; tagged-value C++ Result is a lower bound, not matching Wio object ABI |
| Unicode, maps/sets, errors, weak refs, closures | Pending | Add focused fixtures before claiming those costs qualified |
| Async, scheduling, cancellation, threading | Pending | Existing correctness tests are not performance baselines |
| Cold startup, reflection, real file/network I/O | Pending | Whole-process wall time alone does not isolate these costs |
| Copy/move bytes and retain/release events | Pending | Need opt-in attribution; allocation bytes are not copied bytes |
| Peak/live memory and aligned/malloc traffic | Pending | Not measured by the ordinary-new probe; require an appropriate profiler or separate diagnostic build |

N01 has a working first cost-map slice, **not complete coverage of all native
runtime subsystems**. Missing metrics are not zeros. Later slices must extend
coverage before changing the relevant subsystem. N02 has not started here.

## Instrumentation and reproducibility

See [the manual runner](../tests/performance/native_costs/README.md). Timing and
allocation instrumentation share a build; atomic counters add overhead, so these
are diagnostic timings, not an uninstrumented production-speed leaderboard.
Requested bytes are cumulative successful ordinary new/new[] request sizes,
not RSS, resident/peak/live memory, allocated capacity or copy traffic.

Record source/toolchain/runtime/binary hashes, current revision and dirty state,
optimization/FP flags, excluded warm-up, sample counts and ranges. Alternate
paired order. Verify every output and keep failed/timeout samples. Preserve
Mandelbrot's same-output and fixed dated cross-language reference policy.

An N01 baseline does not prove an optimization or a regression against another
session. Numeric-only reference loops may be optimized more aggressively than
generated Wio; the result is an end-to-end composite cost, not the latency of an
individual component operation, Ref increment or instruction.
