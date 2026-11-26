# Native Integer Interpolation and Per-Update Measurements

Date: 2026-10-15. Implementation: `9554b63`. This is a bounded continuation of
native/runtime optimization pass 1, not the VM heavy pass or final budget closure.

Follow-up: [general direct formatting and observing string copies](WIO_NATIVE_STRING_FAST_PATHS.md)
records `d6cdbd3`, long-string allocation/time improvement and fresh Mandelbrot
native/VM measurements. Both its initial adverse native delta and subsequent
balanced recheck are retained; this earlier report's figures remain historical.

## Implementation and correctness

- Generated native integer stringify uses installed `integer_format.h`: bounded
  stack storage and `std::to_chars` for classic-locale decimal conversion,
  without constructing an ostringstream per integer. Nonclassic global locales
  keep the stream path, including custom numeric grouping. Signed minima and
  unsigned maxima do not pass through signed negation or a narrowing conversion.
- Existing bool, byte/signed-byte, char, float, object/container and Unicode
  formatting overloads keep their contracts. This is not a float formatting or
  locale-policy redesign. The VM already uses `to_chars` for integer interpolation
  and is unchanged by this native fast path.
- Native interpolation accumulates pieces into one string rather than chaining
  string addition. Literal pieces carry explicit byte lengths. This also fixes
  an existing truncation of embedded NULs in interpolated literal prefixes and
  suffixes; ordinary length-aware string constants were already preserved.
  Parts are appended in source order; value and text results remain owning
  snapshots, with the existing checked UTF-8 conversion for text.
- The owning string argument allocation identified in the previous slice is
  not silently removed. Eliminating it requires a separate alias/call-use proof;
  changing a public value parameter to an observing borrow is not assumed safe.

Only two newly introduced correctness gates run locally:

| Gate | Scope |
| --- | --- |
| `wio_integer_format` | All integer widths, zero/sign/extremes, 4,096 deterministic pseudo-random triples, classic and custom grouped locales against stream output |
| `wio_wir_integer_interpolation` | Real generated native O0/O2 and bytecode VM: integer extrema, bool/float fallback, Unicode, NUL prefixes/suffixes, nested calls/interpolation and long multi-part strings |

Final Windows Debug local run: **2/2 passed in 5.31 seconds**. The source fixture
was first corrected to avoid calling unavailable builtins under `--no-builtin`;
its NUL assertion then failed against the old concatenation path and passed
after the length-aware accumulation fix. Earlier failed attempts are not counted
as successful qualification. The older corpus was not rerun. Diagnostic
reference check remains 693 IDs, three dynamic-only entries and 49 committed
std Result construction sites. Clean CI at `9554b63` builds the actual CLI and
passes both new tests on all three platforms:

