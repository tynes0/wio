# N04 owned byte I/O

Two same-byte workloads exercise `io::ReadBytes` and `io::ReadSomeBytes` with a
64 KiB file containing all byte values, including NUL and invalid UTF-8. Each
iteration seeks to the start, obtains an independent owning byte array and
hashes every byte. Two workload warmups precede timing; setup, open and close
are outside the kernel. This measures a hot OS-cache composite, not cold disk
throughput or isolated read syscall latency.

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

The separate diagnostic image reports global new/delete traffic, not malloc,
VM heap accounting or RSS. `contract.cpp` compares the actual new runtime reads
with the previous two-vector conversion in the same process. It checks full-read
allocation/requested/peak storage, partial-read error counts/cursor, owning
snapshots, all byte values, speculative capacity at short reads/EOF and the
existing zero-count invalid-handle behavior. `semantics.wio` covers native O0/O2
and the Release VM/native bridge, including translated partial/closed errors.
