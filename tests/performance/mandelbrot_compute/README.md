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

See [`../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md`](../../../docs/WIO_MANDELBROT_PERFORMANCE_AUDIT.md)
for the measured baseline, limits, and optimization priorities.
