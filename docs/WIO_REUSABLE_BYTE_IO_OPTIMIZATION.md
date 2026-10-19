# N04: caller-owned byte read buffers

2026-10-19. This is an opt-in reusable-buffer slice of N04, not closure of N04,
N14, VM optimization or the compiler-throughput pass.

## Contract and implementation

`std::io::ReadBytesInto(file: ref File, buffer: ref u8[]) -> usize` fills the
buffer's existing length exactly. `ReadSomeBytesInto` performs a bounded read and
returns the valid prefix length without changing the destination length. Its
unread suffix stays intact. Reserve is not resize: a reserved empty array still
requests zero bytes. Empty destinations return zero without validating the file,
as with existing zero-count reads. Nonempty reads preserve the existing file
validation, OS error classification, partial byte counts and cursor behavior.
Exact-read failure does not promise transactional buffer contents or rollback.

The native runtime uses the existing raw read primitives with a mutable span of
caller elements. The vector overload passes its storage directly; neither size
nor capacity changes and there is no intermediate owning payload. The C++ span
overload also supports subranges, checked with neighboring canaries. Old owning
reads still return independent arrays and keep their existing behavior.

The public Wio surface uses ordinary mutable array borrows. No compiler intrinsic,
ABI version change, unchecked arbitrary pointer or buffer-pool global is added.
Native caller reuse is not automatically a zero-copy VM path: the VM's generic
array bridge still materializes/copies elements. VM zero-copy/bulk transport is
separate work and must retain lifetime and boundary checks.

## Measurement design

The expanded byte-I/O fixture has four cases: owned exact/some and reusable
exact/some. Every language performs the same 257 seeks plus 64 KiB reads plus
full-byte FNV checksum (16.0625 MiB). Reusable storage is prepared before warmups,
the measured kernel and allocation window in Wio, C++ and C#. Setup/open/close
stay outside the kernel; the cache is hot, not a cold disk throughput test.
C++ uses ifstream, C# FileStream with managed buffering disabled, and Wio OS file
handles. Error/syscall implementations are not identical across languages.

Compare storage strategies inside the same selected session. Expanded source
files and binaries differ from the two-case historical fixture, so preserve new
hashes and do not call them unchanged inputs. Historical original/previous
observations remain in the [direct-read](WIO_BYTE_IO_OPTIMIZATION.md) and
[runtime-profile](WIO_RUNTIME_PROFILE_OPTIMIZATION.md) reports, with their timing
qualification limits. No favorable retry/trimming or power/priority change.

The expanded local byte-I/O gate passes 1/1 in 117.64 seconds (29 processes):
native O0/O2, allocation/span controls and both O0/O2 VM bridges, including
reusable partial/closed errors. The C++ reuse loop performs zero allocations.
Cross-language performance and actual Wio heap evidence are recorded below.
The initial fixture mistakenly used an unsupported array `Resize` member; it was
corrected to explicit Reserve/Push initialization outside the measured kernel.
Do not hide that failed test or treat it as a runtime regression.

