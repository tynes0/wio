# Floating fast-path frame and bounded stream-buffer views

N03 slice, 2026-10-18. This changes runtime helper implementation, not Wio
syntax, ABI, typed/lowered WIR or bytecode instructions. N03 is not closed.

## Changes and bounds

`AppendFloating` now outlines stream construction/EH work into
`detail::AppendFloatingStream<T>`. The usual finite/classic/nearest path retains
the exact precision-6 general conversion, with a 32-byte scratch buffer instead
of 128 bytes. Conversion failure still falls back to the host stream. Locale,
rounding, nonfinite spelling and signed-zero policy are unchanged. The known
libc++ spelling counterexample remains on the stream path; it is not revived as
a to_chars optimization. That helper is noinline but deliberately not marked
cold on libc++, where it is the normal path. MSVC uses noinline; GCC/libstdc++
uses noinline/cold for the usual rare fallback.

Integer and floating fallback helpers share `AppendStreamBuffer`. If the target
already has enough spare capacity, it appends a counted stream-buffer view
instead of constructing the extra owning `str()` result. The stream remains
alive and unmodified throughout the append. Long and embedded-NUL facet output
is preserved, including partial output when a facet throws and the stream
records badbit. The size check uses subtraction to avoid addition overflow.

If append can grow the target, the existing owning snapshot is retained. Native
facets can expose their stream to an allocation/new-handler hook; carrying a
live buffer view across that allocation boundary could invalidate it. This is
intentionally **not** unconditional zero-copy, stream-buffer stealing, a new
public string view or an altered target-growth/exception ordering policy.
No allocation event order is promised where an allocation is removed, but a
growing target retains the old snapshot boundary.

The new header is registered with the runtime's IDE/header target. The backend
runtime archive was checked: it does not instantiate these helpers and remains
unchanged. The Debug compiler's embedded VM was rebuilt for correctness; the
Release VM measurement host and Wio benchmark controller are not rebuilt.

## Local gates and retained test correction

The new stream unit compares deterministic random float/double bit patterns,
numeric boundaries, supported rounding modes, nonfinite/signed-zero values,
custom long/NUL facets and partial-failure output with the actual host stream.
Both reserved and growing targets are checked. The expanded Wio gate covers
native O0/O2 and VM scalar/container/builder snapshots, large/small exponents,
Unicode Text, and a native helper exercising reserved custom-facet fallback.
That helper mutates/restores its own C++ runtime locale; no assertion assumes
that an MSVC VM host and GCC native bridge share one locale global. The existing
60,000-value stream oracle also runs through that narrow gate, not a whole old
functional corpus.

An initial test incorrectly required **exactly one** fewer allocation per
formatted value. Windows Debug removes extra string debug bookkeeping too:
896 -> 640 allocations for 128 calls, not 896 -> 768. The failed XML and raw
counts are retained. The portable gate requires at least one eliminated
allocation per call, zero final live storage and identical growing-target
allocation/requested-byte behavior. Production code was not changed to make
that platform-specific test count pass. A separate guard was subsequently
added to preserve the growing-target snapshot boundary; pre-guard and guarded
test reports remain distinct.

## Direct allocation evidence

These are standalone diagnostic global-new/delete windows with reserved targets
and 128 long custom-facet conversions, **not production timing, RSS or all
malloc traffic**. MinGW probes link libstdc++/libgcc statically so replacement
new/delete and stream internals share one runtime image.

| GCC O2, each of integer and float | Before | After |
| --- | ---: | ---: |
| Allocations = frees | 256 | 128 |
| Requested = freed bytes | 99200 | 65664 |
| Peak live bytes | 775 | 513 |
| Final live bytes | 0 | 0 |

