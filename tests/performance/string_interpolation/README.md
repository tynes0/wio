# Long-string interpolation allocation diagnostic

This Wio fixture feeds 30,000 interpolated copies of a 32-byte fragment to the
builder. Output is the existing 960,000-byte builder fingerprint
`2058931307218008101`. Unlike the original literal-only append fixture, this
exercises borrowed string operands and interpolation's string conversion.

Use the existing native manual runner with a fresh output directory:

```powershell
pwsh -File tests/performance/std_builders/run.ps1 `
    -FixturePath F:/Projects/wio/tests/performance/string_interpolation/append.wio `
    -OutputDirectory F:/Projects/wio/build/string-interpolation-new-run `
    -BaselineDirectory F:/Projects/wio/build/string-fast-paths-baseline `
    -Repetitions 15
```

The baseline directory must hold this same fixture's saved pre-change generated
`append.wio.cpp`; do not substitute the literal-only append snapshot and claim
it measures interpolation. The runner records the selected fixture hash and
recompiles both snapshots against the current toolchain/runtime. Output bytes,
hash and counted allocations are validated in every run. C++ reference is the
existing direct append lower-bound implementation, not an identical intermediate
string-materialization algorithm. The paired Wio snapshots isolate this change.

This is a manual native diagnostic, not a default CTest or a VM allocation
measure. The regular per-update Mandelbrot runner separately measures native
and Release VM numeric/full-output workloads and retains their checksums.
