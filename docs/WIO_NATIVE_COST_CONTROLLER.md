# N01: Native cost measurement controller — 2026-10-15

Opening item: **N01**. Closing item: **N01, still open**. No transition to N02.
This closes the native-cost **controller migration slice**, not the entire N01
cost/attribution inventory. No production compiler, runtime, std or workload
source is optimized or changed.

## Delivered

- `native_costs/run.py` now only prepares/builds and writes provenance before
  delegating measurements. Sources/toolchain/headers are hashed around the build.
- `native_costs/qualify.py` reuses frozen core/extended C++ and Wio binaries with
  recorded suite/count/counter mode and refuses changed input artifacts.
- Both native cost suites use Python pre-launch affinity, async process
  capture, timeout/tree termination, hash, median and stability helpers.
- Every lane has a warm-up; rounds rotate/reverse case order. An A/A lane repeats
  the exact first C++ executable. No second compilation is used as the A/A oracle.
- `OutputsCorrect` and `TimingQualified` are separate. Unstable timing returns
  nonzero without discarding correct fingerprints/allocation evidence.
- Allocation fields stay null in counter-free runs. Inconsistent raw mode
  placeholders, malformed/overflowing counters and zero/invalid times are rejected.
- Extended counts 1/17/2000 are checked against existing independent goldens as
  well as C++ parity. Core/arbitrary counts remain explicitly parity checks.
- The old original instrumented core provenance needs an explicit recorded
  legacy override. It is not silently inferred as a counter-free/timing build.

Both actual workload suites remain `.wio`. The user's testing preference is
Wio behavior tests with Python orchestration; do not add substantial PS1 test
logic for new work. The `.ps1` entries forward old arguments for compatibility
only. The just-added synthetic C++ protocol fixture/PS test have been removed
(recoverable in Git); synthetic protocol cases are now injected Python unit mocks.

Only this diagnostic process tree is pinned. Power scheme, priority and other
applications are unchanged; the controller mask is restored on all exits.
Default range/drift/A/A limits remain 15%/10%/10%, with no trimming or automatic
reruns. They are coarse noise gates, not statistical precision guarantees.

## New checks

The new Python contract exercises constant/unstable reported durations through
injected mocks, real child capture/termination (including descendants), affinity
restoration, and a real tracked scratch-header mutation. It then compiles/runs
the core and extended Wio fixtures at count 17, with 128 checked executions.
Synthetic times are never benchmark results. Windows/Linux support inherited
hard affinity; macOS collects explicitly unpinned correctness with timing marked
unqualified. The same test executes real Wio workloads on all three CI platforms.
No old correctness corpus is rerun. Power-policy data can be unavailable on Unix;
that does not silently become a qualified time.

The real build/delegation path was checked separately at core count 17: 76/76
outputs pass, build sources/toolchain remain unchanged, and the tiny/noisy timing
samples are rejected. The initial extended frozen-binary smoke passed 52/52
independent fingerprints but correctly rejected some unstable timing lanes.

## Final Python contract and full-count evidence

Implementation: `33f2c1b`, committed/pushed on `feature/backend-closure`. Final local
new CTest: **1/1 in 55.66 seconds**, all **26 Python contract tests** pass with no
skips, including actual Wio emission/compilation/execution of both suites. There
are **128/128 correct process results** in the real-Wio portion at count 17.
The old full correctness corpus was not run.

Only measurement/orchestration is Python. Core/extended workloads remain the
original Wio source files. Both compare to C++ references and diagnostic native
hooks. Source copying and generated output stay in fresh ignored build folders;
user std/playground changes are preserved. Test timing acceptance is deliberately
not a CI machine-speed requirement.

The three final 2000-count experiments reuse the exact previously recorded
native artifacts, not new builds, with CPU 2 and 15 measured rounds plus warm-up:

| Experiment | Checked executions | Stable timing lanes | Output/provenance | Timing acceptance |
| --- | ---: | ---: | --- | --- |
| Core instrumented | 304 | 6/19 | Pass | Rejected |
| Extended instrumented | 208 | 9/13 | Pass | Rejected |
| Extended counter-free | 208 | 9/13 | Pass | Rejected |

All **720/720** fingerprints pass; all before/after runner/binary/provenance hashes
are unchanged. Instrumented allocation-count/request-byte sets match the earlier
baseline exactly for every original C++ and Wio lane. Counter-free allocation
fields remain null. This supports the cost-map observations, not a timing gain.
No samples were trimmed or rerun until passing. The rejected timings and their
ranges are retained in full:

