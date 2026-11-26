# N01: Extended Cost Map and Counter-Free Timing — 2026-10-15

This extends measurement coverage, **not production runtime behavior**. The
original nine core workload sources remain unchanged. Six additional workloads
cover Unicode text, dictionary, Result failure, runtime weak references, captures
and immediately-ready coroutines. N01-N14 order is unchanged; N02 is not started.

Implementation: `5f40405`. The initial N01 platform gate is now confirmed green
on Windows Debug, Linux Release and macOS Release: **3/3 new test executions**
in [the previous CI run](https://github.com/tynes0/wio/actions/runs/38725060356).
This confirms correctness, not remote performance.

## Why separate counter-free timing?

Counting ordinary new/new[] adds atomic operations and makes allocation events
observable. This can affect compiler optimization/allocation elision as well as
execution cost. Timing that build alone is not a production-speed comparison.

`-DisableAllocationCounters` now compiles out the diagnostic replacement
new/delete operators entirely, leaving standard allocation operators. The
same source kernels/checksums are used. Timer boundaries remain unchanged.
Counter-free JSON marks allocation fields **null**, with
`CountersEnabled=false`; summaries say `not-measured`. Raw executable output's
zero placeholders must not be mistaken for measured zero allocations.

These modes ran in separate serial sessions, not a balanced cross-mode
experiment. Each session alternates C++/Wio order within measured pairs. Do not
subtract their medians to infer an exact cost per allocation or claim a runtime
speedup: no production optimization was implemented.

## Final extended baseline

2,000 iterations per case; fifteen measured pairs after an excluded warm-up
pair in each mode. **384/384 final executions** match independent modulo-2^64
goldens documented in [the fixture instructions](../tests/performance/native_costs/extended/README.md).
The CTest gate independently checks counts 1/17; the Count=2000 golden check
was also performed on all saved final samples, not only the C++ oracle output.

### Counter-free internal timing

| Composite workload | C++ median ms | Wio median ms | C++ range ms | Wio range ms |
| --- | ---: | ---: | --- | --- |
| Text slice/concat/UTF-8/hash | 0.1017 | 4.9883 | 0.0945-0.3598 | 4.4082-19.1241 |
| Integer Dict insert/lookup/miss | 0.1753 | 0.2179 | 0.1717-0.2706 | 0.2100-0.6701 |
| Result failure/error payload/hash | 0.2966 | 2.8879 | 0.2749-0.4680 | 2.3202-3.7699 |
| Native Ref/WeakRef lifecycle helper | 0.1699 | 0.2157 | 0.1630-0.5200 | 0.2074-0.3868 |
| Scalar capture/call/cleanup | 0.0027 | 0.5318 | 0.0026-0.0032 | 0.4928-1.0656 |
| Immediate coroutine + BlockOn | 0.8821 | 1.1608 | 0.7891-1.1767 | 0.9253-1.7241 |

Ranges are wide and affinity is not pinned. Preserve those outliers; do not
turn these composite timings into precise universal language-speed ratios.
Compiler elision of a known scalar lambda in the C++ reference is allowed.

### Separate instrumented allocation traffic

| Workload | C++ / Wio ordinary new/new[] calls | C++ / Wio requested bytes |
| --- | ---: | ---: |
| Text | 16 / 4,004 | 44,779 / 41,997,209 |
| Dictionary | 2,008 / 2,009 | 83,936 / 84,016 |
| Result error | 4,000 / 22,000 | 268,000 / 1,486,000 |
| Native weak helper | 2,000 / 2,000 | 64,000 / 64,000 |
| Closure | 0 / 6,000 | 0 / 272,000 |
| Ready coroutine | 4,000 / 6,000 | 528,000 / 640,000 |

Counts/bytes are consistent across the final instrumented samples. They describe
that instrumented build; they are not peak/live memory, bytes copied, RC-event
counts, malloc/aligned traffic or a promise of identical allocation elision in
the counter-free build. Instrumented timing remains in raw evidence rather than
being silently mixed with the counter-free timing table.

## What is and is not comparable

- Text uses Wio's actual validated UTF-8 Text runtime on both sides, including
  code-point slicing and a combining accent, not a wchar/UTF-32 substitution.
  This is not normalization, grapheme or general Unicode qualification.
- Dict is integer-key unordered insert/lookup and missing-key fallback, not
  ordered Tree/set, string-key hashing or a standalone rehash benchmark.
- C++ Result is a tagged error-value lower bound. It does not implement Wio's
  current Result object identity/ABI, domain/native-code storage or all error
  machinery. Neither variant throws a benchmarked exception/panic here.
- Weak uses the same noinline C++ helper through the real Ref/WeakRef runtime.
  It checks live lock, expiry, failed lock and delayed physical destruction
  after final weak release. Wio adds a native boundary, not a new source-language
  weak declaration. These are not counts of every generated retain/release.
- Closure is a local scalar capture, type-erased callable and one invocation;
  no escaped capture, native callback or cross-thread transfer is covered.
- C++ ready coroutine uses the same actual AsyncTask runtime with a simple
  co_return. Wio's generated cancellation/transport checks remain in its path.
  No suspension, timer, worker handoff or cancellation race is measured.

The structural candidates remain N02 text/concat buffers, N05/N08 capture/place
transport, N10 Result error copies and N11 ready-task representation. Similar
allocation traffic in Dict and the weak helper does not prove zero overhead or
complete subsystem optimization. Keep the agreed order and attribute costs
before changing implementations.

## Verification, environment and remaining gaps

Only the new `wio_native_cost_extended_contract` was run locally on the final
implementation: **1/1 passed in 140.03 seconds**. It covers instrumented O0/O2
and counter-free O2, six cases at counts 1/17 against independent fingerprints,
counter reset/disable, invalid inputs and disabled counter placeholders. No
timing thresholds or old corpus rerun. The earlier two-mode preflight passed
before counter-free support; it is not the final qualification result.
The final new gate also passed on clean Windows Debug, Linux Release and macOS
Release: **3/3 new test executions**, including O0/O2 and counter-free O2 checks,
in [the extended CI run](https://github.com/tynes0/wio/actions/runs/38754331032).
These are correctness runs at counts 1/17, not remote performance measurements.

Windows build 26100, GCC 15.2.0, C++20 -O2 -ffp-contract=off -static, no fast-math,
no pinned affinity and no overlapping local build/benchmark jobs. Machine idle
state, CPU frequency and other user activity were not controlled. Existing user
std/playground edits remain uncommitted and are recorded/hashed, not modified.
This is not clean-tree local performance qualification; clean CI is separate.

Raw evidence preserves source/generated/binary/runtime/runner hashes, current
revision and dirty state, flags, all samples, modes and ranges:

- [Instrumented samples](evidence/native-cost-extended-counted-2026-10-15.json)
  and [summary](evidence/native-cost-extended-counted-summary-2026-10-15.json).
- [Counter-free samples](evidence/native-cost-extended-plain-2026-10-15.json)
  and [summary](evidence/native-cost-extended-plain-summary-2026-10-15.json).

N01 remains open for copy/move and retain/release attribution, peak/live memory,
scheduled/suspended/cancelled async, broader containers, reflection/startup and
real I/O baselines. Missing metrics are not zero. The new mode does not change
the production allocator or VM safety limits.

## Fresh Mandelbrot tracking: unstable session, no speedup attribution

The existing frozen full source and numeric kernel were rebuilt/measured after
the new suites, without overlapping local build/benchmark jobs. Native uses five
measured observations after excluded warm-ups; the Release non-JIT VM uses three
separate-process observations with the existing explicit finite 10-billion
diagnostic budget. Production safety limits are unchanged.

| Metric | Median | Range / measured count |
| --- | ---: | --- |
| Native compute | 43.9924 ms | 30.9035-60.5521 ms / 5 |
| Release VM compute | 49,681.1540 ms | 44,162.5139-57,076.4868 ms / 3 |
| Native full PPM, previous saved C++ | 195.5976 ms | 170.1989-305.0779 ms / 5 |
| Native full PPM, current emission | 187.3799 ms | 180.9896-278.4276 ms / 5 |
| Release VM full PPM | 61,027.9853 ms | 36,302.4417-62,074.4851 ms / 3 |

All **24 executions**, including excluded native warm-ups, pass their checksums:
compute `11613930/3721766287256/94440`; PPM **5,156,528 bytes**, SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.
Kernel excludes startup/bridge loading/reporting; full wall includes startup,
loading, numeric work, formatting/reporting and file write/close, not compilation
or hash validation. Normal OS caches, not durable sync. The user playground/PPM
was untouched. [Complete evidence](evidence/mandelbrot-n01-extended-2026-10-15.json).

**These timings are substantially slower and more variable than the preceding
session; they are retained, not replaced with a more favorable rerun.** Previous
native compute/full medians were 23.3227/133.6333 ms, VM 19,044.3468/20,595.9902 ms.
This update makes no production changes and compiler, backend runtime archive
and Release VM host hashes match the preceding run exactly. In the current
session the prior saved native C++ control also runs slower, with ranges heavily
overlapping the new emission. There is no attributed optimization/regression or
precise current cross-language performance ratio from this session. The source
of execution-environment variation (power/frequency, scheduling, other activity,
etc.) was not isolated; do not present a guessed cause as measured fact.

Before quantitative acceptance/5-10% budget decisions, N01 must establish a
controlled, documented baseline with paired controls and stable machine
conditions. Do not silently change the user's system power plan or discard
unfavorable runs. This is part of measurement qualification, not a new main pass.

### Required dated-reference table

All values below are milliseconds; the two columns measure different scopes.

| Implementation | Compute-only kernel | Complete PPM external wall |
| --- | ---: | ---: |
| C++ fixed reference | 28.4052 | 76.02 |
| Wio native, fresh unstable session | 43.9924 | 187.3799 |
| Wio Release VM, fresh unstable session | 49,681.1540 | 61,027.9853 |
| C# fixed reference | 29.5856 | 149.13 |
| Python fixed reference | 1,348.0412 | 1,613.71 |

C++/C#/Python are unchanged **2026-10-04** observations, not rerun here. GCC
15.2.0, .NET 9 Release, plain CPython 3.12.14; same 800x800, 100-iteration, f64
workload and output/fingerprints, with storage/formatting differences documented
in [the original audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md) and
[reference evidence](evidence/mandelbrot-reference-baselines-2026-10-04.json).
Historical rows are context, not controlled comparisons to this variable session.
