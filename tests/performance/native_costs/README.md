# Native cost map (N01)

Actual workloads are Wio (`costs.wio`, `extended/costs.wio`); Python performs
build/process/measurement orchestration. Requires Python 3, a built Wio compiler,
backend runtime and compatible C++20 compiler. It changes no production implementation. Sources
are copied to a fresh ignored build directory before C++ emission; user playground
files and outputs are never used. Do not benchmark while another build/test runs.

The default `-Suite core` remains the original nine cases. `-Suite extended`
selects six additional workloads; see [their boundaries](extended/README.md).
`--disable-allocation-counters` compiles out replacement allocation operators for
counter-free timing. The JSON marks allocation metrics unmeasured, not zero;
the default instrumentation mode and original core workload sources are unchanged.

```sh
python tests/performance/native_costs/run.py --output build/native-cost-new --repetitions 15 --count 2000 --cpu 2
```

Nine independently selected workloads: ordinary string `+=`, StringBuilder,
integer interpolation + builder, ByteWriter, dynamic array, component return,
object construction/alias/method, native scalar call, Result success/unwrap.
The count is runtime input, bounded to 1..100000. Large concat counts can be
quadratic in the current implementation; do not assume every count is cheap.

Both executables use C++20 `-O2 -ffp-contract=off`; no fast-math. Windows adds
static/large-object flags and system libraries; Unix uses `-pthread`.
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

`run.py` completes source copying/C++ emission/compilation before pinning, then
delegates all measurement to `qualify.py`. Sources, copied headers, generated C++,
compiler/runtime/toolchain and std/runtime/SDK headers are hashed around the build;
mid-build changes refuse qualification. Existing frozen binaries can be measured
directly without using today's possibly modified std sources:

```sh
python tests/performance/native_costs/qualify.py --artifacts build/native-cost-extended-plain-2026-10-15 --output build/native-cost-remeasured --cpu 2 --repetitions 15
```

Suite, count and counter mode come from input `build.json`, or legacy `results.json`.
Recorded artifact hashes must match before launch. Missing/invalid modes, tampered
binaries, duplicate or nonlocal artifact names are rejected before measurement.
The original core provenance predates explicit suite/counter fields; only that
known instrumented format can be selected with `--legacy-instrumented-core`. It is
never guessed silently, and the override is recorded. Mode placeholders in
counter-free stdout must be zero, while JSON allocation fields remain **null**.

The common [Python measurement controller](../measurement.py) pins its own process
before launch; children inherit affinity before executing. It restores the
original mask on all exits. Hard-affinity qualification supports Windows x64 /
single processor group and Linux. macOS needs explicit `--allow-unpinned` for
correctness collection; timing stays unqualified. Missing power-policy evidence
also prevents timing qualification, not correctness collection. Run through a
dedicated Python process. `--wio`, `--runtime`, `--cxx` override build paths;
CTest supplies configured paths. Use `python3` if that is the installed command.
The `.ps1` files now contain forwarding only; no test/build logic lives there.
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

The new `wio_native_cost_python_contract` tests acceptance/rejection, counters,
corruption, timeouts, descendant cleanup, large stdout/stderr and partial evidence
without machine-speed gates. Synthetic durations exist only in injected Python
unit mocks, never executed Wio benchmarks. It then **compiles and runs both actual
Wio suites**, at count 17: 128 checked executions including independent extended
fingerprints and counter-free null fields. The same test runs on all three CI
platforms, not just a syntax check. It does not rerun the old correctness corpus.

Worker startup is excluded from child wall time (which still includes launch,
waiting/drain and controller overhead); do not compare it directly with old wall
samples. Internal reported kernel time is unchanged. Windows workers own a
kill-on-close job before spawning; POSIX workers own a process group. A timeout
retains partial stdout/stderr and terminates that owned tree. Windows contract:
[job lifetime](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_limit_information).

```sh
python tests/performance/native_costs/test_measurement.py --wio PATH --runtime PATH --cxx CXX
ctest --test-dir build -C Debug -R "^wio_native_cost_python_contract$" --output-on-failure
```

`--unit-only` skips Wio compilation. Old Mandelbrot PS tooling is still legacy;
this slice does not claim to replace every repository script. New behavior tests
should prefer Wio, with Python for orchestration.

Full plan and remaining coverage: [N01-N14](../../../docs/WIO_NATIVE_RUNTIME_PLAN.md).

For opt-in RC and Ref copy/move attribution without runtime source changes, see
[the Wio/Python ownership profiler](ownership/README.md). Its calibrated gcov
diagnostic build is separate from ordinary O2 output checks and from timing
acceptance; handle-transfer counts are not copied payload bytes.

For live/peak requested bytes, aligned allocation/free accounting and process
resident-memory checkpoints, use the separate
[Wio/Python memory diagnostic](memory/README.md). Its replacement allocator is
never linked into these ordinary allocation/timing probes or production code.

For observed memcpy/memmove request byte lengths in string/text/component
transport and concat, see [the copy diagnostic](copies/README.md). It has its
own calibrated GCC/linker build and ordinary O2 output-parity checks; it is not
a complete payload-copy profiler or a throughput measurement.
