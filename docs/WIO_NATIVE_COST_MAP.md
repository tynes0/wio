# N01: First Native Runtime Cost Map — 2026-10-15

This is measurement infrastructure, **not a runtime optimization**. The fixed
[N01-N14 native plan](WIO_NATIVE_RUNTIME_PLAN.md) records std deletion/merging
policy, execution order and unmeasured categories. N01 remains in progress;
N02 has not started. No production compiler/runtime/std source is changed.

## Nine-workload baseline

Each row executes 2,000 iterations with an observable checksum. Fifteen measured
C++/Wio pairs follow an excluded warm-up pair; measured execution order alternates.
All **288 executions** match the C++ warm-up checksum oracle. Byte fingerprints
are order-sensitive FNV-1a; integer sequences use a rolling checksum. This is
parity evidence, not an independent algorithm oracle.

| Workload | C++ median ms | Wio median ms | C++ / Wio ordinary allocation calls | C++ / Wio requested bytes |
| --- | ---: | ---: | ---: | ---: |
| String `+=` | 0.1041 | 1.3158 | 13 / 2,003 | 131,085 / 64,034,122 |
| StringBuilder + snapshot | 0.1264 | 0.3395 | 14 / 2,017 | 195,086 / 261,230 |
| Integer interpolation + append | 0.0489 | 0.2462 | 12 / 15 | 39,624 / 39,768 |
| ByteWriter + snapshot + hash | 0.0182 | 0.2990 | 16 / 19 | 24,416 / 24,544 |
| Array growth + rolling checksum | 0.0140 | 0.0352 | 13 / 14 | 32,793 / 32,841 |
| Component return + field reads | 0.0057 | 0.3756 | 1 / 4,001 | 33 / 160,033 |
| Object + alias + method + release | 0.1120 | 0.5157 | 2,001 / 6,001 | 48,033 / 192,033 |
| Native scalar call chain | 0.0084 | 0.0868 | 1 / 1 | 33 / 33 |
| Result success + unwrap | 0.0092 | 0.7272 | 1 / 2,001 | 33 / 176,033 |

Times include relaxed-atomic allocation instrumentation, dispatch/setup,
checksum work and local destruction; they are not an uninstrumented production
leaderboard or isolated operation latency. The common setup constructs a
32-byte fragment (one 33-byte request), even in numeric cases. Allocation counts
and byte sums include that setup. Requested bytes are cumulative requests, not
peak memory, copied bytes or retained capacity. Aligned allocation, malloc,
live bytes and retain/release events are not measured.

All ranges are retained in [the summary](evidence/native-cost-map-summary-2026-10-15.json)
and all samples in [raw evidence](evidence/native-cost-map-2026-10-15.json).
For example, Wio concat spans **1.2745–21.1634 ms**, rather than hiding its long
outlier behind the median. Short reference cases run in microseconds; do not
interpret their ratios as precise universal compiler overheads.

The C++ builder is direct string append with a matching snapshot. Both object
cases use the actual Wio intrusive Ref and an explicit alias. The Result
reference is an optimized std::variant success value, **not an equivalent Wio
object identity/ABI representation**. Component/reference loops may optimize
differently. These are cost-map comparisons, not proof that only one specific
operation accounts for the whole gap.

## What this tells us to investigate

- N02: growing-prefix `+=` traffic is the clearest first target. About 64.0 MB
  of requests produce a 64,000-byte final value. No implementation fix is made.
- N03/N08: builder's repeated owned string argument costs remain visible;
  numeric formatting also has costs not explained by allocation count alone.
- N05: inspect component place/projection/return transport. Generated MakePair
  still uses `Place<Component>::local()` with the general shared owning cell;
  the component value itself being stack-oriented does not eliminate that cell.
  There is no allocation stack trace or exact two-allocation attribution yet.
- N07/N08/N10: object handle transport, native call wrappers and Result success
  need focused attribution; object allocation count is not an RC-event count.
- N04/N06: ByteWriter and array have relatively similar allocation volumes to
  references, so copying, checks, dispatch and scalar transport need examination.

Do not reorder or close N02-N14 based solely on this matrix. Follow the agreed
order while adding the missing subsystem-specific baselines before changes.

## Reproducibility and verification

