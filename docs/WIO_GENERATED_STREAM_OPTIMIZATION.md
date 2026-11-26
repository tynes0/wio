# N03 actual C++ backend stream formatting

2026-10-18, based on `c28cddf`. This is generated-code optimization, not the
legacy compatibility `runtime::Format` template changed in the preceding slice.
N03 remains open; N04 has not started.

## Production changes and bounds

1. The generic native `operator<<` stringify fallback takes the finished
   stream's owning buffer with `std::move(stream).str()`. It does not expose a
   view into a dead stream or replace streams with a different spelling policy.
2. A bare, one-operand string interpolation uses `format_single`. Basic types
   retain the existing direct append/container-locale policy. Nonbasic values
   reuse the formatter's owning result instead of assembling a second empty
   buffer. If capacity minus size exceeds size (an overflow-safe 2x comparison),
   produce a compact owning copy instead. This bounds oversized native reserves
   in possibly escaping results; it intentionally gives up some allocation
   savings for retained memory. User/native formatting still runs once at the
   existing interpolation instruction, not before operand evaluation.
3. For a single operand with a literal suffix, the generated helper preserves
   prefix construction, renders a nonbasic value once, then preflights the
   **already rendered** string length plus current target and independent
   literal-tail lengths. One checked reserve avoids suffix-only growth. It
   never formats twice for a size estimate. Basic values keep their previous
   direct append then suffix path. Tail bytes have an explicit count, including
   NUL/UTF-8, and are not a possibly invalidated view into the target.

The new bare/tail selection excludes private rendered integer/bool fragments;
their view append representation is not a general owned string. It also leaves
Text-result interpolation on its existing path. Multi-operand ordering is not
rewritten. The checked preflight uses the existing overflow/length-error helper.
No typed/lowered WIR, bytecode, language signature, native layout or SDK ABI
changes are introduced. Codegen file formatting also received mechanical
clang-format alignment; no other compiler policy changed.

## Actual generated-helper evidence

The test includes the emitted C++ (renaming only its host main), not a hand-written
model of the candidate implementation. It compares those real helpers against
copies of the old stream snapshot + assembly paths in the same image. Long
input caches are warmed before allocation windows. Counts below are for 128
calls, with every result destroyed inside the window:

| Metric, GCC O2/static C++ runtime | Bare copy -> current | Affix copy -> current |
| --- | ---: | ---: |
| Allocations | 384 -> 256 | 512 -> 256 |
| Requested bytes | 121636 -> 93650 | 179016 -> 94674 |
| Peak live bytes | 733 -> 733 | 733 -> 741 |
| Returned capacity for the sample | 217 -> 217 | 442 -> 225 |

All diagnostic windows end with zero live blocks/bytes and no tracked failures.
The bare result compacts excessive spare capacity; the affixed result avoids
the old suffix doubling. The affix helper's isolated peak **rises 8 bytes** even
though allocation traffic and final capacity fall. Neither transfer nor reserve
is a universal memory win. These replacement-new/delete counters are not RSS,
all malloc traffic, copied-byte counts or ordinary timing.

## Correctness and current VM boundary

Final new/expanded gates pass **2/2 in 31.06 seconds**:

- Wio native O0/O2 covers native POD stream interpolation, counted NUL/Unicode
  affixes, exactly-once operators, independent owning snapshots, seek/high-water
  output, badbit partial output and native exception propagation. Builtin
  scalar/container/string/Text and public scalar Wio ToString paths also pass.
- A native stream reserves 1 MiB but returns only 217 bytes; the actual generated
  bare helper must return equal bytes without retaining that excessive reserve.
- The emitted-helper executable covers actual C++ output and the allocation
  controls above; no shipping compiler instrumentation is inserted.
- Expanded scalar-format-plan IR cases cover **bare and suffix** consumers of
  private integer/bool fragments and assert they keep direct view appends rather
  than entering either new owning helper.
- VM builtin/scalar Wio ToString cases pass. Native PODs relying only on a C++
  `operator<<` still reach **WVM1062**: no declared native formatter thunk exists
  for those values. That explicit negative case passes its rejection assertion,
  not native/VM feature parity. It is tracked under KI-008.

