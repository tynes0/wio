# N02 local append: measure, optimize, measure

`bench.wio` is the measured workload; `semantics.wio` is the behavioral test.
Python only orchestrates builds, processes, independent fingerprints and reports.
The production change is in `WirCppStoragePlan` and the C++ backend, not these
timers. The VM is a semantic cross-check, not the target of this optimization.

Build a compiler from before the change and freeze a baseline in a fresh folder:

```sh
python tests/performance/append_reuse/run.py before --output build/append-pair --wio PATH_TO_BEFORE_WIO --runtime PATH_TO_RUNTIME
```

After rebuilding the changed compiler, use the same runtime and benchmark sources:

```sh
python tests/performance/append_reuse/run.py after --output build/append-pair --wio PATH_TO_AFTER_WIO --runtime PATH_TO_RUNTIME
python tests/performance/append_reuse/run.py measure --output build/append-pair --count 2000 --repetitions 15 --cpu 2
```

Pass `--cxx` when the default compiler is not on PATH. Defaults are local Windows
build paths, not a promise of a prebuilt compiler on other hosts. Build directories
are immutable and must be new. The measurement command writes `results.json`;
preserve it before repeating collection for a documented methodological reason.
Do not repeat until the desired number passes.

Five rotating/reversing lanes include an exact before-string A/A control. Plain
durations include append, hash and cleanup, but exclude process startup and source
construction. Memory runs use a separate replacement-allocator binary. Allocation
counts, cumulative requested bytes, live bytes and peak bytes are different metrics;
they are not all-copy attribution or shipping process RSS. Final tracked live
bytes/blocks must be zero. Every result is checked independently including NUL and
UTF-8 bytes. Raw samples, hashes, endpoint power state and rejection reasons remain.

Only the new semantic and optimizer-proof tests:

```sh
ctest --test-dir build -C Debug -R "^wio_n02_append_(reuse|plan)$" --output-on-failure --no-tests=error -j 1
```

The Wio fixture runs as native O0, native O2 and VM. It covers empty/SSO/growing
strings, snapshots, self-append, NUL, Unicode, RHS mutation, a borrowed parameter
and a component field. The C++ unit tests check selection and rejection boundaries.

`track.py` is a Windows-local rebuild helper for the existing frozen Mandelbrot
experiment. It rebuilds native outputs with current Wio, explicitly reuses the
old VM bytecode/bridge and records that provenance in `build.json`. The common
`mandelbrot_compute/progress.py` controller can then measure that directory. Its
historical `Stage=N01`/`Rebuilt=false` fields describe the collector, not the
companion N02 native rebuild. It is not the portable library interface.
