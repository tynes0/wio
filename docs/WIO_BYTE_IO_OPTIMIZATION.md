# N04: direct owned byte reads

Work started 2026-10-18 and continued 2026-10-19. This is the first N04 slice,
not closure of byte/buffer/I/O, all std cleanup or N14 timing acceptance.

## Cost identified

Wio `io::ReadBytes`/`ReadSomeBytes` bind to the runtime's unsigned-byte helpers.
Those helpers first obtain an owning `vector<std::byte>`, then allocate a second
`vector<unsigned char>` and push every byte into it. A successful 64 KiB read
therefore needs two payload buffers before the normal Wio result transport.
The existing write helper already forwards a counted byte span; it does not
have this particular conversion allocation.

The candidate reads directly into the final unsigned-byte vector through the
same raw read/exact-read primitives. It does not reinterpret a vector object,
transcode bytes, weaken error checks, change Wio signatures or replace file
ownership. Existing string and `std::byte` reader APIs remain separate.

## Behavior and retained capacity

- Exact reads keep EOF versus partial-read errors, requested/read byte counts,
  native errors and cursor effects. No partial success is returned as exact data.
- Some-read results contain only the bytes actually read, never initialized or
  uninitialized capacity beyond the returned size.
- Empty/EOF returns discard speculative storage. If the read result's capacity
  exceeds twice its size, compact it rather than retaining an arbitrary request
  size in a small long-lived result. That exceptional path may allocate/copy;
  this is not an unconditional zero-copy promise.
- Moderate short reads may retain up to twice their size, unlike the old second
  vector normalized to the returned count. Capacity and allocation tradeoffs
  must be reported separately from full-read savings.
- Owned snapshots do not alias later reads. All byte values, including NUL and
  invalid UTF-8, are byte data; no string conversion is needed.
- Existing zero-count behavior on invalid handles is preserved, not silently
  changed into a new validation error.

## Measurements and correctness scope

The Wio-owned fixture lives in `tests/performance/byte_io`. Both workloads read
64 KiB from a regular file, seek for each read and hash every byte. File setup,
open and close are outside timing; two workload warmups make this a hot-cache
composite. C++ uses ifstream, C# an unbuffered FileStream, Wio raw OS handles:
same output/ownership, not identical syscall implementations or cold disk speed.
Python independently derives the checksum; it does not own timing samples.

Full native allocation windows and actual runtime-versus-old-helper C++ probes
are separate from RSS and VM heap/transport costs. Wio semantics cover native
O0/O2 and VM/native bridge. The focused gate is `wio_n04_byte_io`; the old full
corpus is not rerun locally for this slice.

### Discovery failures are not timing baselines

The first preparation failed because the fixture passed `ref file` when its
parameter was already a ref. That fixture error was corrected before sampling;
the failed build record remains separate.

The first measured fixture used `io::OpenReadBinary` and exposed **KI-010**:
computed VM module globals retain their defaults. Native has 64 correct samples
and VM has 20 failures with `invalid_mode`; there is no VM speed denominator in
that session. The minimal global-mode reproduction expects success but exits 17
on the current Release VM. No VM initialization fix is claimed by this slice.

Both matched before/after fixtures instead construct the identical mode from
local flagset values. Keep the failed discovery session and the new explicit-mode
session separate; never splice their timing samples into one comparison.

## Remaining N04 work

Reusable caller-owned read buffers, byte-pool/ByteBuffer bulk paths, span/view
transport, batched writes and syscall counts, read-all/filesystem and encoding
round trips, streaming/large-file cases, failure/allocation pressure and the
coordinated Result-first/formatting cleanup of affected std modules remain open.
Dirty user-owned `std/buffer.wio` and playground files are preserved. KI-010 is
an independent VM correctness issue, not resolved by the local-mode workaround.

## Selected implementation and local gate

The direct unsigned-byte runtime read is implemented. The new scoped CTest gate
passes **1/1 in 115.61 seconds**, with 17 processes covering native O0/O2, actual
runtime versus old-helper allocation checks, and VM/native-bridge success/error
cases. The isolated full-read payload allocation count falls 2 -> 1; requested
and peak tracked bytes fall 131072 -> 65536. No old full suite was run locally.
The first gate failed on an unparenthesized test cast; its XML/process log is
retained separately from this passing gate.

Compiler and Release VM binaries are unchanged. The backend archive is rebuilt;
consumers must rebuild against the updated runtime/header to use the new
out-of-line helper instead of their previously compiled inline conversion.
Public Wio signatures and module/native ABI descriptor layouts are unchanged.

The source commit records the local gate and before/discovery evidence; the
following measurement-only supplement adds the selected after session. Platform
qualification and timing acceptance are independent of the local gate.

