# N04 ByteBuffer borrowed block writes

`buffer.WriteBlock(ref values)` copies one contiguous block into the existing
buffer at its current position, extending when necessary. It borrows `byte[]`
for the call and does not retain or change the source. Position advances by
the count; overwrite preserves the untouched suffix; empty input is a no-op.
Native reserved storage is reused. VM copy-back preserves capacity, but can
marshal/allocate replacement storage. Native self-aliasing remains snapshot-equivalent.

Unlike `Write(byte[])`, **WriteBlock does not invoke virtual per-byte Write
hooks**. The old owned/snapshot Write overload remains unchanged, including
derived overrides. This distinction is deliberate: blindly replacing its loop
would break inheritance. There is no RTTI/self-handle guard or implicit retain
added to the old method. Clone's destination is a newly constructed exact
ByteBuffer, so Clone can use WriteBlock without bypassing a derived hook.

```wio
let values: byte[] = [1u8, 2u8, 3u8];
let buffer = heap::ByteBuffer(1024usize);
buffer.WriteBlock(ref values);
buffer.Rewind();
buffer.WriteBlock(ref values);
```

Native nonalias growth inserts only the known tail, then memcpy overwrites the
existing prefix. It avoids resize/zero-fill and per-byte method dispatch. Growth
finishes before existing bytes are changed, so native bad_alloc leaves the old
destination intact. Self-alias growth reacquires the pointer after resize and
uses memmove. Bounds/maximum-size checks precede addition and mutation. Capacity
growth policy for unreserved buffers may differ from individual Push calls;
size, bytes, position and explicit reserve guarantees are preserved. VM uses
its existing array bridge/marshalling; do not claim native zero-copy transport
or whole-operation transactional behavior for VM marshalling failures.

## Focused correctness gate

```sh
python tests/performance/byte_buffer_bulk/verify.py --wio build/app/Debug/wio.exe --vm build/app/Release/wio-vm.exe --runtime build/runtime/backend/libwio_runtime.a --cxx g++ --output build/NEW_BULK_CONTRACT
```

Native O0/O2 and both VM bridge levels check overwrite, extension, unchanged
tail/source/reserve, Clone isolation, empty blocks, legacy derived hooks, new
block semantics on derived objects, finalizer use and exactly-once cleanup.
Native exhaustive tests compare 23001 distinct-input combinations and 1122
alias cases against the original scalar algorithm. Reserved helper loops have
zero heap traffic; a separate executable fault-injects allocation failure for
overlapping alias and nonalias growth. No timing uses those diagnostic images.

## Timing and memory

The frozen legacy fixture uses the old Write overload. The block fixture changes
exactly three call sites to WriteBlock(ref source); it is an **API-strategy
comparison**, not an unchanged-source compiler gain. Each iteration clears a
reserved 2048-byte buffer, writes 1024 bytes, seeks to 512, overwrites/extends,
rewinds and overwrites again. Final count/position are 1536/1024. Full output
validation and ToArray are outside kernel/counter windows. Work input snapshots
are owned in all languages; C# explicitly clones once. New block arguments are
borrowed in Wio/C++/C#. Reference class/struct/GC/RC machinery is not identical.

Freeze legacy images before modifying std; keep their imported std input hashes.
Prepare the block images after the change. `combine.py` verifies the bounded
source delta, equal payload/flags/runtime/count/checksum and creates one session
with legacy/native+VM, block/native+VM and current C++/C# block references. Each
native lane has A/A controls and a final phase; 1000 native/C++/C# and 20 VM
samples. No overlapping heavy local jobs, trimming or retries.

```sh
python tests/performance/byte_buffer_bulk/combine.py --before build/FROZEN_LEGACY --after build/FROZEN_BLOCK --output build/NEW_BULK_PROFILE.json
build/tools/wio-bench/Debug/wio-bench run build/NEW_BULK_PROFILE.json --output build/NEW_BULK_RESULTS
```

Use the platform's actual executable suffix/configuration. New output paths
must not exist. N04 remaining inventory and N14 timing acceptance stay open.