Windows build 26100, MinGW GCC 15.2.0, C++20 `-O2 -ffp-contract=off -static`,
no fast-math, no pinned affinity, no overlapping local builds/benchmarks.
Sources/toolchain/runtime/generated source/binary hashes and the dirty tree
are in raw evidence. The fixture was measured on parent `13510a5` with the new
N01 files present; it was then committed as `ae12526`. Pre-existing unrelated
std edits remain present and hashed; this is not a pristine-tree performance
claim. Earlier exploratory runs used weaker sum checksums; their local results
are not substituted for this final rolling-checksum baseline.

Generated C++ is 754,000 bytes; the native diagnostic executable is 5,430,313
bytes, versus the direct reference's 547,265 bytes. These are total diagnostic
artifacts, not stripped-size or link-time comparisons. Build throughput and
generated-code sharding remain pass 3; no compilation-speed gain is claimed.

Only the new `wio_native_cost_contract` test was run locally on the final source:
**1/1 passed in 32.41 seconds**. It qualifies counter reset/disable/byte counts,
invalid input, and native/reference fingerprints at counts 1/17 under O0/O2.
No timing threshold and no old test corpus rerun. Three-platform correctness
qualification was dispatched in [CI](https://github.com/tynes0/wio/actions/runs/38725060356).
At this report's checkpoint Linux Release and macOS Release passed the new
contract gate; Windows Debug was still building. This is not a completed
three-platform success claim or remote performance measurement.

Run instructions and instrumentation boundaries:
[native_costs README](../tests/performance/native_costs/README.md).

## Fresh per-update Mandelbrot tracking

The existing runner rebuilt native and bytecode/bridge outputs from the frozen
full source used in the prior direct-append slice; user playground sources and
PPM were not used. Native measured samples follow an excluded warm-up. VM uses
the MSVC Release non-JIT diagnostic host and explicit finite 10-billion
instruction budget, with production limits unchanged. VM has three independent
observations, not a hidden warm-up. No concurrent local build/benchmark ran.

| Metric | Median | Range / measured samples |
| --- | ---: | --- |
| Native compute | 23.3227 ms | 23.2166-23.6619 ms / 5 |
| Release VM compute | 19,044.3468 ms | 19,029.9142-19,242.5836 ms / 3 |
| Native full PPM, prior saved C++ | 131.0273 ms | 128.2287-132.5179 ms / 5 |
| Native full PPM, current emission | 133.6333 ms | 130.4066-135.0875 ms / 5 |
| Release VM full PPM | 20,595.9902 ms | 20,547.4245-20,637.8799 ms / 3 |

The saved prior generated snapshot comes from `d6cdbd3` and the current compiler
runtime behavior is unchanged in this instrumentation slice. The nominal 1.99%
current/prior native full median difference has overlapping ranges and is not
attributed to a new optimization/regression. The previous session's balanced
current full median was 131.7694 ms; its 23.8496 ms native kernel and 19.0779206 s
VM kernel are historical samples, not a controlled before/after experiment for
this no-production-change slice. There is no speedup claim.

All **24 executions** (including excluded native warm-ups) pass their output
checks: compute `11613930/3721766287256/94440`; PPM **5,156,528 bytes**, SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.
Kernel timing excludes startup, reporting and bridge loading; full PPM external
wall time includes startup/loading, computation, formatting/reporting and file
write/close, but excludes compilation/hash validation and does not imply durable
disk sync. Raw samples/hashes/configuration are in
[the progress evidence](evidence/mandelbrot-n01-2026-10-15.json).

### Fixed-reference comparison

| Implementation | Compute-only kernel | Complete PPM external wall |
| --- | ---: | ---: |
| C++ fixed reference | 28.4052 ms | 76.02 ms |
| Wio native, fresh | 23.3227 ms | 133.6333 ms |
| Wio Release VM, fresh | 19,044.3468 ms | 20,595.9902 ms |
| C# fixed reference | 29.5856 ms | 149.13 ms |
| Python fixed reference | 1,348.0412 ms | 1,613.71 ms |

C++/C#/Python remain the **2026-10-04** references, not new paired executions:
GCC 15.2.0, .NET 9 Release and plain CPython 3.12.14. Same dimensions (800x800),
100-iteration limit, FP precision policy and checksums; differing output/storage
algorithms and original environment are documented in
[the baseline audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md) and
[preserved reference evidence](evidence/mandelbrot-reference-baselines-2026-10-04.json).
Do not subtract columns to invent a formatting-only time or infer a precise
current cross-language speed ratio from historical rows.
