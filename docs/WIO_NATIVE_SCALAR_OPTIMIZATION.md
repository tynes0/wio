# Native Scalar Storage Optimization

Date: 2026-10-15. Implementation: `722e836`. This is the first measured slice
of optimization pass 1 (generated C++ and runtime), not completion of the pass.
VM runtime and compiler-throughput optimization remain passes 2 and 3.

Later std follow-up: [builder consolidation](WIO_STD_BUILDERS_CONSOLIDATION.md)
addresses growing-prefix append and requalifies native full PPM completion.
The scalar-only measurements below retain their original scope and do not
claim the later std gain or complete language-wide performance acceptance.

## What changed

`WirCppStoragePlan` makes conservative function-local emission decisions over
verified Lowered WIR. WIR identity, ABI, numeric checks, ownership and borrow
contracts are unchanged.

- Closed, stack-classified primitive local cells use `ScalarPlace<T>`: an inline
  scalar and initialization bit, without shared allocation, `std::function` or
  `std::optional`. Reads after clear/before initialization still reject.
- Trivial primitive SSA values use ordinary C++ scalar slots instead of optional
  storage. Explicitly released/reset values retain their lifetime slots.
- Synchronous CFGs use direct labels/edges instead of a block-number dispatch
  loop. All block/edge temporaries remain scoped; cleanup instructions still run.
- Block-parameter transfers snapshot their inputs before assigning destinations,
  including self-loop swaps. Phi assignment is simultaneous, not sequential.
- Borrowed, captured, projected, passed/stored/returned reference cells retain
  the full Place representation. Managed values, aggregates and coroutine frame
  storage are not promoted by this slice. Async/coroutine CFG dispatch remains
  on its established representation.

Canonical escape metadata currently classifies writes *into* a local cell as
`Store` too. The plan therefore admits Stack/Local and Stack/Store candidates,
then proves a closed use set: direct trivial loads and destination-only
init/store/replace/clear. Any other operand use, branch argument or hidden borrow
origin rejects promotion. Merely trusting a coarse escape flag was insufficient.

## Measured kernel

Same 800 x 800, 100-iteration-limit double-precision Mandelbrot compute fixture
as the [original audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md). No image formatting,
file I/O, string append or native call occurs inside the iteration loop. Clock
calls surround computation; reporting and process startup are outside the kernel
timer. All variants produce all three golden values on every run:

```text
iterations=11613930 weighted=3721766287256 interior=94440
```

Windows build 26100, Intel Core i7-13620H, MinGW GCC 15.2.0, C++20,
`-O2 -ffp-contract=off -static`, no fast-math. Wio additionally uses the normal
runtime/SDK includes, runtime archive, `-Wa,-mbig-obj` and Windows platform
libraries. Original/intermediate generated sources are recompiled against the
same current toolchain, headers, flags and archive as current output.

One warm-up round is excluded, followed by **15 interleaved measured rounds**
for C++, current Wio, original Wio and the intermediate implementation. No
concurrent local build was running during measurement. CPU affinity/power state
was not pinned, so these are local diagnostic medians/ranges, not universal
performance guarantees or statistically independent confidence intervals.

| Variant | Median kernel time | Range |
| --- | ---: | ---: |
| Original Wio generated at `ef3e53f` | 774.603 ms | 769.153–780.145 ms |
| Intermediate: direct CFG/plain SSA only, before local-cell promotion | 699.159 ms | 690.168–703.375 ms |
| Current Wio, `722e836` | 23.163 ms | 22.548–24.583 ms |
| Equivalent C++ | 22.553 ms | 21.819–24.364 ms |

Current is **33.44x faster** than the original (97.01% less kernel time), and
**30.18x faster** than the intermediate (96.69% less). Current/C++ is **1.027x**
(2.70% median overhead) for this kernel. The intermediate is a saved development
snapshot, not a separately published revision; its generated-source hash is
recorded alongside the original/current hashes in the evidence.

The historical 35.03x audit used another measurement session; its absolute times
must not be mixed into these speedup calculations. C#/Python were not rerun.
Process wall samples, compiler/runtime/source/generated/binary hashes and every
kernel sample are in [the raw evidence](evidence/native-scalar-2026-10-15.json).
Working outputs are under `build/native-scalar-722e836`; original and intermediate
snapshots are retained in separate ignored build directories.