- [Core raw](evidence/native-cost-python-core-2026-10-15.json)
- [Extended counted raw](evidence/native-cost-python-counted-2026-10-15.json)
- [Extended counter-free raw](evidence/native-cost-python-plain-2026-10-15.json)

Examples of unchanged Wio evidence at count 2000: concat **2003 allocations /
64,034,122 requested bytes**; component **4001 / 160,033**; object **6001 / 192,033**;
Result success **2001 / 176,033**. Requested bytes are not copied/live bytes.

The [three-platform Python CI run](https://github.com/tynes0/wio/actions/runs/38768204504)
has passed Ubuntu Release and macOS Release, including the actual Wio workload
build/execution. Windows Debug is still running at this report checkpoint; its
completion is not presumed. This is not just non-Windows syntax qualification.

## Fresh Python Mandelbrot tracking

The new `mandelbrot_compute/progress.py` also uses the Python controller and
frozen existing native/bytecode/bridge/Release-VM artifacts. Nothing is rebuilt.
All **32/32 executions** pass numeric or PPM fingerprints, including excluded
warm-ups; runner/input hashes and endpoint power state are unchanged. Five native
and three VM measured samples per case are retained, with no trimming/reruns.
All lanes pass the unchanged coarse range/drift gates; the identical-native A/A
median difference is **6.52%**, below its 10% gate but not 5% accuracy evidence.

| Metric | Median ms | Min-max ms / measured count |
| --- | ---: | --- |
| Native compute | 23.6669 | 23.5731-27.0291 / 5 |
| Same-executable A/A compute | 25.2621 | 23.6146-26.9767 / 5 |
| Native current PPM child wall | 129.6232 | 125.9504-137.5378 / 5 |
| Native previous PPM child wall | 131.2624 | 128.7239-139.5459 / 5 |
| Release VM compute | 20,503.4887 | 19,861.9410-20,914.5332 / 3 |
| Release VM PPM child wall | 22,000.8845 | 21,749.5095-22,431.4787 / 3 |

[All Python Mandelbrot samples](evidence/mandelbrot-python-native-cost-2026-10-15.json).
This locally validates the new Mandelbrot driver on Windows, not a claim that
the Mandelbrot artifact path has been tested on every OS. VM keeps the existing
diagnostic-only finite 10-billion-instruction budget; production defaults are
unchanged. PPM remains 5,156,528 bytes and SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.

### Required dated-reference table

| Implementation | Compute-only ms | Complete PPM ms |
| --- | ---: | ---: |
| C++ fixed 2026-10-04 | 28.4052 | 76.02 |
| Wio native, Python controller 2026-10-15 | 23.6669 | 129.6232 |
| Wio Release non-JIT VM, Python controller 2026-10-15 | 20,503.4887 | 22,000.8845 |
| C# fixed 2026-10-04 | 29.5856 | 149.13 |
| Python fixed 2026-10-04 | 1,348.0412 | 1,613.71 |

Reference rows were not rerun. Same 800x800/100-iteration f64 workload/fingerprints;
GCC 15.2.0 `-O2 -ffp-contract=off`, .NET 9 Release and plain CPython 3.12.14, with
storage/formatting differences in the [original audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md).
The **harness** Python 3.13 is not the Python reference workload; do not confuse
these versions or infer that the language baseline was remeasured.

Kernel time excludes startup/load/reporting. New PPM child wall excludes Python
worker startup, includes child launch/wait/output-drain overhead, and excludes
validation hashing, compilation and durable disk sync. It is not identical to
the old external-wall harness boundary. Different dates/controllers mean the
table is context, not a precise current cross-language speed ratio.

The original fast tracking session had native compute 23.3227 ms and VM compute
19,044.3468 ms. The immediately preceding read-only follow-up had 26.1001 ms and
21,134.2951 ms, with ranges failing acceptance
([retained evidence](evidence/measurement-followup-2026-10-15.json)). The earlier
unstable controller session had native 59.7378 ms and VM development observation
59,212.9412 ms. The 2-4x difference does **not** recur here. No production code or
same-binary runtime improvement is attributed to this controller migration.

N01 still needs copy/move and RC-event
attribution, peak/live memory, suspended/worker/cancellation scenarios, broader
containers and startup/reflection/real-I/O coverage. A quiet controlled timing
baseline for the noisy microcases and tighter accuracy remains required. One
coarse-passing Mandelbrot experiment does not close N01. N02 has not started.
