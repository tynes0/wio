# N03: float and builtin-container formatting

2026-10-17; first production slice of N03, **not completion of N03** or the VM
optimization pass. N02 is complete within its recorded bounds; its three-platform
CI run 38965482262 passed. N01 residual coverage and N14 acceptance stay open.

## Actual optimizations

`floating_format.h` implements the existing native display contract: defaultfloat,
six significant digits, current host locale. Finite f32/f64 values in classic
locale and FE_TONEAREST use `to_chars(general, 6)` into a fixed stack buffer,
appending directly without creating a stream or an owning temporary string.
Non-classic locale, non-default rounding, nonfinite values, unavailable floating
to_chars overloads or conversion errors use the existing stream behavior.
This is **not** shortest-roundtrip formatting, fixed six decimal places, a new
precision option or JSON serialization policy. No rounding mode/locale is changed.
Existing FP exception flags are not cleared; exact incidental stream exception-
flag side effects are not promised as a cross-standard-library contract.

`value_format.h` recognizes builtin scalar/string/text/enum leaves and recursive
vectors/maps/unordered maps of those leaves. Generated interpolation appends
their brackets, separators, keys and values directly to its local result buffer.
It avoids the old whole-container stringify -> final-buffer copy, intermediate
closing-bracket result and per-element floating stream. NUL, byte-char vs numeric
i8/u8, booleans and UTF-8 bytes retain their previous spellings.

Unknown native/user hooks retain the old generic path. Native containers with
non-classic locales also retain that path so stateful numeric-facet evaluation
is not silently rearranged. This internal trait is not a new Wio user type-trait
extension or permission to bypass custom ToString. Builder construction, reserve,
clear, Append/AppendLine and ToString snapshot APIs are unchanged in this slice.
Numeric direct-builder overloads remain future N03 work; we did not introduce
lossy integer-to-float overload selection or a generic native conversion escape.

## Formatting correctness fixes found by measurement

The old VM rejected builtin f64 array interpolation with **WVM1062**. It also
used shortest float output instead of native's six-digit display contract and
did not distinguish char from integer payloads. The new bounded `ValueFormatter`
uses existing interpolation signature metadata, shared scalar helpers and
recursive builtin array/dictionary traversal. No extra register-type table,
bytecode schema, native ABI or arbitrary runtime callbacks are added. Its depth
budget is 128; unsupported user/opaque/component leaves still diagnose WVM1062.
These are scoped conformance changes, not the broad VM optimization pass.

Negative zero in a native lowered constant could be printed as C++ integer
`-0`, losing the double sign. It is now emitted as `-0.0`. Runtime signed-zero
formatting and literal signed-zero transport have separate coverage.

**Remaining gap KI-008:** VM user-defined object ToString is not yet materialized
as a shared callable operation/continuation. Native stateful object/array hooks
have positive compatibility coverage; the VM fixture deliberately checks its
current WVM1062 rejection. That negative test is not feature parity. The shared
Wio tests exercise only the now-qualified builtin container leaves. Nonfinite
spellings retain the host stream, including platform-specific NaN variants;
universal cross-CRT signaling/payload-NaN spelling is not claimed.

## Measurement protocol

The baseline is `be038d5` (N02 already applied). Frozen related Wio, C++ and C#
programs were prepared before production changes; after uses the exact same
workload-source copies. The baseline directory name ends in 2026-10-16 because
preparation began before local midnight; collection/reporting is dated 2026-10-17.
The original VM host was retained as `vm-host-before.exe` before its Release
rebuild. Both sessions use the same Wio-owned controller image, not a new collector.

Three cases at 10,001 repetitions: independent f64 strings, mixed float/integer/
bool rows through StringBuilder with capacity 64 plus final snapshot, and an
eight-element f64 array. Independent Python `.6g`/full-byte FNV oracles validate
the exact bounded data. C++ is GCC 15.2 C++20 O2 `-ffp-contract=off`, C# is net9.0
Release, tiering disabled, invariant G6 with lowercase exponent. C++ uses one
standard stream per row/whole array, not one stream per array element. C#'s
UTF-16 strings, per-number strings and UTF-8 hashing array remain real costs;
these are equivalent outputs/workloads, not identical allocator layouts.

