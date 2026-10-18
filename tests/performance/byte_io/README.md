# N04 owned and reusable byte I/O

Two same-byte workloads exercise `io::ReadBytes` and `io::ReadSomeBytes` with a
64 KiB file containing all byte values, including NUL and invalid UTF-8. Each
iteration seeks to the start, obtains an independent owning byte array and
hashes every byte. Two workload warmups precede timing; setup, open and close
are outside the kernel. This measures a hot OS-cache composite, not cold disk
throughput or isolated read syscall latency.

Two additional cases use `ReadBytesInto`/`ReadSomeBytesInto`. All three languages
prepare one 64 KiB caller-owned buffer outside the measured kernel/allocation
window, then reuse it for every seek/read/hash iteration. Compare owned and
reused cases within the same session, not just across historical noisy sessions.
This is an opt-in storage strategy, not a claim that owning snapshots can alias
a reusable buffer. The original two workload modes remain available, but the
expanded source files/binaries have new identities and must not be represented
as the previous frozen benchmark image.

```wio
mut bytes: u8[] = [];
bytes.Reserve(65536usize);
for (index in 0usize..<65536usize)
{
    bytes.Push(0u8);
}

let count = io::ReadSomeBytesInto(ref file, ref bytes);
// Only bytes[0..<count] was read. Count()/Capacity() are not read lengths.
```

The destination's existing length is the maximum writable length; reserve alone
does not add elements. Successful exact reads fill that entire length. Some
reads return a valid prefix count, keep the length, and leave the unread suffix
untouched, including EOF returning zero. Empty buffers return zero even for a
closed handle, matching existing zero-length byte reads. Nonempty exact short
reads throw the existing error/count/cursor metadata; destination contents on
failure are unspecified, with no transactional rollback guarantee. Callers must
not consume the whole buffer after a short read. Native vector/span paths do no
payload allocation. VM sequence marshalling can still allocate/copy; buffer reuse
does not imply zero-copy across the current VM native bridge.

Both matched workloads construct their mode from explicit local flagset values.
The first discovery session used `io::OpenReadBinary`, exposing the separate VM
computed-global defect KI-010. `global_mode_repro.wio` preserves that reproduction;
it is not a successful VM test or a timing denominator. Keep that failed session
separate from the matched explicit-mode before/after comparison.

Wio uses raw native file handles, C++ uses `ifstream`, and C# uses a FileStream
with managed buffering disabled. They share bytes, seeks, ownership and FNV
output, not identical syscall implementations, allocation policy or failure
contracts. The two modes use a full-size regular-file read during timing; short
reads, EOF and error metadata are separate correctness/retention gates.

```sh
python tests/performance/byte_io/prepare.py --output build/byte-io-before --label before --count 257
build/tools/wio-bench/Debug/wio-bench run build/byte-io-before/manifest.json --output build/byte-io-before-results
# Apply the candidate and rebuild the backend runtime archive.
python tests/performance/byte_io/prepare.py --output build/byte-io-after --sources build/byte-io-before --label after --count 257
build/tools/wio-bench/Debug/wio-bench run build/byte-io-after/manifest.json --output build/byte-io-after-results
```

Adapt executable suffixes/tool paths for the platform. Python prepares frozen
builds and the independent byte oracle; Wio owns sample scheduling, A/A controls
and the separate final native/VM phase. Retain failures, noise and all samples;
do not trim or rerun for a preferred result. Run builds, tests and timing serially.

The current fixture defaults to an explicit O2 native bridge with FP contraction
off. Select `--bridge-optimization default` or `O0` only for an intentionally
recorded control. Other fixtures using the shared harness keep their previous
default. `--reuse-build` cannot relabel bridge flags or a known runtime build
fingerprint; historical manifests without that fingerprint remain unknown.
Build the backend archive with the intended `WIO_BACKEND_RUNTIME_PROFILE` and
retain its profile file. The old default-flag N04 evidence is historical, not a
fully optimized native-boundary VM result.

The separate diagnostic image reports global new/delete traffic, not malloc,
VM heap accounting or RSS. `contract.cpp` compares the actual new runtime reads
with the previous two-vector conversion in the same process. It checks full-read
allocation/requested/peak storage, partial-read error counts/cursor, owning
snapshots, all byte values, speculative capacity at short reads/EOF and the
existing zero-count invalid-handle behavior. `semantics.wio` covers native O0/O2
and the VM with both O0/O2 native bridges, including translated partial/closed
errors. Performance sampling uses the Release VM host independently.

The extended contract also checks repeated reusable reads with zero C++ payload
allocations, fixed storage identity/size/capacity, short-read suffix preservation,
span canaries, exact error counts, reserved-but-empty buffers, and Wio native/VM
mutation/snapshot/EOF behavior. Keep initial failed fixture builds as evidence.
