# Native cost map (N01)

Manual Windows / PowerShell 7 diagnostic. Requires the built Debug Wio compiler,
backend runtime and MinGW GCC. It changes no production implementation. Sources
are copied to a fresh ignored build directory before C++ emission; user playground
files and outputs are never used. Do not benchmark while another build/test runs.

The default `-Suite core` remains the original nine cases. `-Suite extended`
selects six additional workloads; see [their boundaries](extended/README.md).
`-DisableAllocationCounters` compiles out replacement allocation operators for
counter-free timing. The JSON marks allocation metrics unmeasured, not zero;
the default instrumentation mode and original core workload sources are unchanged.

```powershell
pwsh -NoProfile -File tests/performance/native_costs/run.ps1 `
    -OutputDirectory F:/Projects/wio/build/native-cost-map-new-run `
    -Repetitions 15 -Count 2000 -LogicalProcessor 2
```

Nine independently selected workloads: ordinary string `+=`, StringBuilder,
integer interpolation + builder, ByteWriter, dynamic array, component return,
object construction/alias/method, native scalar call, Result success/unwrap.
The count is runtime input, bounded to 1..100000. Large concat counts can be
quadratic in the current implementation; do not assume every count is cheap.

Both executables use GCC C++20 `-O2 -ffp-contract=off -static`; no fast-math.
Each lane has an excluded warm-up, followed by measured rounds whose case order
rotates/reverses. A separate A/A lane repeats the first C++ workload using the
**same executable**, not a second compilation. C++ warm-up supplies the checksum
oracle; every C++/Wio execution must match. Extended counts 1, 17 and 2000 also
have pinned independent goldens. Other counts/core cases are parity checks, not
independent algorithm proofs. Bytes use order-sensitive FNV-1a; integer sequences use a rolling checksum
rather than a sum that would ignore reordering. This verifies parity, not an
independent algorithm proof. Build provenance goes to `build.json`;
`qualification/results.json` preserves every sample, timeout/failure, environment
and before/after hash. `qualification/summary.json` contains medians/ranges and
per-lane stability. A failure stops the run with nonzero status and partial evidence.
Output directories cannot already exist. Build failures stop before measurement.

The internal timer includes dispatch, setup, construction, copies, hashing/sums
and local destruction inside Run, but excludes process startup and printing.
Process wall time is recorded separately. This is a composite cost map, not
per-instruction latency. Native Hash conversion is borrowed; native argument
transport elsewhere remains part of the Wio path. Both object versions use
Wio's real intrusive Ref and explicit aliasing. The C++ builder is direct
std::string append with a matching snapshot; Result uses std::variant success,
a tagged-value **lower bound, not Wio's current object/identity/ABI model**.
Compiler optimization of reference loops is allowed and must not be attributed
entirely to one runtime operation.

Ordinary throwing new/new[] are replaced only in these standalone diagnostic
executables. Counters use relaxed atomics and record successful calls and the
sum of requested sizes while the timer is active. Instrumentation itself costs
time. It excludes malloc, aligned/nothrow-specific paths not routed through these
operators, deallocation/live bytes, copied bytes, RC events and RSS. Do not use
it for a VM-host allocation claim or link it into production. Future attribution
requires separate profiler/opt-in builds; the current runtime is untouched.

The new `wio_native_cost_contract` CTest qualifies counter reset/disable/byte
accounting, invalid arguments, fingerprints and all nine cases at counts 1/17
under both `-O0` and `-O2`. It has no timing threshold and is not the manual
benchmark. Only this new test is needed for the instrumentation slice.

## Controlled remeasurement without rebuilding

`run.ps1` completes source copying/C++ emission/compilation before pinning, then
delegates all measurement to `qualify.ps1`. Sources, copied headers, generated C++,
compiler/runtime/toolchain and std/runtime/SDK headers are hashed around the build;
mid-build changes refuse qualification. Existing frozen binaries can be measured
directly without using today's possibly modified std sources:

```powershell
pwsh -NoProfile -File tests/performance/native_costs/qualify.ps1 `
    -ArtifactDirectory F:/Projects/wio/build/native-cost-extended-plain-2026-10-15 `
    -OutputDirectory F:/Projects/wio/build/native-cost-controlled-new-run `
    -LogicalProcessor 2 -Repetitions 15
```

Suite, count and counter mode come from input `build.json`, or legacy `results.json`.
Recorded artifact hashes must match before launch. Missing/invalid modes, tampered
binaries, duplicate or nonlocal artifact names are rejected before measurement.
The original core provenance predates explicit suite/counter fields; only that
known instrumented format can be selected with `-LegacyInstrumentedCore`. It is
never guessed silently, and the override is recorded. Mode placeholders in
counter-free stdout must be zero, while JSON allocation fields remain **null**.

The common [measurement controller](../Measurement.psm1) pins its own process
before launch; Windows children inherit the mask before executing. It restores
the original mask in `finally`. This manual controller requires Windows x64 /
PowerShell 7 and rejects multiple processor groups. Use a dedicated `pwsh` process.
An explicit logical CPU index is not a claim that the CPU is physically isolated,
idle or fastest. No system power scheme, process priority or other application is
changed. See the [Mandelbrot guide](../mandelbrot_compute/README.md) for limits of
affinity, CPU-time/system-load snapshots and missing clock/thermal data.

`OutputsCorrect` requires all expected outputs and unchanged artifacts.
`TimingQualified` additionally requires unchanged endpoint power scheme, all
per-lane range/drift gates and the A/A median gate. Defaults are relative range
<=15%, chronological drift <=10% and A/A median difference <=10%, recorded in the
JSON. These are coarse noise limits, **not** confidence intervals, 5% accuracy or
proof that sessions are comparable. No trimming, automatic retries or threshold
relaxation. An unstable session keeps valid checksum/allocation evidence but
returns nonzero and does not qualify timing. Instrumented timing is still affected
by counters; do not subtract independent mode medians as exact counter overhead.

The separate new `wio_native_cost_measurement_contract` uses a synthetic native
protocol fixture on Windows to test acceptance/rejection, counters, corruption,
timeouts, large stdout/stderr and partial evidence without machine-speed gates.
Its reported durations are deliberately fake, never performance results.
Non-Windows runs test portable script parsing/protocol handling only, not Windows
affinity/process execution. This does not rerun the old workload correctness corpus.

Full plan and remaining coverage: [N01-N14](../../../docs/WIO_NATIVE_RUNTIME_PLAN.md).