The initial 1/1 generated-stream gate passed in 31.31 seconds. The unbounded
intermediate 2/2 gate passed in 29.82 seconds; it removed more bare allocations
(384 -> 128) but returned capacity 512 for a 217-byte result. Capacity review,
not a favorable timing rerun, motivated the final compacting guard. All reports
and all intermediate samples/builds remain. No shrink-to-fit loop is added.
The preceding `c28cddf` commit's Windows Debug, Ubuntu Release and macOS Release
CI passed; this change needs its own platform qualification.

## Measurement method

The Wio workload interpolates the actual native POD in user code, not a native
wrapper returning a preformatted string. Both cases consume full UTF-8 FNV
checksums: 192-byte cached ASCII/NUL input with bare interpolation, and 8192-byte
cached input with a prefix-NUL and suffix. The count is 2001. Wio native, C++ and
C# perform two in-process warmups. Python prepares frozen builds and independent
oracles; the Wio controller collects rotating/reversing comparison rounds,
exact-command native A/A and a separate final native/VM phase. Heavy builds,
correctness gates and timing sessions are serial/separate.

C++ is a frozen one-stream owning-copy reference, **not** optimal C++ with rvalue
buffer extraction. C# uses UTF-16 builders and an allocating UTF-8 hash. Input
caches initialize during warmup in all supported implementations; these are
equivalent byte outputs, not identical runtime/encoding/allocation contracts.
The earlier legacy-helper and float workloads are different cases, not valid
historical paired denominators for this new native-POD fixture.

The four-implementation runner retains all native-POD VM failures in all three
sessions. Consequently overall `outputsCorrect=false`, and VM has **no valid
duration**, not zero cost. Empty-summary zeros must not be quoted as timings.
This is a visible missing VM formatting surface, not an optimization regression.
No native formatter thunk, VM callback/continuation or interpreter-core
optimization is claimed by this native codegen patch.

## Before, intermediate and selected observations

Comparison medians below are milliseconds for 2001 operations, five measured
samples per lane. The middle build is the **unbounded intermediate**, not the
selected result. Before/final workload sources and compiler flags are frozen;
the candidate is the generated helper, not a changed workload or reference.

| Case / implementation | Before | Unbounded intermediate | Selected capacity guard |
| --- | ---: | ---: | ---: |
| Bare 192 / Wio native | 1.3622 | 1.2170 | 0.9553 |
| Bare 192 / C++ | 1.2269 | 0.8980 | 1.0136 |
| Bare 192 / C# | 2.4916 | 2.5904 | 2.1958 |
| Bare 192 / Wio VM | unsupported | unsupported | unsupported |
| Affix 8192 / Wio native | 26.8820 | 23.2500 | 22.8379 |
| Affix 8192 / C++ | 23.7331 | 22.4417 | 23.7684 |
| Affix 8192 / C# | 34.3416 | 32.4749 | 34.0841 |
| Affix 8192 / Wio VM | unsupported | unsupported | unsupported |

The separate final native medians (three measured samples) are bare
**1.6782 -> 0.8882 -> 1.3615 ms**, affix
**30.5865 -> 23.3466 -> 23.1462 ms**. The selected bare final is worse than the
intermediate final, despite its better comparison median. Do not select the
lower phase as the answer. In the selected session bare native comparison has
70.4% range and 11.6% A/A difference; final has 37.5% range and 29.3% A/A
difference. Affix comparison has 8.5% range and 8.9% A/A difference, final 13.4%
range and 3.3% A/A difference. Unchanged C++/C# references also move. Every
session has `timingQualified=false`; no accepted speedup percentage, universal
native-language parity, VM improvement or N14 budget closure is established.

Each session completes all 84 launches: **64/64 supported outputs are correct**
(including warmups and native A/A), with **20/20 VM launches rejected WVM1062**.
Affinity, artifact and power-profile stability checks pass in each session.
No failed sample was silently dropped; overall output correctness remains
false because the VM surface is unsupported. The compiler is a Debug host;
timed generated native/C++ artifacts are GCC O2, C# is .NET 9 Release with
tiering disabled, and the VM runner is Release but has no valid workload time.

### Full Wio workload heap diagnostics