Generated source is 40,484 bytes / 1,052 lines, versus 41,322 bytes / 978 lines
originally. More explicit edge scopes and readable helper formatting increase
line count; this is not support-code pruning. Native binary size is 3,618,372
bytes versus 3,649,706 bytes. Neither metric is a general size-budget closure.
Native compilation/linking and startup are excluded from the kernel gain.

A separate allocation-instrumented execution of each generated entry point
also preserves the three checksums: **15,454,738** calls to global
`new`/`new[]` originally, **2** currently. This includes entry/reporting work,
not only the timed kernel. The
[manual wrapper](evidence/native-scalar-allocation-driver.cpp) reuses
`tests/native/scalar_codegen_probe.h`; compile with the measured snapshot
directory and `tests/native` on the include path, the flags above and
`-DMEASURE_BASELINE` for the original. Do not mix its instrumented timing into
the uninstrumented medians. This counts ordinary C++ allocation calls, not
malloc/aligned allocations, allocation bytes, peak memory or retain/release.

## Correctness gates

Only the two newly introduced tests run for this optimization slice:

- `wio_wir_cpp_storage_plan`: admission/rejection boundaries for scalar cells,
  direct uses, native calls, captures, hidden borrow origins, phi edges,
  heap storage, explicit cleanup and async representation.
- `wio_wir_cpp_scalar_storage`: native `-O0` **and** `-O2`, initialization/clear
  guards, zero `new`/`new[]` calls in the scalar loop, exact checksum through
  early `continue`/`break`, snapshot capture/native `ref` fallback, and a verified
  Lowered-WIR self-loop phi swap (must return 21, not 22).

Local Windows Debug: **2/2 passed, 9.48 seconds**. Clean platform CI also builds
the real self-hosted CLI as well as the compiler/runtime and these tests:

| Platform/configuration | New tests | CTest duration | Evidence |
| --- | --- | ---: | --- |
| Windows / Debug | 2/2 passed | 12.26 s | [job](https://github.com/tynes0/wio/actions/runs/38670549194/job/116212409158) |
| Linux / Release | 2/2 passed | 8.79 s | [job](https://github.com/tynes0/wio/actions/runs/38670549194/job/116212408967) |
| macOS / Release | 2/2 passed | 8.46 s | [job](https://github.com/tynes0/wio/actions/runs/38670549194/job/116212409150) |

The [complete run](https://github.com/tynes0/wio/actions/runs/38670549194)
passed for implementation commit `722e836`: **6/6 new test executions** across
three clean builds. These are correctness test durations, not remote performance
benchmarks. The old full corpus was not rerun; historical functional closure
qualification is separate. Other architectures/configurations and whole-language
performance budgets are not inferred from these narrow gates.

## Reproduction

Build `wio_app` and its backend runtime first. The runner does not rebuild the
compiler. Windows/PowerShell 7 and MinGW GCC are required; .NET/Python are not.
Choose fresh output directories; the runner rejects overwriting measurements.

```powershell
pwsh -File tests/performance/native_scalar/run.ps1 `
    -OutputDirectory F:/Projects/wio/build/native-scalar-current `
    -BaselineDirectory F:/Projects/wio/build/native-scalar-baseline-ef3e53f-v2 `
    -PreviousDirectory F:/Projects/wio/build/native-scalar-stack-cfg-v1 `
    -Repetitions 15
```

Baseline/PreviousDirectory are optional and must contain a previously saved
`compute.wio.cpp`. They are recompiled, not executed as stale frozen binaries.
Without snapshots, the runner compares current Wio with the C++ reference.
For a fresh historical baseline, build the original compiler revision in an
independent checkout and emit the unchanged fixture there first. Report source
and compiler hashes if a measured worktree differs from HEAD; this runner
records both rather than assuming a clean user worktree.

## Remaining work

The compute-only representation bottleneck is addressed for this measured case.
Pass 1 remains open for managed/aggregate allocation and copying, retain/release,
containers, strings, byte/buffer output, formatting, async and complete apps.
In particular, the growing StringBuilder prefix-copy defect and the historical
full PPM timeout are **not fixed or requalified** by this compute result.
Capability-driven helper emission and broad build-throughput budgets remain open.
Next: isolate string/buffer formatting and alias/self-append behavior, then
optimize capacity reuse with before/after checksums and allocation measurements.
