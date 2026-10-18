# N04: explicit runtime and native-bridge optimization profiles

2026-10-19. This follows the direct owned-byte-read slice, not a change to the
fixed N01-N14 ordering or closure of N04/N14. No Wio source semantics, WIR/VM
instruction format, public SDK ABI or CLI compilation default is changed.

## Product build policy

The backend runtime previously used no explicit optimization flag, including
when its host project was configured Release. Its archive output is shared
across host configurations, so deriving flags from whichever host configuration
built last would be misleading. It now has an explicit CMake cache profile:

| `WIO_BACKEND_RUNTIME_PROFILE` | Common flags |
| --- | --- |
| Release (default) | `-O2 -ffp-contract=off` |
| Debug | `-O0 -g -ffp-contract=off` |
| RelWithDebInfo | `-O2 -g -ffp-contract=off` |
| MinSizeRel | `-Os -ffp-contract=off` |

No profile adds NDEBUG, fast-math or disables exceptions/checks. C++ runtime
objects and the C99 utf8proc object share this policy, plus PIC on Unix.
`configure_file` records the profile/compiler/flags in `build-profile.txt`; all
runtime objects depend on it. A changed profile rebuilds objects, an unchanged
configuration does not rewrite the fingerprint. The profile file is installed
beside `runtime/lib/libwio_runtime.a`. Host compiler/VM configuration is separate;
Debug generated applications may link the optimized runtime, as with a normal
prebuilt standard library. Select Debug explicitly when debugging runtime code.

## Measurement correction and controls

The byte-I/O fixture now explicitly builds its VM native bridge at O2 with FP
contraction off. The shared preparation harness keeps driver-default bridge
flags for other historical fixtures unless selected explicitly. It records
bridge flags and the runtime build fingerprint, rejects reuse that would relabel
known flags/runtime inputs, and preserves unknown provenance in older manifests.
Build durations are retained separately from execution kernel durations.

The frozen previous runtime archive (SHA-256
`edc34510c8068cf32a2b30d3438e28459c2cd633268c2e78148d55e60b99b36b`)
is retained locally. Previous before/after samples remain untouched in the
[direct-read report](WIO_BYTE_IO_OPTIMIZATION.md). This slice uses its previous
selected after session as a **historical baseline**, not a newly timed or
retroactively qualified before session. Five workload files and 257 reads stay
the same. Native changes isolate the archive policy; VM changes combine archive
and bridge optimization, so their effects are not individually attributed.

Only Release native-boundary behavior is a performance candidate here. O0/O2
generated/bridge correctness remains checked; this does not turn every Debug
application into an O2 build or claim that every installed old release used the
same runtime flags. KI-010 computed VM globals remain open; both fixtures keep
the same local mode expression.

## Selected local evidence

- Two scoped gates pass 2/2 in 107.91 seconds (107.58 byte-I/O, 0.07 profile).
  The expanded I/O gate has 21 processes, including O0/O2 generated native code,
  both O0/O2 native bridges, exact/partial/EOF/closed behavior and heap contracts.
- Profile tests check all four flag lists and rejection of invalid profiles.
- A local runtime-only CMake install reproduces the selected archive hash and
  profile text. This is not a full clean SDK/distribution acceptance matrix.
- Attempting to reuse the preceding default-bridge image as an O2 image rejects
  before rewriting its manifest. No old image is silently relabeled Release.
- The backend archive falls **7344752 -> 1721974 bytes**. This is a disk/archive
  size result, not a runtime RSS or speed measurement. Compiler, Release VM and
  Wio controller binaries are kept unchanged for the performance experiment.

## Selected measurement supplement, 2026-10-19

The single selected Wio-controlled session completes with **84/84 correct
samples**. Affinity, unchanged artifacts and unchanged power-profile checks pass.
`timingQualified` is **false**; controller exit 1 is the timing-qualification
result, not a workload failure. No favorable retry, discarded sample or local
build/test overlaps this session. Five frozen workload source hashes match the
historical direct-read after session; the compiler, Release VM and controller
images are unchanged.

Five-sample comparison medians in milliseconds, for 257 seek/read/hash operations
(16.0625 MiB, OS-cache hot; setup/open/close excluded):

| Workload | Wio native original -> previous -> current | Wio VM original -> previous -> current | C++ current | C# current |
| --- | ---: | ---: | ---: | ---: |
| Exact 64 KiB | 65.8445 -> 36.0935 -> 39.3734 | 6619.8585 -> 3713.8901 -> 5789.2383 | 43.8652 | 35.1273 |
| Some 64 KiB | 67.2972 -> 40.6438 -> 39.0302 | 5996.0838 -> 3623.6411 -> 4310.3273 | 42.9091 | 28.7246 |

