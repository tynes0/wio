# Wio benchmark library and wio-bench

`std::benchmark` is a Wio library. `main.wio` is the benchmark controller: manifest
validation, ordering, verification, statistics, checkpoints, comparison and final
Wio runs are Wio code. Native C++ implements only the OS/clock/allocator boundary.
Python bootstraps the tool and independently tests the implementation; it is not
the production sample collector. No shell or PowerShell is required.

## Build and run

With a configured source build:

```sh
cmake --build build --config Release --target wio_bench --parallel 1
```

The executable is in `build/tools/wio-bench/Release/` (configuration directory may
be empty for a single-config generator). `wio_dist` also builds and installs it.
Alternatively run the included `wio.makewio` project with a Wio installation that
contains this new std module/runtime header. An older installed release will not
automatically acquire this library.

Generate a real four-implementation example with explicit local tools:

```sh
python tools/wio-bench/bootstrap.py --output build/bench-example --example --wio build/app/Debug/wio.exe --runtime build/runtime/backend/libwio_runtime.a --cxx g++ --vm-host build/app/Release/wio-vm.exe --vm-config Release --cpu 2
```

Paths above are Windows source-build examples; pass the corresponding executable
paths on Linux/macOS. Bootstrap needs Python and the example needs a .NET 9 SDK.
It performs no downloads. `--runner PATH` reuses an explicitly built tool instead
of bootstrapping one. `--tool-opt=-O0|-O1|-O2` controls only the **controller** build;
the generated example recipes always use O2 native workloads and Release C#.

Bootstrap prints executable and manifest paths. Then:

```sh
wio-bench validate compute.bench.json
wio-bench run compute.bench.json --output results-before
# Make one scoped optimization, then rebuild via the same manifest recipe:
wio-bench run compute.bench.json --output results-after
wio-bench compare results-before/results.json results-after/results.json
wio-bench report results-after/results.json
```

An output directory must be new. Existing results are never overwritten. A failed
run keeps its `results.json` checkpoint (`complete=false` if interrupted). There
is no retry/trim/outlier-removal operation. `--skip-build` is explicit frozen-binary
remeasurement and should not be used to pretend changed sources were rebuilt.

Exit codes: **0** outputs and timing gates qualified, **1** invalid/incorrect/build
or I/O failure, **2** correct outputs but unqualified timing. Qualification uses
coarse range/drift/A/A checks, not a promise of 5% accuracy or universal speedup.
Power-profile observation is read-only. Where unavailable (currently macOS), timing
remains unqualified; correct output is still reported independently.
Child affinity must also be requested and applied for overall timing qualification;
leaving `cpu=-1` is valid for correctness/exploration, but cannot earn that label.
`statisticsQualified` records only the per-lane numerical/control checks, separately.
`--with-python` adds a fifth implementation to the generated example.
On Windows Store Python, bootstrap resolves the loaded interpreter image instead
of hashing/executing a zero-byte App Execution Alias.

## Required workflow for future optimization passes

1. Write one equivalent workload in Wio, C++ and C#. Use runtime inputs and an
   independent expected fingerprint and operation count. State arithmetic/FP,
   encoding, error, allocation, output and threading contracts explicitly.
2. Build Wio native and VM, C++ and C# with recorded versions/configuration/flags.
   Freeze source/runtime/toolchain/artifact hashes. Measure the **before** state.
3. Make the scoped optimization. Rebuild and measure the **after** state using
   the same inputs/contracts, keeping every raw sample and failure.
4. Each run ends with a **separate final phase** for Wio native and VM, including
   fresh warmups and native exact-command A/A controls. This is not pooled into
   the comparison samples or silently substituted for them.
5. Report speed and memory separately, including tradeoffs. A comparison command
   checks workload/checksum/operations/implementation/metric compatibility, but
   humans must also review sources and flags; matching output alone is not proof
   of equivalent semantics. Do not publish qualified speedups from rejected runs.

Builds are sequential and outside workload timings. Measurement order rotates
and reverses by round. Every native lane has an identical-command control lane.
Warmup samples remain in raw data but not summary statistics. Other languages do
not run in the final phase. Missing C#/VM cases are errors, not silent skips.

## Manifest schema 1

```json
{
  "schemaVersion": 1,
  "name": "buffer append before/after",
  "warmups": 1,
  "repetitions": 15,
  "finalRepetitions": 5,
  "timeoutMs": 30000,
  "outputLimit": 1048576,
  "cpu": -1,
  "environment": {"WIO_ROOT": "EXPLICIT_WIO_INSTALL_OR_SOURCE_ROOT"},
  "maximumRange": 0.15,
  "maximumDrift": 0.10,
  "maximumControlDifference": 0.10,
  "metadata": {"revision": "record me", "flags": "record exact flags and contracts"},
  "versions": [],
  "builds": [],
  "artifacts": ["source.wio", "source.cpp", "Program.cs", "runtime.a", "module.wiob", "bridge.dll"],
  "cases": []
}
```