Source `a1fb58184aef02ecb85ac3d4971721e5bce3a142` passes scoped Windows Debug,
Ubuntu Release and macOS 15 Release jobs in
[CI 39177619566](https://github.com/tynes0/wio/actions/runs/39177619566), including
the expanded N04 gates and the workflow's scoped N03 regressions. This is not a
full historical test-corpus run or a multi-platform performance acceptance.

## Selected timing session

The single Wio-controlled session completes with **168/168 correct samples**.
Affinity, unchanged artifacts and unchanged power-profile checks pass. Overall
`timingQualified` is **false**, so controller exit 1 is a qualification result,
not a workload failure. No favorable retry, discarded sample, local overlapping
build/test or power/security/priority change is used to improve the result.

Five-sample comparison medians, milliseconds for 257 seek/read/hash iterations:

| Storage / read mode | Wio native | Wio VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Owned exact | 40.8699 | 5271.2642 | 30.7136 | 31.1988 |
| Owned some | 41.2362 | 5290.4742 | 28.1455 | 30.6950 |
| Reused exact | 28.1145 | 10136.3242 | 28.2641 | 29.0861 |
| Reused some | 29.0215 | 13160.1089 | 29.2004 | 28.3945 |

The raw native reuse medians are 31.2%/29.6% below owning and approximately
0.995x/0.994x the corresponding reused C++ medians. This is promising in this
bounded hot-cache case, **not accepted speedup, parity or a universal 5-10%
guarantee**. Native reused ranges are 41.1%/25.6%, C++ reused ranges 26.7%/25.1%;
native reused A/A median differences are 2.1%/1.0%, but that does not qualify the
noisy suite. The existing owning API still has an observed gap to owning C++
(about 1.33x/1.47x here); adding an opt-in storage strategy does not fix that gap
automatically. Owning and reuse also differ in retention/alias semantics.

Separate final three-sample Wio phase, milliseconds:

| Storage / read mode | Native | VM |
| --- | ---: | ---: |
| Owned exact | 46.5875 | 4906.2925 |
| Owned some | 58.4565 | 4344.4338 |
| Reused exact | 29.2826 | 10961.5185 |
| Reused some | 30.2818 | 11854.9567 |

Keep these final observations, including the worse owning times. Final native
reused ranges are 7.4%/14.7%, A/A differences 5.2%/9.6%, but final VM reused
ranges are 26.1%/32.4%. This does not repair overall timing qualification. C++/C#
comparison medians are not contemporaneous final-phase references.

The **VM reuse case is worse**, in both observed time and resident memory. The
generic mutable-array bridge encodes the destination before the native call and
plans element-wise copy-back afterward, in addition to array conversion. Source
inspection identifies work to isolate; no VM cycle/allocation profile proves
the exact contribution of each part. No VM source/binary is optimized in this
slice, and no VM speed or memory improvement is claimed.

### Historical context, not unchanged-input acceptance

| Selected session | Native exact / some, ms | VM exact / some, ms |
| --- | ---: | ---: |
| Original two-vector owning | 65.8445 / 67.2972 | 6619.8585 / 5996.0838 |
| Direct owning reads | 36.0935 / 40.6438 | 3713.8901 / 3623.6411 |
| Previous explicit runtime/bridge profile | 39.3734 / 39.0302 | 5789.2383 / 4310.3273 |
| Current expanded fixture, owning | 40.8699 / 41.2362 | 5271.2642 / 5290.4742 |
| Current expanded fixture, reused | 28.1145 / 29.0215 | 10136.3242 / 13160.1089 |

All of these sessions are timing-unqualified. The expanded fixture has new
source/binary identities, more cases, dispatch and caller initialization; reuse
also changes storage strategy. Do not infer a fixed-input compiler speedup from
this history or erase the original/previous raw sessions.

## Actual native Wio allocation window

The separate diagnostic executable runs the real generated Wio call path, not
only the C++ helper control. For 257 full reads in either exact/some mode:

| Metric | Owning reads | Caller reuse |
| --- | ---: | ---: |
| Allocations / frees inside window | 514 / 514 | 0 / 0 |
| Requested / freed bytes inside window | 16855088 / 16855088 | 0 / 0 |
| Peak newly tracked live bytes | 65632 | 0 |
| End newly tracked live bytes / blocks | 0 / 0 | 0 / 0 |
| Allocation failures | 0 | 0 |

This eliminates allocation traffic in the loop, **not all memory use**. Reuse
keeps its 65536-byte payload plus ordinary caller state alive from setup until
cleanup, outside this tracking window. The zero tracked peak is therefore not a
claim of zero physical live memory or a 65632-byte RSS saving. An owning snapshot
and caller-owned reusable storage also have different retention/alias semantics;
old owning APIs are not silently converted to reuse. Do not infer C#/VM managed
heap counts from this native new/delete diagnostic.

Median process peak RSS during the comparison, KiB (whole process including
startup/warmups, not the narrow allocation window):

| Storage / read mode | Wio native | Wio VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Owned exact | 9304 | 24844 | 5468 | 37008 |
| Owned some | 9300 | 24828 | 5468 | 37020 |
| Reused exact | 9312 | 43456 | 5420 | 24496 |
| Reused some | 9312 | 43460 | 5416 | 24496 |

Native RSS is essentially unchanged despite the allocation-traffic reduction.
VM reused RSS is substantially higher; this is an exposed follow-up, not a hidden
tradeoff or a balanced-memory win for the VM. C# differences are process-RSS
observations, not an isolated GC allocation/retention attribution.

## Size and build-cost accounting

This adds APIs and expands the fixture, so size grows relative to the preceding
two-case runtime-profile image:

| Artifact | Previous bytes | Expanded reusable fixture bytes |
| --- | ---: | ---: |
| Runtime archive | 1721974 | 1722580 |
| Native executable | 5744602 | 5758419 |
| Native bridge DLL | 2452902 | 2459505 |
| Allocation-diagnostic executable | 8930797 | 8949602 |
| Generated C++ | 3579899 | 3599292 |
| Bytecode | 2707472 | 2724504 |

The fixture now contains four modes plus dispatch and buffer initialization,
not a smaller generated program. Build-command elapsed times are emit 4.45s,
native O2 compile/link 150.12s, allocation diagnostic 139.38s, C++ reference
3.74s, .NET Release build 5.60s and native bridge plus bytecode 52.57s. These are
different source volumes/caches/tool pipelines, not a fair standalone compiler
throughput comparison. Generated build cost remains a separate open pass.

## Remaining N04 work

Native reuse does not close the broader buffer/streaming surface. A bounded
write range should consume a short-read prefix without allocating a Slice copy;
bulk ByteBuffer/pool operations and stream batching still need their own matched
workloads and contracts. Existing dirty user buffer/playground files were not
edited in this slice. The later VM/native-ABI optimization work must address
bulk scalar byte transport rather than weaken borrow/copy-back checks: mutable
generic array calls currently pack input elements and plan element-wise copy-back,
in addition to output conversion. This is a code-inspection lead, not an isolated
VM allocation or cycle profile.

## Evidence

- [Selected 168 samples](evidence/n04-reuse-results-2026-10-19.json),
  [native heap windows](evidence/n04-reuse-memory-2026-10-19.json),
  [build commands and elapsed times](evidence/n04-reuse-build-2026-10-19.json).
- [Source/tool/artifact identities and CI](evidence/n04-reuse-artifacts-2026-10-19.json).
- [Passing scoped test](evidence/n04-reuse-tests-2026-10-19.xml),
  [29 process records](evidence/n04-reuse-contract-2026-10-19.json),
  [initial fixture failure](evidence/n04-reuse-initial-contract-2026-10-19.json).

N04 remains in progress; N05 has not started. KI-010 computed VM globals,
VM/bulk-ABI optimization, N14 timing acceptance and compile-time optimization
remain separate open work.