The Wio controller runs builds/version capture outside sampling, rotating/
reversing five-sample comparison rounds with native A/A controls, one excluded
external warmup, and a **separate** three-sample final native/VM-only phase with
fresh warmups/controls. Each program also performs two in-process warmups and
uses runtime input/result consumption. Processes are serial, child CPU 2 affinity
requested before launch; no power/priority/security settings change. Kernel
time includes formatting/hash/local cleanup, not startup/parsing/printing.
Whole-root-process CPU/peak RSS are recorded separately from kernel time.

The baseline's 10 float-vector VM warmup/sample/final processes fail WVM1062 and
remain in raw evidence. They are **not zero-time samples**, nor were they replaced
with a different workload. Native/C++/C# rows and other VM cases are individually
correct, but the baseline is not an overall correct/qualified comparison session.
No before/after VM-vector speedup can be computed from unsupported execution.

Separate diagnostic executables enable the benchmark allocation windows. Those
counts are global-new/delete requested/live/peak bytes, not RSS, copied bytes,
all malloc/custom traffic, VM heap counters or production timings. All completed
windows must end with zero tracked live bytes/blocks. Source/artifact hashes
cover declared inputs, not the entire transitive repository or every dirty std
file. Existing user std/playground changes are not part of this patch.

## Observations, not qualified speedup claims

Comparison medians in milliseconds, 10,001 repetitions, **before -> after**:

| Workload | Wio native | Release VM | C++ streams | C# G6 |
| --- | ---: | ---: | ---: | ---: |
| Scalar f64 | 6.6028 -> 1.5881 | 48.3113 -> 45.0151 | 5.6115 -> 5.3155 | 1.4348 -> 1.4849 |
| Mixed builder | 11.4649 -> 4.5098 | 50.1137 -> 49.5397 | 7.8094 -> 7.9923 | 3.2221 -> 3.6563 |
| Float vector | 54.0146 -> 7.0450 | unsupported -> 51.6240 | 45.8275 -> 41.1372 | 12.2726 -> 11.1143 |

The separate final Wio-only phase has its own warmups and controls:

| Workload | Native before -> after, ms | VM before -> after, ms |
| --- | ---: | ---: |
| Scalar f64 | 8.0627 -> 1.4782 | 54.6519 -> 46.0657 |
| Mixed builder | 10.7649 -> 4.6059 | 47.5871 -> 47.1685 |
| Float vector | 51.6014 -> 6.9815 | unsupported -> 57.4805 |

After has **126/126 correct process outputs**; before has 116/126, with the ten
unsupported VM-vector processes preserved. Both sessions keep affinity applied,
unchanged endpoint artifacts and unchanged power profile, but overall timing
qualification is **false**. For example, after mixed-builder C++ relative range
is 1.19 and final VM-vector drift is 0.47; not all A/A/stability gates pass.
Even individually stable lanes do not qualify the whole session. No definitive
before/after speedup factor, cross-language ranking or N14 budget acceptance is
inferred. C++ here is the equivalent standard-stream implementation, **not an
optimized `to_chars` lower-bound reference**. A stronger reference is still needed
before accepting a general native-overhead budget.

Native diagnostic new/delete accounting (successful allocations equal frees):

| Workload | Allocations before -> after | Requested/freed bytes before -> after | Peak live bytes before -> after | Final live blocks/bytes |
| --- | ---: | ---: | ---: | ---: |
| Scalar f64 | 10001 -> 10001 | 560056 -> 560056 | 112 -> 112 | 0 / 0 |
| Mixed builder | 18690 -> 18690 | 2221563 -> 2221563 | 858364 -> 858364 | 0 / 0 |
| Float vector | 50007 -> 30005 | 2460358 -> 1480260 | 278 -> 285 | 0 / 0 |