Every workload group in `cases` must include all four `implementation` values:
`wio-native`, `wio-vm`, `cpp`, `csharp`. Additional implementations such as
`python` are supported in comparison rounds; they do not enter the final Wio phase.
A case is:

```json
{
  "name": "append-native",
  "implementation": "wio-native",
  "workload": "append-v1",
  "program": "ABSOLUTE_EXECUTABLE_PATH",
  "args": ["2000"],
  "cwd": ".",
  "metric": "kernel",
  "operations": 2000,
  "expectedChecksum": "INDEPENDENT_EXPECTED_CHECKSUM"
}
```

Names must be unique. A case's group has identical `workload`, `metric`,
`operations` and `expectedChecksum`. Arguments are literal strings, never a shell
command. `program` is an absolute executable path. Relative cwd/artifacts resolve
against the manifest folder. Build/version commands have the same
`program/args/cwd` shape; build commands may override `timeoutMs` up to one day.
Example generation uses fully resolved paths and records tool versions.
Optional `environment` contains string-valued overrides for this serial runner
and its child commands only; no user/system environment is persisted. Generated
examples explicitly select their Wio root, so a stale installed `WIO_ROOT` cannot
silently select a different std. These declared values are saved in the report:
do not put secrets in a report intended for publication.

List all relevant source/include/runtime/bridge/module/library artifacts, not
just executables. The controller also automatically fingerprints itself and all
command executable paths. Hashes are checked again at the end. SHA256 hashing is
outside measurement, currently bounded to 512 MiB per file and reads the file
into memory; it is not a streaming hasher. Use representative bounded artifacts.

Limits: 1..128 cases, 1..100 warmups, 3..10000 repetitions, up to 16 MiB captured
output, positive bounded deadlines. CPU `-1` leaves affinity unchanged; other
values request child-only affinity before execution (Windows/Linux). Unsupported
or denied affinity is a failure, never silently ignored. This changes no power
policy, priority, system-wide affinity or other applications.

### Workload output protocol

One ASCII JSON object on stdout, no logging:

```json
{"schemaVersion":1,"elapsedNanoseconds":12345678,"operations":2000,"checksum":"123456789"}
```

`elapsedNanoseconds` is a positive integer. `checksum` is a string to preserve
64-bit integer precision. Extra fields (e.g. managed allocation counters) are
retained as payload, not conflated with native allocation counters. Wrong output,
exit status, timeout or limits reject correctness. stderr is retained separately.

`metric=kernel` uses the workload's timer and excludes process startup. `wall`
uses child launch-to-exit time and includes startup, cleanup and <=1ms wait polling.
CPU time and peak RSS refer to the root child process, not the full process tree
or the timed kernel. Windows jobs/POSIX process groups clean up ordinary child
workers. This is **not a sandbox** for hostile programs; POSIX daemons can escape
their group. Captured stdout/stderr use temporary files, not blocking pipes. The
output cap is checked while polling, so disk writes can overshoot it between
checks; returned capture strings remain bounded. Raw bytes are represented as
Latin-1 code points (`stdoutEncoding=byte-as-latin1`); protocol itself is ASCII.

Fresh processes mean external warmups do **not** warm a later process's JIT.
C# must warm its actual method in the same process or explicitly use AOT. The
included C# recipe disables tiered compilation and all implementations execute
two in-process warmups before their reported compute kernel. The operation count
is a runtime argument and Python computes the expected recurrence independently.

## Wio library usage

```wio
use std::benchmark as bench;
use std::console as console;

fn Sum(count: usize) -> u64
{
    mut value = 0u64;
    for (index in 0usize..<count)
    {
        value += index fit u64;
    }

    return value;
}

fn Entry() -> i32
{
    let measured = bench::Run("sum", 1000usize, 3usize, 15usize, Sum, 499500u64);
    if (measured.IsError()) return 1;
    let report = measured.Value();
    console::Print(report.NanosecondsPerOperation());
    return 0;
}
```

`Run` validates every checksum including warmups. It supplies the iteration count
through `Input` and consumes the checksum before the end timestamp. `Input(u64)`,
`Input(i64)` and `Input(f64)` use thread-local volatile transport to prevent reuse
of a previous pure computation; `Consume(u64)` adds an observable atomic sink.
Custom protocol workloads should use both boundaries INSIDE their timed interval.
The C++ example uses volatile input/result and C# uses a non-inlined method plus
volatile result consumption. These small boundary costs are included, not
subtracted. None is a universal barrier against constant folding INSIDE an
arbitrary callback; inspect generated code and use meaningful runtime inputs.