| Platform/configuration | New tests | CTest duration | Evidence |
| --- | --- | ---: | --- |
| Windows / Debug | 2/2 passed | 8.18 s | [job](https://github.com/tynes0/wio/actions/runs/38714046268/job/116352256641) |
| Linux / Release | 2/2 passed | 3.33 s | [job](https://github.com/tynes0/wio/actions/runs/38714046268/job/116352256531) |
| macOS / Release | 2/2 passed | 4.16 s | [job](https://github.com/tynes0/wio/actions/runs/38714046268/job/116352256368) |

The [complete run](https://github.com/tynes0/wio/actions/runs/38714046268)
succeeds: **6/6 new test executions**. These are clean correctness checks, not
remote benchmark results or qualification of the older full corpus. The manual
diagnostic benchmark host is built/measured locally in MSVC Release; this CI
does not claim fresh cross-platform Mandelbrot throughput or benchmark-host runs.

## Measurement policy: each update, not only each pass

The requested reporting cadence now means every implementation update ends
with newly measured Wio native and Release VM rows for this same Mandelbrot
case, retaining dated C++/C#/Python references. Pure explanations do not invent
new samples. Rebaseline references if workload/toolchain/measurement policy
changes. The all-language final requalification after the three passes remains.

Versioned runner:
[`progress.ps1`](../tests/performance/mandelbrot_compute/progress.ps1), with
[`vm_host.cpp`](../tests/performance/mandelbrot_compute/vm_host.cpp) as a
Release, non-JIT diagnostic host. Its explicit finite 10-billion instruction
budget is necessary for this full-sized workload; the production runner's
10-million default is unchanged. Verification, native ownership, checked
operations and other machine limits remain enabled. The host is excluded from
default builds/install; no new production SDK/CLI promise is introduced.

```powershell
cmake --build build --config Release --target wio_mandelbrot_vm_benchmark
pwsh -File tests/performance/mandelbrot_compute/progress.ps1 `
    -OutputDirectory F:/Projects/wio/build/mandelbrot-progress-new-run `
    -FullSourceDirectory F:/Projects/wio/build/mandelbrot-ddfa2b2 `
    -Repetitions 5 -VmRepetitions 1 -TimeoutSeconds 180
```

Compute source is versioned; the optional full-render snapshot is currently
retained in the ignored audit directory, not a clean-checkout fixture bundle.
The runner snapshots it before emission, keeps hashes, requires a fresh output
directory and never modifies the user playground/output. Current native and
the saved previous generated full source are recompiled against the same
headers/runtime/toolchain, not executed as stale binaries. Compilation is
outside execution timing; generated size/build cost remains pass 3 work.

Environment: Intel Core i7-13620H, Windows build 26100; native GCC 15.2.0 C++20
`-O2 -ffp-contract=off -static`, no fast-math; VM host/runtime are MSVC Release.
Wio compiler is the local Debug host; that does not make the generated program
or VM an unoptimized Debug executable. Power/affinity is not pinned. Local
generation retains unrelated pre-existing user std worktree edits; clean remote
correctness evidence is separate. Five native measured samples follow one
excluded warm-up; each VM row is **one separate-process observation**, without
an explicit VM warm-up, not a statistically qualified median or cold-cache claim.

## Fresh Wio results

800 x 800, maximum 100 iterations, strict f64 recurrence. Compute-only checksum:
`11613930/3721766287256/94440`. No strings/file writes are inside its kernel
timer; native clock calls surround it in both backends. Full PPM includes
computation, interpolation, builder output, console messages, write/close and
startup/VM loading in external wall time. Hash verification is outside the timer;
writes use normal OS caches, not durable disk synchronization.

| Fresh Wio measurement | Value | Range / samples |
| --- | ---: | --- |
| Native compute kernel | 23.300 ms | 22.786–24.023 ms; 5 |
| Release VM compute kernel | 19.0023835 s | 1 observation |
| Native full PPM, previous saved `ddfa2b2` source | 250.618 ms | 249.086–252.327 ms; 5 |
| Native full PPM, current `9554b63` | 128.437 ms | 126.275–131.305 ms; 5 |
| Release VM full PPM | 20.5356141 s | 1 observation |

Native full output is **1.951x faster**, **48.75% less wall time**, versus the
last measured full-render generated snapshot in the same session. That snapshot
precedes both the closed-string storage slice and this formatting slice; this
combined gain must not be attributed solely to integer conversion. There was
no saved `79568bd` full-render source, so an immediate-previous-commit full
speedup is not claimed. The prior report's 247.528 ms belongs to a different
session; its recompiled paired value here is 250.618 ms.

The earlier native compute median was 23.163 ms; the numeric kernel is unchanged
by this slice, and no compute speedup is claimed from the new 23.300 ms sample
set. VM runtime source is unchanged here too: 19.00/20.54 seconds is the first
explicit Release VM baseline in this progress runner, not a measured regression
or VM optimization gain. Its substantial overhead is openly retained for pass 2.

Every completed full render has 5,156,528 bytes and SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.
All 20 warm-up/measured executions pass their respective checksums; no timeouts.
Source/generated/bytecode/bridge/compiler/runtime/host/binary hashes and samples:
[raw current evidence](evidence/mandelbrot-progress-integer-2026-10-15.json).

## Fixed reference comparison

References below are **2026-10-04 historical baselines**, not rerun or paired
with today's Wio samples. Versions: GCC 15.2.0 `-O2 -ffp-contract=off`, .NET 9
Release (SDK 9.0.311/runtime 9.0.13; warmed non-inlined compute kernel), plain
CPython 3.12.14. Same dimensions/iteration/checksums. C++ uses `to_chars` and a
pixel buffer, C# uses StringBuilder, Python formatted lines/join; the Wio full
program interleaves computation and append. These are equivalent output tasks,
not identical storage algorithms. Compute and full columns are different metrics
and must not be subtracted to manufacture a Wio formatting-only time.

| Implementation | Compute-only kernel | Full PPM external wall |
| --- | ---: | ---: |
| C++ fixed reference | 28.405 ms | 76.02 ms |
| Wio native, fresh | 23.300 ms | 128.437 ms |
| Wio Release VM, fresh | 19,002.384 ms | 20,535.614 ms |
| C# fixed reference | 29.586 ms | 149.13 ms |
| Python fixed reference | 1,348.041 ms | 1,613.71 ms |

Do not infer an exact current native-vs-C++ budget or universal cross-language
speed ranking from references collected on a different day. The historical
full-C++ 76.02 ms also differs from the later 55.233 ms session, illustrating
why dates/measurement provenance cannot be discarded. Fixed reference compute
raw samples and full-output documented medians are preserved in
[reference evidence](evidence/mandelbrot-reference-baselines-2026-10-04.json),
with the original [audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md) as provenance.

Remaining pass-1 work includes owning argument/alias proofs, direct numeric
append/buffer output, ordinary +=, managed/container/async costs and coordinated
std modules. VM dispatch/value/place/call costs belong to pass 2; compiler/build
throughput belongs to pass 3. No 5–10% whole-program budget, release, broad old
corpus pass or completion of all optimization work is claimed.
