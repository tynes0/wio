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
    -Repetitions 5 -VmRepetitions 1 -TimeoutSeconds 180
```

The compute fixture is versioned here. Optional full-output input uses a frozen
`main.wio` plus `mandelbrot.wio` snapshot; a saved `main.wio.cpp` also enables
paired previous/current native full-output samples. The frozen full source is
currently retained in the ignored local audit directory, not a standalone
clean-checkout fixture bundle. Do not point this option at a changing playground
and claim it is the same experiment. All copied source/generated/bytecode/bridge/
binary hashes are preserved. Ordinary Windows GCC/runtime linkage matches the
earlier native experiments; the VM itself is an optimized MSVC Release build.

Native has an excluded warm-up; VM repetitions are explicit separate-process
samples with no hidden warm-up. One VM sample is an observation, not a robust
median. Kernel time excludes VM setup; full-output wall time includes it. A
timeout is retained without a checksum/success claim or fabricated exact runtime.
No .NET/Python installations/runs or old correctness corpus are required here.

See [`../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md`](../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md)
for the measured baseline, limits, and optimization priorities.