The vector path removes allocation/copy intermediates, but growth capacity
temporarily overlaps differently: **peak requested live memory rises by 7 bytes**.
Do not call every memory metric improved. Short scalar float output already fits
small-string storage, so removing stream work need not change heap counts.
Builder row/snapshot allocations remain and motivate the next direct-append slice.

After comparison median whole-process peak RSS in KiB:

| Workload | Wio native | VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Scalar f64 | 9280 | 23672 | 5324 | 26052 |
| Mixed builder | 10120 | 23576 | 5864 | 35488 |
| Float vector | 9288 | 21640 | 5368 | 36532 |

Native RSS before was 9280/10300/9296 KiB respectively. RSS includes host/runtime,
startup and warmups; small shifts are not isolated heap savings. C# additionally
reports current-thread managed allocation bytes of 655104/3855856/8400928 for the
three kernels. Those GC counters are not directly equivalent to C++ requested
bytes, VM heap accounting or OS RSS.

Raw evidence: [before](evidence/n03-format-before-2026-10-17.json),
[after](evidence/n03-format-after-2026-10-17.json),
[build before](evidence/n03-format-build-before-2026-10-17.json),
[build after](evidence/n03-format-build-after-2026-10-17.json),
[allocation before](evidence/n03-format-memory-before-2026-10-17.json),
[allocation after](evidence/n03-format-memory-after-2026-10-17.json),
[latest Wio validation](evidence/n03-format-validation-2026-10-17.json),
[host/VM unit validation](evidence/n03-format-unit-2026-10-17.xml),
[initial user-hook failure](evidence/n03-format-user-hook-failure-2026-10-17.json).
The original collector's Markdown renders an empty statistics median as zero;
failed VM-vector rows have no valid timing observations, not zero execution cost.
The tables here mark those rows unsupported, and the JSON retains each failure.

## New-only validation and boundaries

- Host C++: 30,000 float and 30,000 double raw bit patterns against the actual
  standard stream, plus signed zero, subnormals/extremes, nonfinite values,
  supported rounding modes and custom grouping/num_put locales. The Wio gate
  additionally compiles this test with the generated-code C++ compiler.
- VM helper: typed float/char/NUL/Unicode, array/dictionary syntax and precision,
  f32 normalization, invalid type/depth rejection and unsupported leaves.
- Wio native O0/O2 + VM: scalar/nested containers, numeric bytes/bools/chars,
  NUL/Unicode strings, dictionary values, generic ToString, signed-zero literals,
  reserve/clear and owning builder snapshots. Native user object/array ToString
  callbacks stay stateful; VM user formatting is an explicit negative boundary.

An initial VM helper test used a raw U-string literal under MSVC's source code
page; it was corrected to universal character escapes plus explicit UTF-8 bytes.
The renderer was not changed to accommodate an incorrectly encoded fixture.
Failed baseline and development checks remain under `build/n03-*`; no old
functional corpus was run locally. CI has the three new tests on Windows Debug,
Ubuntu Release and macOS Release; platform success must be checked after push.
All three new tests passed locally; the latest Wio gate took 127.77 seconds,
including the generated-code compiler's independent float oracle and explicit
VM-user-hook negative case. The preceding combined three-test run took 126.14
seconds; the expanded Wio gate was then rerun, not silently inferred from it.
The final separate host/VM unit recheck passed 2/2 in 0.69 seconds.

## Remaining N03 work

Direct numeric append into existing builder storage was subsequently implemented
by [the second N03 slice](WIO_NUMERIC_BUILDER_OPTIMIZATION.md). Richer
formatting/precision surfaces, the legacy native
`runtime::Format` stream surface and user-hook callable lowering/VM invocation
remain open. Keep ToString snapshots, call order, locale/rounding policy and
failure behavior explicit. N04 byte/buffer/I/O and the fixed later passes have
not started in this slice. No general 1.05–1.10x budget acceptance is claimed.

See [fixtures and reproduction](../tests/performance/formatting/README.md).
