# N04 ByteBuffer bulk writes

Implementation slice, 2026-10-20. N04 remains open for the remaining I/O/buffer
inventory; N14 timing acceptance and N05 representation work are not closed.

## API and semantic boundary

`ByteBuffer.WriteBlock(values: view byte[])` borrows a binary block and writes
it at the current position. It overwrites existing bytes, appends the remaining
tail, preserves untouched suffix bytes, and advances position by the count.
An empty block returns before any native call. The source is not modified or
retained. Reserved native storage is reused when the result fits.

```wio
let payload: byte[] = [1u8, 2u8, 3u8, 4u8];
let buffer = heap::ByteBuffer(1024usize);
buffer.WriteBlock(ref payload);
buffer.Rewind();
buffer.WriteBlock(ref payload);
```

The old `Write(byte[])` is **unchanged**: it takes an owned snapshot and calls
the virtual scalar `Write(byte)` for every element. A derived object can override
that scalar hook. Replacing the old loop unconditionally would silently bypass
valid user behavior, so this slice does not do that. WriteBlock explicitly has
block semantics and does not call per-byte hooks, including on derived receivers.
Existing Write callers do not automatically gain the new block performance.

`Clone` can safely use WriteBlock: its destination is a newly constructed exact
ByteBuffer, not a user-derived receiver. Clone still isolates storage, preserves
position and requests the source capacity. It no longer walks the scalar Write
loop when copying bytes. This is an implementation improvement for existing
Clone callers, without claiming an unmeasured Clone speedup percentage.

No RTTI check, owning self-handle guard, new ByteBuffer data field, object-layout
rewrite or native-object proxy requirement is introduced. Tests preserve legacy
derived hooks and exercise block writes during finalization with exactly-once
cleanup. Public new method surface is intentional; this is not a binary-ABI
compatibility guarantee for independently compiled generated object vtables.

## Native bulk algorithm

The helper takes a mutable vector, a position and a readonly source vector.
It checks the position and `count > max_size - position` before addition or
mutation. For distinct vectors it inserts only the known tail, completing any
throwing growth first, then memcpy overwrites the old prefix. It avoids
resize/zero-fill and per-element method calls. For native self-alias it captures
the original count, resizes, reacquires the pointer, and memmove copies snapshot
bytes even after reallocation. Empty input performs no pointer arithmetic.

Native allocation failure leaves destination bytes, size, capacity and pointer
unchanged; the Wio position assignment occurs only after a successful helper
return. Fault injection tests both overlapping alias and nonalias growth.
Unreserved growth policy can differ from repeated Push calls; no exact implicit
capacity-growth sequence is promised. Explicit reserve guarantees remain.

## VM reserve consistency fix

The new gate exposed an existing bridge issue: structural native array copy-back
replaced the VM array with a decoded array whose capacity equaled its new length.
Thus a native block write could lose a previous Reserve(16) despite writing only
four elements. Same-shape copy-back already preserved existing storage.

The copy-back planner now prepares at least the caller's old capacity in the
incoming shape-changing array **before commit**, detaching shared incoming
storage first. It does not change caller values, size, capacity or cell addresses
during planning. Shape-changing arrays have no child actions into that subtree.
Existing target exclusivity, depth, staged validation, projection retirement and
cleanup rules stay intact. New focused host cases check reserved growth, native
clear, shared-incoming isolation and unchanged caller storage before commit.

This is a VM correctness fix required by the new buffer path, not the heavy VM
optimization pass. The bridge still marshals arrays and may allocate replacement
storage; logical capacity preservation does not mean physical storage reuse or
zero-copy VM transport. No general transactional promise is added for arbitrary
VM marshalling/allocation failure.

## Correctness coverage

The new gate checks native O0/O2 and both bridge levels: fill, overwrite, extension,
empty input, reserve, unchanged source/tail, Clone isolation, legacy subclass
hooks, explicit block behavior on subclasses, finalizer calls and exactly-once
cleanup. The native helper is compared against the scalar algorithm for 23001
distinct-input combinations and 1122 alias cases, with and without reserve.
257 reserved helper cycles (three block writes each) perform zero allocations.
Separate bad_alloc injection verifies native growth failure without mutation.

The affected native-container-shape gate also passes locally (25.64s) after
updating the previously stale local Release compiler executable. An earlier
attempt used that stale executable and failed at its unsupported fit surface;
it was not a capacity-fix regression. The final new gate passes all 17 processes
locally, including both VM bridge levels; raw process records are retained in
`docs/evidence/n04-buffer-bulk-contract-2026-10-20.json`. Three-platform CI is
registered but not yet claimed complete here. Only new/affected local gates run.

## Measurement contract

The baseline was frozen before production changes. Its Wio fixture uses legacy
Write. The new fixture changes exactly three call sites to WriteBlock(ref source).
This is an **API-strategy/update comparison**, not an automatic unchanged-source
compiler speedup. Each iteration clears a reserved 2048-byte destination, writes
1024 bytes, seeks to 512 and overwrites/extends, then rewinds and overwrites again.
Final count/position are 1536/1024. Full output and state are checked; ToArray and
full-byte validation remain outside kernel/counter windows. Work input snapshots
are owned in all languages, while block arguments are borrowed. The C++/C# lanes
are current block references, not measurements of the old per-byte-hook API.

The two native preparations retain their recorded runtime archive identities
under the same O2 profile; the header update caused an archive rebuild. Both
frozen bytecode versions run under the same capacity-corrected VM host. Keep
these input differences visible rather than attributing every delta to a single
compiler transformation. The new session uses 1000 native/C++/C# and 20 VM samples,
native A/A lanes, a separate final Wio phase and append-only raw rows. No retries,
trimming, threshold relaxation, power changes or overlapping heavy local jobs.

Fixture and preparation details: `tests/performance/byte_buffer_bulk/README.md`.
Uncommitted user std/playground edits are preserved and excluded from commits.