These are separate untimed replacement-new/delete windows for 2001 calls,
not counts from the timing process. Every allocation has a matching free and
all windows end with zero tracked live bytes/blocks and failures.

| Case / metric | Before | Unbounded intermediate | Selected capacity guard |
| --- | ---: | ---: | ---: |
| Bare / allocations | 8004 | 4002 | 6003 |
| Bare / requested bytes | 1986773 | 1106553 | 1546663 |
| Bare / peak live bytes | 774 | 553 | 774 |
| Affix / allocations | 20010 | 16008 | 16008 |
| Affix / requested bytes | 130468762 | 81110425 | 81110425 |
| Affix / peak live bytes | 24723 | 24659 | 24659 |

The selected bare policy deliberately restores one compacting allocation per
call versus unbounded transfer. It still saves one allocation versus the old
path, while avoiding arbitrary spare-capacity retention. The 128-call helper
sample's returned bare capacity is 217 -> 512 -> 217 for 217 content bytes.
This is a balanced allocation/retention choice, not the minimum-allocation
candidate. The exact copy constructor capacity is implementation-dependent;
the guard requests compaction, not an allocator-independent capacity guarantee.

Comparison native peak RSS medians in KiB are bare **9316 -> 9312 -> 9316**,
affix **9612 -> 9608 -> 9616**. Final native RSS is bare
**9316 -> 9324 -> 9320**, affix **9608 -> 9608 -> 9616**. These small changes
do not establish an RSS win. C++ selected comparison RSS is 5288/5576 KiB,
C# 34252/36632 KiB; these whole-process encoding/runtime footprints are not
the allocation-window metric.

### Size, provenance and retained evidence

| Artifact bytes | Before | Unbounded intermediate | Selected capacity guard |
| --- | ---: | ---: | ---: |
| Native timing executable | 6394397 | 6390744 | 6397400 |
| Native memory diagnostic | 9530321 | 9531152 | 9531152 |
| Generated C++ source | 3262306 | 3263078 | 3263276 |
| C++ reference executable | 73866 | 73866 | 73866 |

The selected native binary **grows 3003 bytes**, source grows 970 bytes and
diagnostic grows 831 bytes versus baseline; this is not a code-size win. All
five workload sources match SHA256 across the three stages. The selected
compiler SHA256 is `35320925d6e5ddb2a4d7e3d9d4b616f7737792ec7fb40d0a4dabce12a847fcd7`.
Runtime archive, controller and Release VM hashes remain unchanged. Generated
source is intentionally different; PE timestamps can also change binary hashes.

- Samples: [before](evidence/n03-generated-stream-before-2026-10-18.json),
  [unbounded](evidence/n03-generated-stream-unbounded-2026-10-18.json),
  [selected](evidence/n03-generated-stream-after-2026-10-18.json).
- Builds: [before](evidence/n03-generated-stream-build-before-2026-10-18.json),
  [unbounded](evidence/n03-generated-stream-build-unbounded-2026-10-18.json),
  [selected](evidence/n03-generated-stream-build-after-2026-10-18.json).
- Heap: [before](evidence/n03-generated-stream-memory-before-2026-10-18.json),
  [unbounded](evidence/n03-generated-stream-memory-unbounded-2026-10-18.json),
  [selected](evidence/n03-generated-stream-memory-after-2026-10-18.json).
- Final [CTest XML](evidence/n03-generated-stream-final-tests-2026-10-18.xml)
  and [processes/helper counters](evidence/n03-generated-stream-final-validation-2026-10-18.json).
  Initial and intermediate reports remain beside them under `tests`/`validation`
  and `unbounded-tests`/`unbounded-validation` names.
- [Artifact/source/session proof](evidence/n03-generated-stream-artifacts-2026-10-18.json)
  identifies all stages. Build directories named `after` hold the **intermediate**;
  selected build directories are named `bounded`. Published evidence names
  `after` refer to the selected result, with all original paths retained.

## Remaining scope

This slice completes the generated native stream copy/capacity work, not all
N03 formatting. Richer/indexed formatting and remaining KI-008 hook/VM parity
need separate attribution and correctness work. KI-007/KI-009 and N01/N14
performance acceptance remain open; N04 has not started. SDK, ABI, ownership
cleanup and interpreter-core changes are not hidden inside this patch.
