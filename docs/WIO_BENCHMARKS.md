# Wio-owned benchmarking

Introduced 2026-10-16 during N02; this is measurement infrastructure, not closure
of N02 or a VM/native performance improvement claim.

The implementation and complete usage/schema/memory limits are documented in
[the wio-bench manual](../tools/wio-bench/README.md).

## What runs where

| Part | Implementation | Responsibility |
| --- | --- | --- |
| `std::benchmark` | Wio | Verified in-process runs, calibration, statistics, report/protocol construction |
| `wio-bench` | Wio | Manifest/CLI, build recipes, version capture, rotating comparison/control rounds, final native/VM rounds, checkpoints, reports and comparisons |
| Runtime helper | Native C++ | Monotonic clock, opaque input/observable sink, OS process isolation/deadlines/capture, CPU/RSS, SHA256 snapshots |
| Allocation diagnostic | Opt-in single-TU C++ | Ordinary/aligned/new/delete request/live/peak window accounting, not shipping allocation policy |
| Bootstrap and oracle | Python | Build the Wio tool, generate example recipes and independently validate checksums/contract behavior |

For every future measurement-affecting optimization: equivalent Wio/C++/C# work,
Wio native and VM outputs, frozen before/after states, independent correctness,
same flags/contracts, all raw data retained. Each run finishes with a distinct
repeated Wio native/VM phase. Optional extra implementations such as Python are
allowed, not substituted for the mandatory four core implementations.

## Problems the qualification work caught

- Native code can reuse a pure calculation from warmup, making the purported
  timed workload disappear. The first example produced zero/100ns native samples
  and was rejected, not advertised as a speedup. Current code supplies opaque
  runtime input and consumes the result inside the timed interval. Critical
  benchmarks still require inspection; no black-box helper prevents every form
  of compile-time folding inside arbitrary user code.
- `json::NumberValue` previously formatted through generic interpolation, and
  the JSON writer rejected integer-shaped floating representations such as `0`.
  Zero standard deviation/MAD became null. Number construction and fallback
  writing now use locale-independent `to_chars` with round-trip f64 precision,
  preserving real numeric zero, signed zero and close floating-point values.
  Nonfinite numbers retain the existing null policy. New tests exercise this;
  no old corpus was run locally.
- VM bridge generation removes its temporary generated C++ file. The independent
  native diagnostic build now uses a retained native-source copy, not that
  disposable file. Earlier failed development runs remain under the build folder.

## Acceptance interpretation

Correct output, source/artifact endpoint integrity and timing acceptance are
different fields. A complete, correct but noisy/unpinned run exits 2. It is usable
for correctness and diagnosis, not a qualified speedup assertion. A partial or
incorrect run exits 1 and preserves its checkpoint.

The native exact-command A/A lane, relative range/drift, verified child affinity
and observed unchanged power profile all participate in overall qualification.
macOS currently has no power-profile observer and cannot earn overall timing
qualification. Missing measurements are unavailable/null rather than zeros.
The tool never changes priorities, power policy, security settings or other apps.

Memory columns refer to **OS child-process accounting**, not the kernel's peak
live allocation. Allocation windows are diagnostic-only and must use a separate
executable. VM host RSS is supported; transparent VM-heap allocation attribution
through an instrumented native bridge is not promised. C# managed allocation
counts are retained as a different payload metric, never treated as C++ requests.

## Initial real comparison: 2026-10-16

20,000 steps of the same bounded unsigned integer recurrence, runtime argument,
independent expected checksum `21507387217936`. All four core implementations
plus Python ran. C++ and Wio native: GCC 15.2 O2, C++20, FP contraction disabled.
C#: .NET SDK 9.0.311 / runtime 9.0.13 Release with tiering disabled. Python:
CPython 3.13.14. VM: explicit Release `wio-vm` host. Each implementation has two
in-process warmups outside its reported kernel. Comparison has five measured
samples per lane; the final Wio phase has three, plus separate warmups/A/A.

| Implementation | Comparison kernel median | Final Wio kernel median | Comparison process peak RSS median |
| --- | ---: | ---: | ---: |
| Wio native | 0.0601 ms | 0.0601 ms | 9.03 MiB |
| Wio Release VM | 12.7815 ms | 12.9558 ms | 20.58 MiB |
| C++ O2 | 0.0609 ms | not rerun in final | 5.07 MiB |
| C# Release | 0.0598 ms | not rerun in final | 23.27 MiB |
| Python | 1.5992 ms | not rerun in final | 25.26 MiB |

**48/48 outputs correct**, source/artifact endpoints unchanged, requested CPU 2
affinity applied and observed power scheme unchanged. Overall timing is
**unqualified**: Python relative range 24.6% exceeds 15%; no sample was removed or
remeasured to pass. Other lanes pass their coarse numerical gates. These are
descriptive initial timings, not an optimization/speedup claim or Mandelbrot
comparison. The controller's Debug folder names the bootstrap configuration;
the controller itself is native O2, and workload flags/VM Release provenance are
explicit in the manifest. Windows tool is statically linked to avoid requiring
MinGW DLLs in an installed user's PATH.

[Raw session](evidence/wio-bench-compute-2026-10-16.json) and
[generated report](evidence/wio-bench-compute-2026-10-16.md) include all samples,
build/version output, source/toolchain/runtime/binary hashes and memory scopes.
Two earlier preparation attempts were rejected before collecting timings: a
stale installed WIO_ROOT and an unhashable Windows Store Python execution alias.
The generated suite now declares its process-local environment and chooses the
actual Python interpreter image; failed checkpoints remain under `build/`.

The separate O0 allocation example (`examples/memory.wio`) observed 9 requests,
4587 requested/freed bytes, 3652 peak live requested bytes and zero final tracked
bytes/blocks. Those are diagnostic requested sizes, not RSS or optimized speed.
Binding the Begin Result to a long-lived local exposed a real generated local
cell allocation; the scoped example consumes that Result immediately. The window
does not hide generated bookkeeping or promise to measure only user payloads.
The [diagnostic observations and artifact hashes](evidence/wio-benchmark-memory-2026-10-16.json)
are retained separately from the plain timing session.

## Final validation

Only the new `wio_benchmark_contract` CTest ran: **1/1 passed, 690.91 seconds**
on Windows Debug. The existing core test corpus was not run. This final run used
the CMake-built static native O2 controller explicitly; CI's default bootstraps
the Wio controller from source. The test covers library native O0/O2/VM,
instrumented Wio allocations, independent C++ allocator/OS calibration, protocol
and negative manifests, no-overwrite/checkpoints, ordinary descendant cleanup,
and actual four-implementation build/comparison/final phases. An intentionally
wrong inherited WIO_ROOT is overridden by the suite's declared process-only root.

[Contract evidence](evidence/wio-benchmark-contract-2026-10-16.json) and
[its four-implementation output](evidence/wio-benchmark-contract-compute-2026-10-16.json)
are separate from the 20,000-operation Release-VM demonstration above. The
functional contract's VM host follows its Debug CTest configuration and its
timings are not substituted into that performance table. The CMake target,
project manifest discovery and Windows no-MinGW-DLL linkage were also checked.
Linux/macOS CI is configured, not yet claimed green locally.