The after executable also compares the original copy implementation and current
helper in the same image. For a fresh/growing float target both retain 384
allocations, 132736 requested bytes and 1037 peak live bytes; the guard does not
claim a gain there. Windows Debug's reserved counts are 896 -> 640, requested
163840 -> 126976, peak 800 -> 768; its growing control/current counts are both
1152, requested 200704, peak 1088. Do not compare those absolute totals as if
different standard-library debug modes were one allocator.

## Frozen runtime measurement

Final relevant gates pass **2/2 in 85.41 seconds**, including native O0/O2,
Debug embedded VM and the standalone stream diagnostic. The final GCC O2
diagnostic also passes with strict warnings. Raw command/output records and
failed/earlier gate reports are linked below. The preceding scalar-fragment
commit's Windows Debug, Ubuntu Release and macOS Release CI all passed; this
change requires its own platform qualification.

Fresh before/after builds use the five identical frozen float-formatting
workload files and 10001 operations, GCC 15.2 C++20 O2 with FP contraction off,
and .NET Release. Wio-written `wio-bench` controls samples, A/A and a separate
final native/VM-only phase. Python prepares builds and independent checksum
oracles, not timing samples. Benchmark builds and collections are serial; no
trimming, retry-to-pass, priority/power/security changes or unrelated process
kills are used. Existing dirty std/playground files remain untouched. Declared
artifact hashes are evidence, not a clean full-transitive-repository claim.

