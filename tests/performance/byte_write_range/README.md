# N04 checked byte-range writes

`std::io::WriteBytesRange(ref file, ref bytes, offset, count)` borrows an existing
`u8[]` without materializing a `Slice`. This is especially useful after
`ReadSomeBytesInto`, whose return value is the initialized prefix to write, not
the destination's capacity or entire length. No existing API changes.

```wio
let count = io::ReadSomeBytesInto(ref input, ref buffer);
io::WriteBytesRange(ref output, ref buffer, 0usize, count);
```

The input range is checked before I/O with overflow-safe subtraction. Invalid
ranges throw; they do not clamp. An empty range at the end is valid and returns
zero even for a closed file, matching existing `WriteBytes` empty behavior.
Nonempty writes retain the existing exact-write loop and exception byte counts.
Source size, capacity and contents do not change. The native borrow is synchronous
and not retained. The VM bridge supports the API but may marshal/copy its arrays;
zero-copy VM transport is **not** promised.

## Correctness and allocation contract

```sh
python tests/performance/byte_write_range/verify.py --wio build/app/Debug/wio.exe --vm build/app/Release/wio-vm.exe --runtime build/runtime/backend/libwio_runtime.a --cxx g++ --output build/NEW_CONTRACT
```

The gate checks native O0/O2, both bridge optimization levels, short-read valid
prefix versus untouched suffix, closed files, zero-length writes, reserved but
uninitialized capacity, overflow/out-of-bounds, native span-relative offsets,
unchanged cursor on rejected ranges, source stability, and zero native payload
allocations across 257 writes. Output bytes are checked, not just byte counts.

## Same-output strategy comparison

`prepare.py` freezes generated Wio, C++, C#, bytecode/bridge and allocation-probe
images. Both modes repeatedly seek to zero and overwrite 65519 bytes selected
from offset 7 in a 65536-byte source. Mode 0 explicitly materializes a slice;
mode 1 writes a borrowed range. All bytes and the exact output length are checked
outside the timed window. The protocol checksum includes the full output FNV
and total reported writes; Python supplies an independent oracle. Two internal
warmups precede every timed kernel. Input preparation, file opening/closing and
verification are excluded from the kernel and allocation window.

This is a public-API/storage-strategy comparison, **not an unchanged-source
compiler speedup**. The references use C++ `ofstream` and unbuffered C#
`FileStream`; these are not identical OS implementations or error contracts.
Writes hit the OS cache, without a durability flush, so this is not physical
disk throughput. Allocation counters cover global C++ new/delete, not RSS,
kernel buffers, VM ownership or managed GC allocations.

```sh
python tests/performance/byte_write_range/prepare.py --output build/NEW_IMAGES --label range-write --count 257
python tools/wio-bench/sampling_profile.py --manifest build/NEW_IMAGES/manifest.json --output build/NEW_PROFILE.json --repetitions 1000 --vm-repetitions 20
build/tools/wio-bench/Debug/wio-bench run build/NEW_PROFILE.json --output build/NEW_RESULTS
```

Use the actual executable suffix/configuration for the platform. Do not reuse
an output directory, overlap heavy local work, trim slow samples or relabel
different-phase measurements as a gain. N04 remains open for ByteBuffer bulk
operations and the remaining I/O inventory; timing acceptance is part of N14.
