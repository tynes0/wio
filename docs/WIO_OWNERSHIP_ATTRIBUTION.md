# N01 ownership attribution — 2026-10-15

Opening and closing stage: **N01**, not N02. This slice adds attribution and
tests, not a production compiler/runtime optimization. The runtime's Ref layout,
atomic ordering, inline definitions and object lifecycle have not been changed.

## Implementation and verification

The actual workload is [Wio](../tests/performance/native_costs/ownership/costs.wio).
It compares owning argument transport, borrowed object views, owning returns,
and calls into the existing header-only native Ref/WeakRef surface. A separate
explicit-operation native calibration is called from Wio; it is not a C++ speed
reference. Python orchestrates all builds and profile collection, with no new
PowerShell logic. Details and commands: [profiler guide](../tests/performance/native_costs/ownership/README.md).

GCC 15.2.0 / matching gcov, C++20, `-O2 -fno-inline --coverage
-fprofile-update=atomic`. A generated diagnostic copy removes only `#line`
directives, since gcov rejects non-monotonic generated source ranges. Production
output is untouched. The first inline-enabled experiment was rejected because
gcov omitted individual functions; its failed result is retained locally under
`build/ownership-gcov-initial-2026-10-15`. Disabling inlining makes these entries
visible, but deliberately prevents treating the result as exact shipping-O2
operation counts or a throughput measurement.

The window resets before the Wio kernel and dumps after its scoped cleanup.
Each case runs in a new process with a fresh profile. All template instances are
summed, not gcov's merged/max source-line counts. Calibration checks the known
number of each Ref copy/move construction/assignment and RC method entry at
counts 1, 17, 2000. Every case also runs in normal uninstrumented `-O2`, with
inlining enabled, and must match an independently computed wrapping-u64 checksum.
Input/compiler/runtime/fixture/generated/binary hashes must remain unchanged.

Only the new `wio_native_ownership_attribution` CTest is run locally: five Python
checks including 30 actual Wio executions, no old corpus. GCC-specific CI is
registered on Windows/Linux; this slice does not claim macOS Clang attribution.
The previous portable Wio/Python controller's CI is now confirmed successful on
Windows Debug, Ubuntu Release and macOS Release:
[run 38768204504](https://github.com/tynes0/wio/actions/runs/38768204504).

## Observed diagnostic counts

At count 2000, GCC 15.2.0 (raw per-template data and provenance in
[ownership evidence](evidence/ownership-attribution-2026-10-15.json)):

| Wio path | IncStrong | DecStrong | IncWeak | DecWeak | TryIncStrong entries | Physical destruction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Owning argument | 6000 | 6000 | 0 | 2000 | 0 | 2000 |
| Borrowed view | 2000 | 2000 | 0 | 2000 | 0 | 2000 |
| Owning return | 8000 | 8000 | 0 | 2000 | 0 | 2000 |
| Native weak lifetime | 2000 | 4000 | 2000 | 4000 | 4000 | 2000 |
| Explicit-operation control | 6000 | 8000 | 2000 | 4000 | 4000 | 2000 |

The weak/control kernels have one successful and one expired lock per iteration.
Successful lock increments strong via TryIncStrong, not IncStrong; therefore
IncStrong and DecStrong alone should not balance there. TryIncStrong entry count
is not CAS-attempt count, lock success count or contention evidence.

| Wio path | Ref copy construction | Ref move construction | Copy assignment | Move assignment |
| --- | ---: | ---: | ---: | ---: |
| Owning argument | 0 | 12001 | 0 | 3998 |
| Borrowed view | 0 | 4001 | 0 | 1999 |
| Owning return | 2000 | 18002 | 0 | 5997 |
| Native weak lifetime | 0 | 0 | 0 | 0 |
| Explicit-operation control | 2000 | 2000 | 2000 | 2000 |

Move events include generated transport/reused loop cells. They do not each
imply an RC atomic or payload allocation. Owning handles can also be formed
from raw pointers, explaining why copy construction alone undercounts retains.
These are evidence for later N05/N07/N08 investigation, not authorization to
remove ownership/safety checks without lifetime proofs.

## Fresh Mandelbrot tracking, unchanged production binaries

[All raw samples](evidence/mandelbrot-n01-ownership-2026-10-15.json): 32/32 output
checks pass, input/binary hashes and endpoint power state unchanged; same CPU2
affinity, five native and three VM measured samples per lane after warm-up.
The full-VM lane exceeds range and drift limits, so **overall TimingQualified is
false**. No sample is removed or rerun to obtain a pass.

| Current lane | Median | Previous Python session median |
| --- | ---: | ---: |
| Native compute | 23.9045 ms | 23.6669 ms |
| Native same-binary A/A | 26.0862 ms | 25.2621 ms |
| Native full PPM child wall | 138.0791 ms | 129.6232 ms |
| Previous native full PPM child wall, same session | 138.8722 ms | 131.2624 ms |
| Release VM compute | 20.5566244 s | 20.5034887 s |
| Release VM full PPM child wall | 21.8450944 s | 22.0008845 s |

Native compute range 23.7074–26.9900 ms; VM compute 20.1106446–21.1829851 s.
Full VM range 21.6214389–24.9630008 s; relative range 15.30%, drift 14.27%.
A/A median difference 8.73% meets the coarse 10% gate, not a 5% accuracy claim.
There was no production optimization: do not label session fluctuations a speed
gain or regression. The first fast N01 session was native 23.3227 ms / VM
19.0443468 s compute; that too is separate-session context, not a paired delta.

Fixed cross-language references remain dated **2026-10-04**, not remeasured:

| Implementation | Compute | Full PPM wall |
| --- | ---: | ---: |
| C++ GCC15.2 O2 | 28.4052 ms | 76.02 ms |
| C# .NET9 Release | 29.5856 ms | 149.13 ms |
| CPython3.12.14 | 1348.0412 ms | 1613.71 ms |

Reference inputs: 800x800, 100 iterations, same arithmetic/output fingerprints.
Different dates/controller wall boundaries/formatting costs preclude precise
current cross-language speed ratios. Python 3.13.14 runs the controller; it is
not the CPython3.12.14 reference implementation.

## Remaining N01 coverage

General String/Text/component payload-copy bytes, exact optimized production
RC attribution, live/peak memory, allocator/malloc/aligned traffic, suspended
and cancelled async, larger container inventory, startup/reflection and real
I/O remain open. Microcase timing qualification remains noisy. This finishes a
narrow Ref attribution slice, **not all N01**; N02 has not started.