The Release VM host is unchanged, SHA256
`d5897fa48bbf346c5b3c2f5f979dcabf66e589534aed6cab5d92cb7b9b035879`,
as is the controller,
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`.
Runtime archive SHA256 remains
`59ff4005117cb2c7cff701629b98d7a599d4550b18ff40b3aa402923f8863941`.
No Release VM speedup is attributed to the changed headers in generated native
code. Debug VM correctness uses rebuilt headers; those are distinct artifacts.

Comparison medians, **before -> after**, milliseconds:

| Workload | Wio native | Release VM | C++ streams | C# G6 |
| --- | ---: | ---: | ---: | ---: |
| Scalar f64 | 0.7818 -> 0.7533 | 49.6284 -> 44.4930 | 5.4908 -> 5.4946 | 1.4600 -> 1.4673 |
| Mixed builder | 4.2988 -> 4.2397 | 45.5657 -> 50.4373 | 7.8589 -> 7.8478 | 3.2844 -> 3.5080 |
| Float vector | 6.3691 -> 6.1426 | 52.8318 -> 48.7853 | 42.2711 -> 41.3251 | 11.0642 -> 10.0096 |

Separate final native/VM-only phase, milliseconds:

| Workload | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Scalar f64 | 0.7605 -> 0.7529 | 48.4642 -> 47.3800 |
| Mixed builder | 4.2792 -> 4.4872 | 44.9676 -> 45.4792 |
| Float vector | 6.3830 -> 6.0786 | 52.6579 -> 47.7935 |

Both complete sessions retain **126/126 correct outputs**, 252/252 together,
applied affinity, unchanged endpoint declared artifacts and unchanged endpoint
power profiles. Both are globally **timingQualified=false**. Candidate native
comparison relative ranges are 0.300/0.044/0.071; final mixed-builder range and
drift are both 0.179. A/A comparison differences are 0.018/0.003/0.002. The
final mixed-builder median **rises**, even though all three comparison medians
fall. Preserve that result; no accepted percentage speedup or whole-program
regression-free claim is inferred. Runner exit 1 rejects timing qualification,
not workload correctness. No samples were discarded or replaced.

C++ is the frozen standard-stream implementation, one stream per row/array,
not an optimized to_chars lower bound. Wio's lower times here do not establish
that Wio beats optimized C++ or closes the N14 1.05-1.10x overhead budget.
The previous chat's hook benchmark is a different workload and is not used as
a denominator. For separate historical context, the first N03 formatting
report observed native comparison 6.6028/11.4649/54.0146 ->
1.5881/4.5098/7.0450 ms. Later storage/transport work intervenes before this
fresh 0.7818/4.2988/6.3691 baseline; those dated observations are not a new
paired speedup factor.

## Production memory/code observations

The classic/nearest production workload does not execute custom-facet stream
fallback. Its diagnostic heap traffic is unchanged, not halved:

| Case | Allocations = frees, before/after | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Scalar f64 | 0 / 0 | 0 | 0 | 0 / 0 |
| Mixed builder | 18689 / 18689 | 2221507 | 858308 | 0 / 0 |
| Float vector | 20004 / 20004 | 920204 | 204 | 0 / 0 |

The separate long-facet diagnostic above demonstrates the conditional copy
removal; it is not substituted for production heap or runtime timing evidence.
Native process peak RSS medians slightly rise 9252/10104/9264 ->
9264/10112/9268 KiB. No whole-process RSS reduction is claimed. Candidate
VM/C++/C# peak medians are respectively 23560/5324/26088, 23712/6072/35528,
and 21644/5352/36564 KiB across the three cases. Managed current-thread bytes,
OS RSS, requested allocation bytes and stack reservations remain separate scopes.

The actual GCC O2 `AppendFloating<double>` prologue reservation falls from
**504 -> 112 bytes**, with saved general registers falling 8 -> 3. Work still
calls that helper at its scalar sites; its own reservation remains **856 bytes**.
The outlined fallback has a separate frame and the nonfinite branch tail-jumps
after restoring the wrapper's frame in this binary. This is not a whole-program
peak-stack measurement or a claim that every cold frame shrinks. See
[prologues, call sites and artifact hashes](evidence/n03-float-stream-prologues-2026-10-18.json).

| Artifact bytes | Before | After |
| --- | ---: | ---: |
| Generated C++ | 3446440 | 3446439 |
| Production native executable | 6623765 | 6622171 |
| Allocation-diagnostic executable | 6706063 | 6704365 |

Generated C++ is identical after normalizing only its output-folder name; the
one-byte difference is provenance, not fewer generated statements. Production
size falls 1594 bytes and diagnostic size 1698 bytes. This pass changes helper
implementation/code layout, not the generated program's WIR/statement shape.
Source/binary sizes alone are not runtime or heap performance claims.

## Retained evidence and remaining work

- Before: [all samples](evidence/n03-float-stream-before-2026-10-18.json),
  [build](evidence/n03-float-stream-build-before-2026-10-18.json),
  [production heap](evidence/n03-float-stream-memory-before-2026-10-18.json).
- After: [all samples](evidence/n03-float-stream-after-2026-10-18.json),
  [build](evidence/n03-float-stream-build-after-2026-10-18.json),
  [production heap](evidence/n03-float-stream-memory-after-2026-10-18.json).
- Allocation attribution: [unchanged baseline probe](evidence/n03-float-stream-copy-before-2026-10-18.jsonl),
  [final GCC reserved/growing probe](evidence/n03-float-stream-copy-after-2026-10-18.jsonl),
  [commands](evidence/n03-float-stream-gcc-probe-2026-10-18.json).
- Gates: [final CTest and MSVC diagnostic output](evidence/n03-float-stream-tests-2026-10-18.xml),
  [final Wio processes](evidence/n03-float-stream-validation-2026-10-18.json),
  [initial failed count assumption](evidence/n03-float-stream-initial-tests-2026-10-18.xml),
  [initial Wio processes](evidence/n03-float-stream-initial-validation-2026-10-18.json),
  [pre-capacity-guard pass](evidence/n03-float-stream-pre-guard-tests-2026-10-18.xml).

N03 remains open for richer/legacy formatting and the remaining KI-008 hook
surfaces. Live locale/fenv guards remain part of the contract, not a cached
classic-locale assumption. N04 has not started. KI-007/KI-009, the other fixed
native passes and N01/N14 performance qualification remain explicit; this slice
does not close them or the general backend performance budget.