Source `3c9e268` passes all three jobs in
[CI 39169819998](https://github.com/tynes0/wio/actions/runs/39169819998):
Windows Debug, Ubuntu Release and macOS 15 Release, including the new N04 gate.
This is the scoped backend-closure workflow, not the whole historical corpus.
The [CI snapshot](evidence/n04-byte-io-ci-2026-10-19.json) pins the full SHA;
the subsequent measurement/report commit changes no production code.

### Runtime build-policy finding

The generated programs in this experiment use GCC O2, but the local CMake
backend-archive command for `std_io.cpp` supplies **no explicit optimization
flag**. The same archive build policy is used in the matched before/after pair;
only the byte-read implementation changes. Do not describe these results as a
fully Release-optimized compiler/runtime stack. Compiler and VM host build
configurations are separate again.

The VM uses a Release host, but this fixture's bridge build uses the compiler
driver's default native command without an explicit `-O` backend argument.
Thus VM rows include a driver-default bridge and its array codecs, not a
fully O2-compiled native boundary. Do not attribute their whole cost, or the
large before/after movement, to the VM interpreter. The byte-read source change
also removes the previous per-byte push loop from that bridge path.

Before N14 acceptance, audit backend runtime compile flags/configuration and
package consistency, explicit bridge flags, and qualify an optimized runtime/
bridge as its own measured candidate. This may affect many runtime modules, so
do not silently change it midway through an isolated byte-read experiment. The
current finding does not establish how every installed release archive was built.

The next N04 candidate should isolate that runtime/bridge build-policy issue
before further cross-language performance acceptance, then continue reusable
buffers and batched I/O. Do not mix flag changes into this source-only result.

## Before/after measurement supplement, 2026-10-19

Both explicit-mode sessions complete with **84/84 correct samples**, including
warmups and native A/A controls. Affinity, unchanged artifact and power-profile
checks pass. Both have `timingQualified: false`. All samples are retained; no
retry, trimming or system power/priority change was used to select a result.
The controller's nonzero qualification exit is not a workload failure.

Five-sample comparison medians, milliseconds for 257 reads (16.0625 MiB plus seeks
and full-byte FNV):

| Workload | Wio native before -> after | Wio VM before -> after | C++ before -> after | C# before -> after |
| --- | ---: | ---: | ---: | ---: |
| Exact 64 KiB | 65.8445 -> 36.0935 | 6619.8585 -> 3713.8901 | 27.4504 -> 27.4835 | 28.7193 -> 28.4912 |
| Some 64 KiB | 67.2972 -> 40.6438 | 5996.0838 -> 3623.6411 | 27.3145 -> 26.4156 | 28.9004 -> 26.8216 |

Separate three-sample final Wio phase, milliseconds:

| Workload | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Exact 64 KiB | 69.9481 -> 37.9962 | 6255.7034 -> 3426.2599 |
| Some 64 KiB | 76.4098 -> 36.5215 | 6722.4079 -> 3831.5553 |

Do not turn the apparent changes into an accepted speedup: before exact native
range is 79.5%, after 35.5%; after exact VM range is 64.5%. Reference C++ ranges
after are 63.8%/65.6%. The large movement of the unmodified VM host includes
native bridge code and noise, not a new interpreter optimization. Its byte-array
transport/native-boundary cost remains substantial; profile that separately,
with explicit optimized bridge/runtime flags before comparing VM limits.

### Actual Wio heap window

Both workload modes have the same separate diagnostic counts for 257 reads:

| Metric | Before | After |
| --- | ---: | ---: |
| Allocations = frees | 771 | 514 |
| Requested = freed bytes | 33697840 | 16855088 |
| Peak tracked live bytes | 131120 | 65632 |
| Peak tracked blocks | 3 | 3 |
| End live bytes / blocks | 0 / 0 | 0 / 0 |
| Tracked allocation failures | 0 | 0 |

One payload allocation per call is eliminated. The isolated C++ control proves
payload allocations 2 -> 1 and requested/peak 131072 -> 65536. The actual Wio
window also includes normal transport/handle costs, so it is not an exact 50%
reduction in every aggregate metric. These are new/delete windows, not malloc
traffic, VM heap counts or OS resident-memory savings.

Comparison median process peak RSS, KiB:

| Workload | Wio native before -> after | VM before -> after | C++ before -> after | C# before -> after |
| --- | ---: | ---: | ---: | ---: |
| Exact 64 KiB | 9456 -> 9452 | 25256 -> 25200 | 5468 -> 5468 | 37016 -> 37008 |
| Some 64 KiB | 9460 -> 9448 | 25256 -> 25200 | 5464 -> 5464 | 37000 -> 37020 |

Tiny RSS differences are not proof of allocator savings. Native.exe grows
6853925 -> 6873283 bytes (+19358), and bridge.dll grows 6035607 -> 6050285
(+14678). The helper change therefore has a code-size cost under the current
runtime build policy. Generated C++/bytecode decrease by 15 bytes in these builds;
source/debug path differences are not a WIR or bytecode optimization.

### Provenance and evidence

The before fixture was new/uncommitted at parent `5db6595`, with production
runtime sources untouched. After uses committed source
`3c9e2688e81beedfd980a3a0b3c949c836587cd9`. All five frozen workload source hashes
agree; compiler, Release VM and Wio controller images are unchanged. The backend
archive and native images are rebuilt. GCC is 15.2; C# is .NET 9 Release with
tiering disabled. Keep the default runtime/bridge optimization caveat above.
Builds and tests do not overlap local timing sessions.

- [Before samples](evidence/n04-byte-io-before-2026-10-19.json),
  [after samples](evidence/n04-byte-io-after-2026-10-19.json).
- [Before build](evidence/n04-byte-io-build-before-2026-10-19.json),
  [after build](evidence/n04-byte-io-build-after-2026-10-19.json),
  [artifact/source identities](evidence/n04-byte-io-artifacts-2026-10-19.json).
- [Before heap](evidence/n04-byte-io-memory-before-2026-10-19.json),
  [after heap](evidence/n04-byte-io-memory-after-2026-10-19.json).
- [Passing local gate](evidence/n04-byte-io-tests-2026-10-19.xml),
  [17 process records](evidence/n04-byte-io-contract-2026-10-19.json).
- [Initial ref-argument fixture build](evidence/n04-byte-io-initial-build-2026-10-19.json),
  [initial cast fixture gate](evidence/n04-byte-io-initial-tests-2026-10-19.xml),
  [KI-010 discovery samples](evidence/n04-byte-io-global-mode-failure-2026-10-19.json).

N04 remains in progress. This closes only the direct owning-read conversion
slice and its local correctness/memory evidence, not KI-010, optimized runtime
build policy, the VM performance pass or N14 acceptance.
