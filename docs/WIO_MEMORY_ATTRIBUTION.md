# N01 live/peak memory attribution — 2026-10-15

Opening/closing stage: **N01**. No N02 transition and no production compiler,
runtime, allocator or std optimization in this slice. Only the new memory CTest
was run locally; the previous test corpus was not run.

## What changed

[Wio fixtures](../tests/performance/native_costs/memory/costs.wio) retain objects,
strings and bytes across a live checkpoint and release them through normal Wio
scope cleanup. Native controls, called from Wio, validate exact allocation
counts/byte peaks, alignment, zero-byte/nothrow/overflow behavior, pre-window
frees and delayed physical deletion by the last weak reference. Orchestration,
strict protocol validation and independent checksums are Python. No new PS1.

The separate diagnostic executable replaces ordinary/aligned global C++
new/delete, storing allocation size/window membership in a prefix. It reports
requested/freed/live/peak bytes and blocks, never calling them copied bytes or
RSS. Counts exclude prefix, alignment padding and allocator overhead. The common
timing controller and frozen Mandelbrot artifacts are unchanged. Full commands,
boundaries and source references: [memory profiler guide](../tests/performance/native_costs/memory/README.md).

Process checkpoint observations are Windows working set/private commit, Linux
approximate VmRSS/VmHWM, and macOS Mach resident_size/resident_size_max. These
include the entire diagnostic process, and lifetime peak is not reset at Begin.
Unsupported/unavailable metrics are null. Freeing all window-owned blocks does
not require a drop in OS resident memory.

## Local result

GCC15.2 C++20 `-O2 -fsized-deallocation`; Python3.13.14; Windows x64.
`wio_native_memory_attribution`: **1/1, 8.75 seconds**; five checks and **18 real
Wio executions**, counts 1/17/2000 across six cases. All independent checksums,
accounting invariants, native controls and source/artifact hashes pass. All
final live-byte/block counters are zero. Protocol tests reject malformed,
duplicate/non-finite/overflow counters, inconsistent accounting and retained
window allocations. Not a whole-application leak proof.

At count 2000, requested-byte diagnostics (not elapsed-time or RSS comparisons):

| Workload | Successful allocations | Cumulative requested bytes | Live bytes at checkpoint | Peak live bytes | Live bytes after cleanup |
| --- | ---: | ---: | ---: | ---: | ---: |
| Native ordinary control | 2 | 384004 | 384004 | 384004 | 0 |
| Native aligned/edge control | 6 | 10499 | 10499 | 10499 | 0 |
| Native weak lifetime | 1 | 24 | 24 | 24 | 0 |
| Wio retained objects | 4013 | 160808 | 80432 | 80496 | 0 |
| Wio retained strings | 8013 | 503758 | 152518 | 152674 | 0 |
| Wio byte array | 13 | 4143 | 2096 | 3120 | 0 |

[Raw memory samples and provenance](evidence/memory-attribution-2026-10-15.json).
For example, object-array process working set was 7553024 bytes at baseline,
7823360 at the live point and 7610368 after cleanup, while window-owned live
requested bytes reached zero. Its process-lifetime working-set peak was
7835648 bytes. These metrics measure different boundaries; do not subtract
them to infer allocator metadata or treat retained OS pages as Wio leaks.

CI wiring runs this new test on all three OSes, without speed gates; this local
result is not a claim that the new CI has already passed. The preceding Ref
attribution change is confirmed green:
[run 38773045449](https://github.com/tynes0/wio/actions/runs/38773045449).

## Fresh unchanged-artifact Mandelbrot tracking

[Raw tracking samples](evidence/mandelbrot-n01-memory-2026-10-15.json): **32/32**
output checks, unchanged hashes and endpoint power state, CPU2 inherited
affinity, excluded warm-ups, five native/three VM samples per lane. Overall
**TimingQualified=false**: native A/A chronological drift, VM compute spread/
drift and both full-native lanes exceed their gates. The A/A median difference
is 4.27%, but that alone does not qualify the run. No samples were trimmed or
retried. No production binary was rebuilt or optimized in this slice.

| Lane | First fast N01 session | Previous ownership session | Current memory session |
| --- | ---: | ---: | ---: |
| Wio native compute | 23.3227 ms | 23.9045 ms | 24.0372 ms |
| Wio native full PPM wall | 133.6333 ms | 138.0791 ms | 137.4266 ms |
| Release VM compute | 19.0443468 s | 20.5566244 s | 20.6477775 s |
| Release VM full PPM wall | 20.5959902 s | 21.8450944 s | 22.3969702 s |

First fast N01 values used the older external-wall controller; later values
use Python child-wall timing, excluding worker startup but including process
launch/wait/output drain. Compute internal timing is unchanged. The table is
historical context, not a controlled optimization delta. Current native compute
range is 23.5764–25.3190 ms; VM compute 20.1101847–23.6772661 s. Native full PPM
range 127.6273–196.8047 ms, previous full-native control 124.6617–196.6180 ms;
do not assign the common slowdown to a compiler change that did not happen.

Fixed references, **2026-10-04**, not rerun in this slice:

| Implementation | Compute | Full PPM wall |
| --- | ---: | ---: |
| C++ GCC15.2 O2 | 28.4052 ms | 76.02 ms |
| C# .NET9 Release | 29.5856 ms | 149.13 ms |
| CPython3.12.14 | 1348.0412 ms | 1613.71 ms |

Same 800x800/100-iteration algorithm and verified output fingerprints, but
different sessions/controllers/output costs: these are context, not precise
cross-language speed ratios. Python3.13.14 is the controller, not the reference
Python implementation. All implementations should be rerun after the three
main optimization passes, as planned.

## Remaining scope

No direct malloc/realloc/custom-allocator traffic interception, copy/move payload
byte measurement, continuous per-window process peak, deallocation-race testing,
or uninstrumented production-memory baseline. Broader containers, suspended/
cancelled async, startup/reflection and real I/O remain visible N01 gaps; later
optimization slices need representative evidence for their own changes.
N01 remains open; N02 has not started.
