# Compute-Only Mandelbrot Benchmark

This manual diagnostic benchmark isolates scalar numeric code from image
formatting, strings, buffers, containers, and file I/O. It is not registered as
a default CTest correctness test: .NET/Python are optional benchmarking tools,
and performance qualification should not slow every developer test run.

Requirements: Windows, PowerShell 7, a built Wio Debug compiler and backend
runtime, MinGW GCC, .NET SDK 9, and Python 3. The script does not rebuild the
compiler or change any compiler/runtime/std implementation.

From the repository root:

```powershell
pwsh -File tests/performance/mandelbrot_compute/run.ps1 -Python C:/path/to/python.exe
```

Options include `-Repetitions`, `-BuildDirectory`, `-CppCompiler`, `-DotNet`,
and `-TimeoutSeconds`. Outputs go to `build/mandelbrot-compute-audit` by default:
generated code, executables, C# intermediates, `measurements.json` with every
sample, and `summary.json` with medians/ranges. The source fixture is copied
there before emission; no generated files are left beside versioned sources.

The kernel computes the same 800 x 800, 100-iteration-limit double-precision
Mandelbrot grid in all four languages. It returns three observables:

- sum of iteration counts: `11613930`;
- sum of `iteration * (pixelIndex + 1)`: `3721766287256`;
- number of pixels reaching the iteration limit: `94440`.

No native function is called per pixel or per iteration in Wio. Native clock
calls surround the kernel; reporting/formatting happens after the end timestamp.
`--no-builtin` excludes unrelated std imports. C++ and generated Wio C++ use
the same GCC, `-ffp-contract=off`, and paired `-O0`/`-O2` flags, without fast-math.
C# uses an optimized, non-inlined method warmed with a different 32 x 32 input
before timing. Python is plain CPython without NumPy or other native kernels.

Kernel time excludes compilation, startup, result formatting, and C# warm-up.
External wall time includes process startup, reporting, and that warm-up. The
average nanoseconds-per-step metric divides the entire kernel time by the
iteration count: it includes coordinate setup, escape tests, and checksums; it
is not the latency of an individual arithmetic instruction or a pure loop-only
measurement. Numerical results must match before speed comparisons are accepted.

## Per-update Wio measurements

`progress.ps1` rebuilds/measures only Wio native and a Release non-JIT VM host,
leaving the dated C++/C#/Python baselines fixed for progress reports. Build
`wio_mandelbrot_vm_benchmark` in Release first. This diagnostic-only host uses a
finite 10-billion-instruction budget for the full-size workload, rather than
changing the production runner's 10-million default. Decoding, verification,
native ownership and other machine checks remain enabled. The host is excluded
from the default build/install and is not a new production CLI.

```powershell
cmake --build build --config Release --target wio_mandelbrot_vm_benchmark
pwsh -File tests/performance/mandelbrot_compute/progress.ps1 `
    -OutputDirectory F:/Projects/wio/build/mandelbrot-progress-new-run `
    -FullSourceDirectory F:/Projects/wio/build/mandelbrot-ddfa2b2 `
    -Repetitions 9 -VmRepetitions 3 -TimeoutSeconds 180 -LogicalProcessor 2
```

The compute fixture is versioned here. Optional full-output input uses a frozen
`main.wio` plus `mandelbrot.wio` snapshot; a saved `main.wio.cpp` also enables
paired previous/current native full-output samples. The frozen full source is
currently retained in the ignored local audit directory, not a standalone
clean-checkout fixture bundle. Do not point this option at a changing playground
and claim it is the same experiment. All copied source/generated/bytecode/bridge/
binary hashes are preserved. Ordinary Windows GCC/runtime linkage matches the
earlier native experiments; the VM itself is an optimized MSVC Release build.

Both native and VM now have one explicit excluded warm-up per case. Each measured
sample is a separate process; three measured samples per backend are the minimum,
not a precision guarantee. Kernel time excludes VM setup; full-output wall time
includes it. Build provenance is saved in `build.json`; raw timing evidence is in
`qualification/results.json`. Failure/timeout samples are saved before rejection.
No .NET/Python installations/runs or old correctness corpus are required here.

## Controlled same-binary remeasurement

To isolate environmental variability from recompilation, use existing frozen
artifacts directly. **Nothing is rebuilt**, including native bridges or the VM:

```powershell
pwsh -NoProfile -File tests/performance/mandelbrot_compute/qualify.ps1 `
    -ArtifactDirectory F:/Projects/wio/build/mandelbrot-progress-n01-extended-2026-10-15 `
    -OutputDirectory F:/Projects/wio/build/mandelbrot-controlled-new-run `
    -LogicalProcessor 2 -Repetitions 9 -VmRepetitions 3
```

This controller is Windows x64/PowerShell 7 only. Run it in a dedicated `pwsh`
process. It pins its own process **before** launching children, so affinity is
inherited at process creation instead of racing an already-running short kernel.
The original controller affinity is restored in `finally`, including failures.
Only its own process tree is affected: no power-plan, priority, other-process,
security or machine configuration changes. Multiple processor groups are rejected
explicitly. `-LogicalProcessor -1` selects the first allowed logical processor;
this is a reproducible fallback, **not** an assertion that it is a performance core,
idle, physically isolated or fastest. Choose an explicit allowed index and keep it
constant across comparisons. Do not search CPUs/rerun until attractive results.

Case order rotates and reverses by round. `compute-native-control` executes the
**identical executable** as `compute-native` in a second A/A lane. All input/binary
hashes are captured before and after; every checksum is checked. Output files
must be fresh, not inherited from a previous sample. A `-NativeOnly` diagnostic
is available, but cannot qualify VM behavior.

Acceptance requires every sample/checksum, unchanged artifacts and endpoint power
scheme, >=3 samples per case, relative range `(max-min)/median <= 15%`, first/last
half-median drift <=10%, and A/A median difference <=10%. Thresholds are explicit
options recorded in evidence. **These are coarse noise gates, not 5% accuracy or
statistical confidence guarantees.** No outlier removal or automatic reruns are
performed. Failed qualification returns a nonzero exit and retains raw results;
rejection is not a compiler regression. A passed run alone does not establish
cross-session equivalence or causal attribution of an optimization.

Telemetry records each child CPU time, CPU/wall ratio and aggregate system busy
fraction during the process, plus endpoint power/OS/processor data. CPU-time ticks
are coarse for short cases; aggregate load is not the chosen core's utilization.
Unavailable CIM data is null with its error recorded, not guessed. Snapshot clock
information is not effective workload frequency; temperature, throttling and
physical-core/SMT isolation are not measured. Endpoint power checks do not prove
the scheme stayed constant throughout. Pinning alone cannot remove contention,
frequency changes or OS activity. Historical cross-language rows remain dated
context, not current controlled speed ratios.

Windows inheritance contract: [SetProcessAffinityMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setprocessaffinitymask).

See [`../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md`](../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md)
for the measured baseline, limits, and optimization priorities.
