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

The selected source's platform CI and final performance/heap/size supplement
are pending at this source commit. No accepted speedup is claimed from the
unqualified historical sessions. N04 remains open for reusable buffers,
batching/syscalls, bulk/pool/span transport and remaining std consolidation.