`Calibrate(action, targetNanoseconds, maximumIterations)` doubles batch size until
the target is reached, or returns an error at the cap. Calibration is preparation,
not an accepted sample and not a license to adapt iterations differently in the
middle of a paired comparison. Side-effectful callbacks need fresh/reset state.

`Analyze` / `AnalyzeWithLimits` return `Result<Statistics>`: min/max, mean,
median, linear-interpolated p90/p95/p99, sample standard deviation, standard
error, median absolute deviation, relative range and early/late median drift.
Raw samples are never trimmed. Mean 95% intervals use a **normal approximation**,
not a small-sample Student-t/bootstrap confidence guarantee. `SummaryJson` emits
all fields. `Protocol` formats a workload result; call it **after** the timed kernel.
The explicit sorted copy protects the original sample ordering (needed for drift).
`Percentile` validates its inputs and supports any fraction in [0,1]; `ReportJson`
exports a library report with raw samples. JSON f64 output is locale-independent,
round-trip precision, including real numeric zeros (not null).

## Memory: three distinct kinds

1. `Memory()` reads this process's resident/peak/private bytes with separate
   availability flags. Peak is lifetime high-water, not resettable per call.
   Windows provides working set/peak/private commit; Linux provides approximate
   `/proc/self/status` RSS/HWM (private bytes unavailable); macOS provides current
   resident and lifetime peak (private bytes unavailable).
2. The process controller records the child's CPU time and peak RSS independently
   of workload output. Missing values are JSON null, not fabricated zero.
3. Optional `AllocationSnapshot` counts C++ new/delete requests, cumulative
   requested/freed bytes, live/peak requested bytes/blocks and allocation failures.
   This requires a **separate diagnostic executable**, never the plain timing one.

To enable allocation diagnostics in one generated C++ translation unit, compile
it with `-DWIO_BENCHMARK_ALLOCATIONS` (`std_benchmark.h` installs the opt-in global
operators). Do not enable for shipping binaries, shared libraries, SDK hosts or
multiple TUs. The replacement allocator adds metadata/locking and changes layout;
its timings/RSS are not production timings/RSS. Ordinary/aligned/array/sized and
nothrow paths are covered. Direct malloc/custom allocators and managed heaps are
not counted. VM child RSS works normally; bridge-only instrumentation does **not**
measure the VM host's heap and is not an advertised VM allocation profiler.
`MemoryJson` / `AllocationJson` export these diagnostics with explicit scopes and
availability; do not subtract lifetime peaks and call that a per-kernel peak.

```wio
if (bench::BeginAllocations().IsError()) return 1;
// Perform scoped work; return only a primitive checksum, release owned buffers.
let checksum = Sum(1000usize);
let counters = bench::EndAllocations();
bench::Consume(checksum);
// Format/report counters AFTER EndAllocations, never inside the measured window.
```

Windows are process-global. Prepare worker threads first and join/synchronize
before ending; otherwise concurrent allocations are attributed to the same
window. Nested windows and reset with outstanding tracked blocks are rejected.
Frees after End still update the old window; a new window opens only once live
blocks reach zero. Allocations before Begin are ignored when freed in the window.
Begin prepares its Result success object before activation to avoid counting that
transport. Keep reporting/fixture allocations outside the window. End explicitly
on every path; this API does not auto-catch arbitrary callback failures.
The window counts generated Wio bookkeeping too: binding the Begin result to a
long-lived local can create a tracked local cell after activation. Consume that
Result immediately as above when measuring only the scoped worker; do not hide
such cells from whole-program allocation accounting.

`examples/memory.wio` is a complete scoped append/snapshot/hash diagnostic. Emit
its C++ into a scratch folder, compile that one file with the opt-in macro and
your usual runtime/include/link settings, and run it. Without the macro it exits
2 with a clear instruction; with it, it verifies the fingerprint and zero live
blocks, then prints allocation and before/after OS memory JSON outside the window.
Use `-O0` for diagnostic attribution if desired; never call its instrumented
timing or allocator metadata overhead production performance.
On Windows MinGW, include `-Wa,-mbig-obj` for large O0 generated translation units;
the supplied bootstrap, CMake target and contract-test compiler already do this.

## Verification

```sh
ctest --test-dir build -C Debug -R "^wio_benchmark_contract$" --output-on-failure --no-tests=error -j 1
```

Only the new test: Wio native O0/O2/VM, diagnostic native, independent allocator
calibration, shell-free argv/NUL/invalid-byte capture, timeout/output/nonzero
failures, strict manifests, immutable outputs, failure retention, final/control
phases and a real C++/C#/Wio native/VM comparison with a Python checksum oracle.
Missing required .NET/VM tools cause a test failure, not a silent pass. CI installs
.NET 9 and uses its configured VM host; local acceptance can explicitly select a
Release host. Timing qualification and functional test success are independent.