The unmodified C++ reference moves from 27.4835/26.4156 ms in the previous
session to 43.8652/42.9091 ms here; C# moves from 28.4912/26.8216 to
35.1273/28.7246 ms. These sessions are not stable enough to accept cross-session
speedups or regressions. Current native comparison ranges are 18.0%/93.2%,
VM ranges 30.6%/41.5%, C++ ranges 38.0%/8.5%, C# ranges 59.6%/17.8%.
Native A/A median differences are 1.9%/0.7%, but that does not repair noisy
samples or independently qualify a universal 1.05-1.10x C++ claim. C++ ifstream,
C# FileStream and Wio OS-handle paths do not have identical syscall/error
implementations. Native is in the same observed band in this session, not proven
faster or within the target tolerance. **This slice establishes no accepted
execution-speed gain.**

Separate three-sample final Wio phase, milliseconds:

| Workload | Native previous -> current | VM previous -> current |
| --- | ---: | ---: |
| Exact 64 KiB | 37.9962 -> 41.4784 | 3426.2599 -> 5434.2137 |
| Some 64 KiB | 36.5215 -> 41.3442 | 3831.5553 -> 5646.6025 |

The final exact-native A/A median difference is 14.2%, above the 10% limit;
final native ranges are 20.7%/39.6% and VM ranges 36.9%/57.0%. Keep these less
favorable final results as well as the comparison phase. The larger VM timings
must not be presented as an interpreter improvement: no interpreter source or
binary changed. Its sequence bridge still materializes general VM Values and
per-element native ABI values. Bulk byte transport is separate follow-up work,
not something O2 flags alone solve.

### Memory, file size and build cost

For both modes, the actual Wio native allocation window stays at **514
allocations/frees**, **16855088 requested/freed bytes**, **65632 peak live bytes**,
three peak blocks, zero final live blocks/bytes and zero allocation failures.
The original direct-read baseline had 771 allocations and 33697840 requested
bytes. The earlier allocation reduction is preserved, not improved by this
flag-only slice. These are separate diagnostic new/delete windows, not VM heap
measurements or malloc/RSS accounting.

Comparison median process peak RSS, KiB:

| Workload | Native previous -> current | VM previous -> current | C++ current | C# current |
| --- | ---: | ---: | ---: | ---: |
| Exact 64 KiB | 9452 -> 9292 | 25200 -> 24824 | 5416 | 37012 |
| Some 64 KiB | 9448 -> 9292 | 25200 -> 24820 | 5416 | 37028 |

Small RSS changes alone are not proof of a heap optimization. The deterministic
file-size changes are more substantial:

| Artifact | Previous bytes | Current bytes |
| --- | ---: | ---: |
| Runtime archive | 7344752 | 1721974 |
| Native executable | 6873283 | 5744602 |
| Native bridge DLL | 6050285 | 2452902 |
| Allocation-diagnostic executable | 10052501 | 8930797 |
| Generated C++ | 3579897 | 3579899 |
| Bytecode | 2707470 | 2707472 |

The two-byte generated/bytecode growth is path/source-map variation, not a WIR
optimization. These disk sizes are not resident-memory totals.

Recorded build durations: emit C++ 4.59s, native O2 compilation/link 142.86s,
allocation diagnostic 154.87s, C++ reference 1.83s, .NET Release build 4.18s,
bridge plus bytecode production 45.22s. They are build-command elapsed times,
not directly comparable clean compiler-throughput benchmarks: inputs, imported
library volume, runtimes and caches differ. Generated build cost remains open
for the later compile-time pass; it is not hidden behind a runtime-size gain.

### Evidence and remaining scope

Source `87c4d70d9ba080d78c87e950e436ae3ead3779ef` passes all three scoped
platform jobs: Windows Debug, Ubuntu Release and macOS 15 Release,
[CI 39174868641](https://github.com/tynes0/wio/actions/runs/39174868641).
This checks the expanded N04 gate and the workflow's scoped N03 regressions,
not every historical language test or a cross-platform performance guarantee.

- [Selected samples](evidence/n04-runtime-profile-after-2026-10-19.json),
  [build commands/times](evidence/n04-runtime-profile-build-2026-10-19.json),
  [native allocation windows](evidence/n04-runtime-profile-memory-2026-10-19.json).
- [Artifact/source identities and install/reuse checks](evidence/n04-runtime-profile-artifacts-2026-10-19.json).
- [Two passing local gates](evidence/n04-runtime-profile-tests-2026-10-19.xml),
  [21 process contract records](evidence/n04-runtime-profile-contract-2026-10-19.json).
- Historical original/previous raw sessions and their limits remain in the
  [direct-read report](WIO_BYTE_IO_OPTIMIZATION.md).

N04 remains open for reusable buffers, batching/syscalls, bulk/pool/span
transport and remaining std consolidation. KI-010 computed VM globals remain
open. N05 has not started; N14 timing acceptance and the later VM/compile-time
optimization passes are not closed by this change.
