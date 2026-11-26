# Native Direct Formatting and Observing String Copies

Date: 2026-10-15. Implementation: `d6cdbd3`. This continues native/runtime
optimization pass 1 for general string workloads, not a Mandelbrot-specific
recognizer or completion of all native performance work.

## General transformations

Interpolation now appends built-in values directly into its output string:

- String and text append their bytes without a second stringify-owned string.
- Integer decimal digits go from a bounded stack buffer into the target, rather
  than constructing a separate formatted string first. `AppendInteger` also
  preserves the target's existing prefix/capacity. `StringifyInteger` remains an
  owning convenience wrapper around the same operation.
- Bool/char append their existing textual/byte representations. Enum delegates
  through its underlying type. Byte/signed-byte retain the existing numeric
  stringify overload; floating-point and object/container fallback remain.
- Nonclassic global integer locales retain stream fallback/grouping, rather
  than silently changing native host behavior. Literal NUL lengths and checked
  Unicode text construction remain as qualified by the preceding slice.

The backend also generalizes the adjacent borrowed-string-copy proof. A sole-use
owned copy immediately between a borrowed load and a known observing sink may
be emitted as that borrow. This covers string addition/comparison, the existing
self-alias-safe Append suffix, and an explicit whitelist of read-only search,
slice/trim/split/line/repeat/pad/count operations. The proof now works inside
individual blocks of a multi-block function; it does not move values across CFG
edges. Use counts include all blocks, branch arguments and borrow-origin uses.
An unrelated adjacent operation is not enough: the sink must use that copy.

Unknown calls, unlisted mutators, capture/borrow users, explicit cleanup,
intervening instructions and additional uses retain their owned snapshots.
Async/coroutine functions retain the established representation. Function/SDK
ABI, public value parameters, WIR identity and intrusive reference counting are
unchanged. There is no special-case match for std::builders names or this image.
The internal plan field is renamed to `borrowedStringCopies` to reflect its
broader scope; this is not a public language/API change.

## Focused qualification

Only two newly introduced tests run for this slice:

- `wio_wir_cpp_string_fast_plan`: integer append prefix/NUL/extreme/capacity and
  custom-locale checks; multi-block observer eligibility, binary operations,
  whitelist/rejection and whole-CFG use counting. Explicit tests reject unrelated
  adjacent sinks, intervening writes, unknown mutators/calls, capture, explicit
  destruction, phi transfer, owned loads and async frames.
- `wio_wir_cpp_string_fast_paths`: actual native O0/O2 plus bytecode/native-bridge
  semantic runs. Long string/text/numeric/NUL interpolation, explicit owned
  snapshot before a later argument mutation, ordering/search, self-append,
  self-insert and independent split results are checked. Native char/NUL and
  container fallback also have native-only coverage; this does not claim a new
  VM char/container formatting surface.

Local final Windows Debug: **2/2 passed in 11.30 seconds**. Both native O0 and O2
report **2,000 new/new[] calls for 1,000 maximal-u64 render-and-native-check
operations** (output storage and existing native value conversion), and only
**two calls total for the 1,000-iteration long-string search/comparison loop**
(the initial value arguments). Tests enforce finite budgets, not only output
equality. This is not zero allocations for all string code or the native bridge.
VM does not use that allocation assertion: a DLL's allocation counter cannot
measure the VM host heap. The actual VM semantic path still runs and passes.

An early test version assumed an interpolated borrowed operand was implicitly
snapshotted before a later mutating call. Current lowering retains borrowed
operands until formatting, so the test now explicitly owns the snapshot before
that call. This optimization does not introduce a new interpolation evaluation/
borrow contract. Another initial allocation threshold omitted the existing
native value-conversion allocation; it was corrected to measure the full path.
Earlier failed attempts are not counted as successful qualification.

The old corpus is not rerun. Diagnostic reference validation remains 693 IDs,
three dynamic-only entries and 49 committed std Result construction sites.
Clean CI builds the real CLI at `d6cdbd3` and passes both new tests on all three
platforms. Local allocation counts above are Windows-specific observations, not
fixed libc++ allocation counts on every platform:

