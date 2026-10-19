# N04 borrowed byte-range writes

Implementation slice, 2026-10-20. N04 and N14 remain open. This does not start
N05, migrate the std error model, or change VM array ownership/transport.

## Change and motivation

Existing `WriteBytes(file, bytes: u8[])` remains unchanged. A new synchronous
`WriteBytesRange(file, bytes: view u8[], offset, count)` uses the source's existing
storage. A caller can write the valid prefix returned by `ReadSomeBytesInto`
without first materializing an owned `Slice`. The native helper accepts both
`span<const unsigned char>` and `const vector<unsigned char>&`, selects the
checked subrange and delegates to the existing exact-write loop.

```wio
let read = io::ReadSomeBytesInto(ref input, ref buffer);
io::WriteBytesRange(ref output, ref buffer, 0usize, read);
```

Bounds are checked using `offset > size || count > size - offset`, avoiding
addition overflow. Invalid ranges throw `out_of_range` before any I/O; there is
no silent clamp or partial range selection. Empty ranges are valid at the end
and return zero even for a closed file, matching existing `WriteBytes` behavior.
Nonempty writes preserve `FileWriteException`, native errors and requested/
written byte counts. They do not promise transactional rollback on I/O failure.
Source contents, size, capacity and lifetime are unchanged, and the native
borrow is not retained. Reserved capacity is not initialized length.

The native path avoids a payload allocation/copy; the VM bridge still uses its
existing array marshalling. Native zero-copy does not imply VM zero-copy.
ByteBuffer bulk integration is deferred to its own slice: `std/buffer.wio` has
uncommitted user edits, preserved here without formatting or staging them.

## Local correctness evidence

`tests/performance/byte_write_range/verify.py` passes all 25 processes: generated
native O0/O2, both VM bridge levels, invalid/overflow ranges, closed-file failures,
short-read prefix output, untouched source suffix and native allocation contracts.
The native helper performs 257 writes with zero allocations/frees in the measured
window, with caller storage prepared beforehand. This is not zero total memory
or a VM allocation claim. The C++ gate also checks span-relative offsets,
reserved-empty storage, error byte counts and unchanged cursor on rejected input.

The new gate is registered in CTest and the three-platform backend CI workflow;
local success is not a claim that that new CI run has already passed.

## Measurement boundary

The new fixture compares explicit owned-slice writes against borrowed-range
writes with identical bytes, offsets, operation counts and output oracle. It
includes generated Wio native, Wio VM, C++ and C#. It is an **API/storage-strategy
comparison**, not an unchanged-source compiler speedup. File setup/open/close
and full-byte output verification are outside the kernel. Writes overwrite the
same 65519-byte region in OS cache, without a durability flush; this is not cold
disk throughput. Reference stream implementations are not identical syscalls.

Use 1000 measured native/C++/C# samples and 20 VM samples with native A/A and the
separate final Wio phase. Keep all raw data, failures, image hashes and timing
qualification. Do not attribute old owning-read phase changes to this new API.
Fixture instructions: `tests/performance/byte_write_range/README.md`.
