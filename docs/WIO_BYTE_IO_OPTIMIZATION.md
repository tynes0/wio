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

The explicit-mode before session has 84/84 correct samples, unchanged artifact,
affinity and power checks, but unqualified timing. After measurements and this
source's platform CI qualification remain pending in this source commit; the
measurement supplement will record both costs and failures, not assume a gain.