| Platform/configuration | New tests | CTest duration | Evidence |
| --- | --- | ---: | --- |
| Windows / Debug | 2/2 passed | 19.18 s | [job](https://github.com/tynes0/wio/actions/runs/38717840013/job/116364766800) |
| Linux / Release | 2/2 passed | 12.59 s | [job](https://github.com/tynes0/wio/actions/runs/38717840013/job/116364766674) |
| macOS / Release | 2/2 passed | 11.29 s | [job](https://github.com/tynes0/wio/actions/runs/38717840013/job/116364766487) |

The [complete run](https://github.com/tynes0/wio/actions/runs/38717840013)
is successful: **6/6 new test executions**. This is clean correctness evidence,
not remote performance qualification or a newly executed old full corpus.

## Paired long-string interpolation evidence

The new versioned fixture interpolates a 32-byte string and appends it 30,000
times. Before/after generated sources have the same Wio input, rather than
comparing a literal-only input with an interpolating one. The pre-change source
was saved at `61d43ef`; all variants are recompiled against the same current
GCC/runtime/headers. Every run produces 960,000 bytes with FNV-1a
`2058931307218008101`. Internal timing includes builder/snapshot work but excludes
process startup, compilation, checksum/reporting. The ordinary new/new[] counter
is inside the region; it is not allocation bytes, malloc/aligned traffic or RSS.

15 interleaved measured rounds follow one excluded warm-up for each variant:

| Variant | Median | Range | new/new[] calls |
| --- | ---: | ---: | ---: |
| Previous generated interpolation, `61d43ef` | 4.016 ms | 3.902–4.099 ms | 60,020 |
| Current, `d6cdbd3` | 3.241 ms | 3.160–3.450 ms | 30,020 |
| C++ direct append reference | 0.576 ms | 0.531–0.676 ms | 17 |

Current is **1.239x faster**, **19.30% less measured time**, and removes exactly
one allocation per interpolation. It is still **5.63x** the direct append C++
reference, which constructs the same final bytes without the intermediate Wio
value/call surface; it is a lower-bound reference, not an identical algorithm.
Remaining owning output/value transport costs are not declared gone.

Windows build 26100, Intel Core i7-13620H, MinGW GCC 15.2.0, C++20 `-O2
-ffp-contract=off -static`, no fast-math. CPU/power affinity is not pinned.
Local generation retains unrelated user std worktree edits; clean CI correctness
is separate. Reproduce using
[`the fixture instructions`](../tests/performance/string_interpolation/README.md).
The runner records the selected fixture hash instead of pretending to use the
default literal-only fixture. Source/compiler/runtime/generated/binary hashes
and all 48 executions are in
[the raw evidence](evidence/string-direct-append-2026-10-15.json).

## Per-update Mandelbrot

Fresh native and Release non-JIT VM measurements for the unchanged 800 x 800,
100-iteration case follow below. This run uses five native measured rounds and
three VM observations, retaining the same numeric fingerprint and full PPM hash.
Previous full generated source is `9554b63` (the immediate parent `61d43ef` changes
docs only), so this paired full-output comparison isolates the current slice.

The first five-sample full-native comparison measured current **135.035 ms**
(130.798–138.465) against previous **132.854 ms** (131.685–134.348): an adverse
1.64% median delta, with overlapping ranges. Both sets and the VM measurements
are retained in [the first progress evidence](evidence/mandelbrot-direct-append-2026-10-15.json),
not discarded in favor of a nicer number.

A separate native-only recheck then alternates previous/current execution order
every round, reusing those exact binaries with no concurrent VM/build work.
Fifteen measured pairs follow an excluded warm-up pair. Every image is checked;
all times, pair ordering, binary hashes and the parent evidence's original file
hash are in [the balanced recheck](evidence/mandelbrot-direct-balanced-2026-10-15.json).
This is a subsequent manual recheck of the saved executables, not a fresh rebuild
or an additional independent VM sample set.

| Fresh measurement | Median | Range / count |
| --- | ---: | --- |
| Native compute kernel | 23.850 ms | 22.995–23.928 ms; 5 |
| Release VM compute kernel | 19,077.921 ms | 19,015.557–19,196.307 ms; 3 |
| Native full PPM, previous, balanced recheck | 132.043 ms | 130.346–135.133 ms; 15 |
| Native full PPM, current, balanced recheck | 131.769 ms | 129.126–133.687 ms; 15 |
| Release VM full PPM | 20,570.923 ms | 20,517.444–20,586.706 ms; 3 |

The balanced native median differs by only **0.21%** in the favorable direction;
the ranges overlap substantially. Together with the first adverse delta, this
does **not establish a meaningful Mandelbrot speedup or a certain regression**.
Keep this slice for its demonstrated general long-string allocation/time gain,
not a manufactured image-render gain. The prior report's 128.437 ms belongs to
another session; the same previous executable source measures about 132 ms here.
Do not treat that cross-session shift as a proven compiler regression either.

No numeric compute transformation or VM runtime optimization is implemented in
this slice. Compared with the prior 23.300 ms native and single 19.002 s VM
kernel observation, these fresh results are progress tracking, not an attributed
compute gain/regression. VM's substantial baseline cost remains open for pass 2.

Compute-only timer excludes startup, reporting and bridge loading and verifies
`11613930/3721766287256/94440`. Complete PPM wall time includes startup/loading,
numeric work, formatting, console reporting and file write/close, but not hash
validation or compilation. Normal OS caches are used, not durable disk sync.
All completed files are **5,156,528 bytes**, SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.
All 24 initial executions and 32 native recheck executions pass their checksums.

The VM is MSVC Release, non-JIT, using the existing diagnostic host's explicit
finite 10-billion instruction budget. Production limits are unchanged; VM
samples are separate-process observations without a hidden explicit warm-up.
The current runner also hashes the storage-plan source/header. Native compilation
and the selected source snapshots remain as described in the preceding
[measurement report](WIO_INTEGER_INTERPOLATION_OPTIMIZATION.md).

## Required fixed-reference comparison

| Implementation | Compute-only kernel | Complete PPM external wall |
| --- | ---: | ---: |
| C++ fixed reference | 28.405 ms | 76.02 ms |
| Wio native, fresh | 23.850 ms | 131.769 ms |
| Wio Release VM, fresh | 19,077.921 ms | 20,570.923 ms |
| C# fixed reference | 29.586 ms | 149.13 ms |
| Python fixed reference | 1,348.041 ms | 1,613.71 ms |

C++/C#/Python are the unchanged **2026-10-04** references, not fresh paired rows.
Same dimensions, iteration limit, precision policy and output/checksums;
GCC 15.2.0, .NET 9 Release, plain CPython 3.12.14. Reference storage/formatting
algorithms differ as documented in [the baseline audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md).
Their [preserved reference evidence](evidence/mandelbrot-reference-baselines-2026-10-04.json)
and sample dates remain visible. The two table columns are different metrics;
do not subtract them to fabricate a Wio formatting-only time or infer an exact
current cross-language performance budget from historical reference rows.

The three original optimization passes remain unchanged. Remaining pass-1 work
includes owning argument/effect/alias analysis, direct numeric append-to-builder,
ordinary +=, broader aggregate/container/async costs and coordinated std modules.
VM execution optimization and compiler/build throughput remain passes 2 and 3.
No release, universal speed target or final native performance closure is claimed.
