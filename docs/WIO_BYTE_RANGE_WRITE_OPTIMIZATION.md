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
The OS can still copy the selected bytes into its file cache; no kernel
zero-copy I/O or durability guarantee is added.

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

The new gate is registered in CTest and the three-platform backend CI workflow.
Source `e253f7b` passes Windows Debug, Ubuntu Release and macOS 15 Release in
[CI 39329704323](https://github.com/tynes0/wio/actions/runs/39329704323), including
the new N04 range gate and the existing scoped N03/N04/controller contracts.
This is not a full historical suite or timing qualification. Retain
`docs/evidence/n04-write-range-ci-2026-10-20.json`.

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

### Generated-Wio allocation comparison

Frozen O2 images on source `e253f7b`, 257 writes per kernel, caller input and
file handles prepared before tracking. Both output checksums are
`18032860802463305489` and every output byte/length is checked outside the window.

| Native generated-Wio path | Allocations/frees | Requested/freed bytes | Peak tracked bytes/blocks | End live/failures |
| --- | ---: | ---: | ---: | ---: |
| Owned Slice + existing WriteBytes | 514 / 514 | 33676766 / 33676766 | 131038 / 2 | 0 / 0 |
| Borrowed WriteBytesRange | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

Generated code confirms two owned payload materializations in the legacy path:
`ArraySlice` followed by the by-value native entry. The new entry passes
`as_const(source.read())` directly to the checked helper. No existing API was
silently changed to borrow. The zero peak above is only the measured global
new/delete window, excluding the caller-owned source allocated earlier, OS file
cache, runtime metadata and process RSS. Allocation diagnostics are separate
images, not the timed binaries.

The fresh 1000/20 timing session retains each raw row immediately and checkpoints
the full growing report every 1000 rows (rather than 100); this reduces reporting
work outside child kernels, without changing kernel work, counts, thresholds,
controls, affinity or output verification. Treat it as a new session/protocol
observation, not a direct timing delta against the older read/hash session.

## Completed 1000/20 session

All **12096/12096** raw rows pass: comparison/final samples, warmups and native
A/A lanes. Image/input hashes are unchanged, affinity is applied, and the power
profile is unchanged. The standalone verifier independently recomputes all
twelve summaries, native controls, range/drift qualification, sample budgets,
phase ordering, journal/report equality and RSS medians. Extracting the ZIP and
verifying it again yields the identical summary, including the archived full
payload/output bytes and allocation counters. No retries or trimming.

### Comparison phase: same bytes, two API strategies

Median kernel milliseconds; 1000 samples per native/C++/C# row, 20 per VM row.
Each kernel performs 257 seek/overwrite operations of 65519 bytes. Both paths
produce checksum `18032860802463305489` and verify the complete output.

| Implementation | Owned slice/copy | Borrowed range | Whole-process median peak RSS KiB, copy → range |
| --- | ---: | ---: | ---: |
| Wio native | 16.10180 | 0.87995 | 9320 → 9316 |
| C++ | 8.06575 | 0.86305 | 5440 → 5436 |
| C# | 2.30710 | 0.78685 | 37080 → 24528 |
| Wio VM | 1661.75675 | 1098.25230 | 27920 → 24800 |

The observed native strategy ratio is 18.2985x, and the range native/C++ ratio
is 1.0196x. These are **descriptive medians**, not accepted speedups or a universal
2% native overhead guarantee. Existing callers of owned `WriteBytes` do not
automatically change behavior: they must use the new borrowed-range API to
avoid the intermediate owned Slice. The reference copy lanes materialize one
owned slice; Wio's legacy by-value call additionally copies that slice. This
difference is part of the API/transport cost being exposed, not identical
allocator work across languages.

Native tracked transient allocation drops by 33676766 bytes and 514 calls for
this kernel, but whole-process RSS is almost unchanged. The caller buffer,
metadata and process baseline remain live outside the counter window. The VM
RSS observation is not an allocation-counter or zero-copy-transport claim.

### Separate final phase

| Implementation | Owned slice/copy median ms | Borrowed range median ms | Samples per path |
| --- | ---: | ---: | ---: |
| Wio native | 16.74945 | 1.04605 | 1000 |
| Wio VM | 1643.10625 | 1110.79965 | 20 |

Do not compare final native time to comparison-phase C++ as if simultaneous.
The final native strategy ratio is about 16.01x, but has the same qualification
caveat. No compiler, runtime image or workload change occurs between phases.

### Timing qualification and tails

Overall **timingQualified=false** (collector exit 2), not a correctness failure.
No thresholds were relaxed. Comparison native A/A median differences are only
0.522% / 0.108% (copy/range), and final differences 0.063% / 0.129%; nevertheless,
range/tail limits fail. Native comparison SD is 21.1444 / 3.0652ms, versus MAD
0.85535 / 0.03485ms; p95 is 22.2820 / 1.359525ms and p99 32.7237 / 2.213657ms.
The relative ranges are 3946.6% / 10906.5%. C++/C# also fail the range limits;
C# copy drift additionally exceeds 10%. Do not replace medians with best cases
or delete outliers. Scheduling, filesystem/cache behavior and background work
are possible contributors, **not established causes** without telemetry.

All final VM rows and comparison VM-copy are individually statistics-stable,
but the session remains unqualified. N14 timing acceptance stays open, as does
N04 ByteBuffer bulk/remaining I/O inventory. N05 has not started.

## Reproducible evidence

- `docs/evidence/n04-write-range-contract-2026-10-20.json`: 25 local processes.
- `docs/evidence/n04-write-range-ci-2026-10-20.json`: all three platform jobs green.
- `docs/evidence/n04-write-range-memory-2026-10-20.json`: generated-Wio counters.
- `docs/evidence/n04-write-range-summary-2026-10-20.json`: independently recomputed statistics.
- `docs/evidence/n04-write-range-archive-check-2026-10-20.json`: extracted archive verification.
- `docs/evidence/n04-write-range-raw-2026-10-20.zip`: raw results/journal/report,
  frozen manifest/build/memory records, fixture sources/payload/output and verifier;
  no binaries or generated C++ containing unrelated dirty std sources.

ZIP: 670259 bytes, SHA-256
`0c47b762cfb5ed5d8ed3986a04580bef14335f86cd189ac5a0360af8e671fdae`.
It is evidence with declared image hashes, not a hermetic toolchain package.

```sh
python -m zipfile -e docs/evidence/n04-write-range-raw-2026-10-20.zip build/NEW_RANGE_VERIFY
python build/NEW_RANGE_VERIFY/verify.py build/NEW_RANGE_VERIFY
```
