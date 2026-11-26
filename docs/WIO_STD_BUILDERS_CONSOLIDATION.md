# Std Builders: Result, Source Rules and Performance

Date: 2026-10-15. This is the first scoped std consolidation slice inside
optimization pass 1, not completion of all std or the backend performance budget.

Follow-up: [native string storage and append-copy optimization](WIO_MANAGED_STRING_OPTIMIZATION.md)
records `79568bd` and a fresh original/previous/current/C++ comparison. The
measurements below remain evidence for this earlier std slice, not current-head
timings or a claim that its remaining managed allocation costs were left unchanged.

## Delivery and boundary

- `7246971`: readability-only migration of `std/builders.wio` and its binary
  consumer: expanded bodies/blocks, explicit object access, declaration/closing
  brace spacing and separate-line bracket attributes.
- `cbfdee6`: Result-first ByteReader signatures, stable read errors, the binary
  consumer migration, reserve and bulk string/byte helpers. No Try compatibility
  API or implicit Option conversion was added.
- `d09b8e2`: capacity-preserving StringBuilder append/line/clear and VM string
  mutation, focused tests and a reproducible paired native benchmark.
- `ddfa2b2`: bulk input helpers take read-only views, avoiding whole-input value
  copies when reading many small strings or writing a large payload.

Pre-existing user changes in `std/buffer.wio`, other std modules and playground
are preserved. ByteBuffer/BytePool remain a later module slice. Full SDK kit
parity and language-wide removal of legacy attribute syntax remain separate
gates. This module uses bracket attributes; other source has not been silently
migrated by a broad formatting command.

## Canonical API changes

All ByteReader reads now return `std::Result<T>`:

| Operation | Success type |
| --- | --- |
| `ReadU8()` | `u8` |
| `ReadU16LE()`, `ReadU16BE()` | `u16` |
| `ReadU32LE()`, `ReadU32BE()` | `u32` |
| `ReadU64LE()`, `ReadVarUInt()` | `u64` |
| `ReadBytes(count: usize)` | `u8[]` |
| `ReadString()` | `string` |

Count/position queries return direct values; write methods retain their void
surface. Allocation failure follows the existing allocator/runtime policy, not
a new recoverable-OOM contract introduced by this slice.
`std::Option<T>` itself remains supported as an explicitly chosen utility.

```wio
let reader = builders::ByteReader(bytes);
let result = reader.ReadU32LE(); // Result<u32>, not Option<u32>
if (result.IsError())
{
    return std::Err<u32>(result.ErrorValue());
}

let next = reader.ReadU32LE!();  // explicit unwrap/panic
let tail = reader.ReadU32LE?(); // propagate from a compatible Result-returning function
```

Migrate `IsNone()`/`IsSome()` to `IsError()`/`IsOk()`; `.Value()` remains available
after a successful check. Prefer `!()` when success is asserted and `?()` when
propagating. Return-type changes are deliberate, not source-compatible coercions.
`std::binary::DecodeFrame` is migrated; its existing frame-level errors remain
unchanged rather than exposing unrelated reader internals accidentally.

Read errors have `domain = std::ResultDomain::io`, `nativeCode = 0`, and these
`std::builders::ReadErrorCode` values:

| Code | Name | Meaning |
| --- | --- | --- |
| 1101 | `unexpectedEnd` | Missing/truncated fixed read, byte range, varint or string payload |
| 1102 | `invalidVarUInt` | Unsigned LEB128 exceeds 64 bits, including a continuing tenth byte |
| 1103 | `invalidUtf8` | Length-delimited payload is not valid UTF-8 |

Every failed reader operation leaves its cursor unchanged. ReadString rolls
back its length prefix too. Empty reads/strings succeed; embedded NUL bytes are
preserved. Nonminimal but in-range LEB128 remains accepted, matching the prior
reader. Reader input and writer `ToArray()` results remain value snapshots.
Sequences of separate read calls do not acquire an implicit transaction: an
earlier successful read stays consumed if a later read fails or propagates.

StringBuilder gains `OnConstruct(capacity: usize)` and `Reserve(capacity: usize)`
as allocation hints. `Clear()` empties content through the in-place path;
Append/AppendLine no longer rebuild the prefix. `ToString()` returns an
independent string snapshot, not a dangling view or destructive transfer.
AppendChar retains the existing byte-char contract; AppendCodePoint remains the
checked Unicode scalar operation. Bulk WriteString preserves byte length/NUL;
ReadString still validates UTF-8. Reserve is not a public exact-capacity promise
across native bridge marshalling, and this slice does not add a Capacity API.
Repeated AppendCodePoint still uses the existing Unicode native/ref bridge;
its VM marshalling cost is not removed by the Append/Push fast path.

## Implementation

Native uses existing in-place string intrinsics and a small installed
`std_builders.h` for reserve and bulk byte/string conversion. ByteWriter extends
arrays in bulk; ByteReader slices bytes and validates one bulk-created string,
instead of performing a native call per byte of a growing result. Bulk helper
inputs are view borrows: C++ does not clone the whole input buffer per read.
VM native marshalling remains subject to its own staging costs; this slice
does not claim zero-copy bridge transfers.

VM Value already owns its string inline and deep-copies ordinary string values.
Append/Push/Clear now mutate the actual mutable receiver directly, preserving
capacity and avoiding a fresh prefix copy. Invalid argument types and immutable
receivers keep their diagnostics; other mutators and distinct receiver/target
pairs keep the prior transactional path. This is a builder-related runtime fix,
not completion of the later broad VM optimization pass.

## Native measurement

30,000 appends of a fixed 32-byte fragment, 960,000 output bytes, followed by an
explicit string snapshot. All variants report FNV-1a `2058931307218008101` on
every sample. Compilation, process startup, checksum hashing and reporting are
outside the internal timer; snapshot construction is included. Ordinary global
new/new[] calls are counted inside that region, not malloc/aligned allocations,
allocation bytes or peak memory. The C++ reference performs the same repeated
append/output snapshot without Wio's generated call/storage wrappers.

Windows build 26100, Intel Core i7-13620H, MinGW GCC 15.2.0, C++20, `-O2
-ffp-contract=off -static`, without fast-math. Original/current generated sources
are compiled against the same current toolchain, headers and runtime archive.
15 interleaved measured rounds follow one excluded warm-up round. CPU power/
affinity was not pinned; these are diagnostic medians/ranges, not universal
guarantees. Source/compiler/runtime/generated/binary hashes and every sample are
in [the raw evidence](evidence/std-builders-2026-10-15.json). Local generation
retains the user's pre-existing unrelated std worktree changes; remote clean
correctness qualification is separate from these local performance samples.

| Variant | Median | Range | new/new[] calls |
| --- | ---: | ---: | ---: |
| Before this std slice (`a97b0c4` compiler/old builders) | 389.139 ms | 360.958–402.821 ms | 120,004 |
| Current builders (`ddfa2b2`) | 4.897 ms | 4.687–5.139 ms | 90,020 |
| C++ reference | 0.584 ms | 0.539–0.688 ms | 17 |

**79.47x faster**, 98.74% less measured time, but still **8.39x C++** for this
workload. This does not meet the final 1.05–1.10x target. The prefix-copy defect
is removed; substantial remaining allocation/copy cost is visible in generated
StringBuilder.Append: an owned string argument, heap-backed parameter Place,
and materialized string copy before the intrinsic. The 30,000 calls still cause
roughly three allocations each. These managed parameter/temp costs are a next
hot-path target, not erased from the report by measuring only a scalar kernel.

The earlier scalar Mandelbrot gain is a different workload and must not be
multiplied into this result. No fresh C#/Python comparison is claimed.

The preceding `d09b8e2` snapshot measured 4.984 ms versus paired original
386.073 ms and C++ 0.596 ms in its own 15-round run. Its Append body is unchanged
by the read-only bulk-helper follow-up. Do not interpret the small 4.984 to
4.897 ms difference across sessions as a demonstrated new optimization gain.

Reproduce with PowerShell 7, built Debug Wio/runtime and MinGW GCC:

```powershell
pwsh -File tests/performance/std_builders/run.ps1 `
    -OutputDirectory F:/Projects/wio/build/std-builders-new-run `
    -BaselineDirectory F:/Projects/wio/build/std-builders-baseline `
    -Repetitions 15
```

The baseline directory must contain saved `append.wio.cpp` from the original
compiler/std state. It is recompiled, not run as a stale binary. The output
directory must be fresh; snapshots and measurements are never overwritten.
The existing `native_scalar` benchmark is separate and was not rerun here.

## Full Mandelbrot follow-up

A separate copy of the user's 800 x 800, 100-iteration P3 Mandelbrot workload
now completes. The original playground source/output are untouched. Seven
interleaved measured rounds follow one excluded warm-up; each Wio and C++ run
must produce **5,156,528 bytes** with SHA-256:

```text
1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733
```

| Complete program | Median external wall time | Range |
| --- | ---: | ---: |
| Current Wio, `ddfa2b2` | 247.528 ms | 245.341–251.242 ms |
| C++ reference | 55.233 ms | 53.425–56.724 ms |

These times include process startup, computation, formatting, console reporting
and file write/close, but exclude compilation and output hash verification.
Writes use normal OS caches, not durable disk synchronization. Same GCC and
`-O2 -ffp-contract=off -static` policy; Wio additionally links its normal runtime
archive. The C++ reference uses a separate pixel buffer and `to_chars`, whereas
Wio interleaves computation/interpolation/append, as in the original audit.
They produce identical images but are not identical storage algorithms.

Wio is still **4.48x C++** on this whole-program wall metric. The historical
300-second timeout is not a measured exact baseline and came from another
revision/session; do not invent an exact full-program speedup from it. This is
native completion/hash evidence, not VM full-render performance qualification.
Samples and source/binary hashes are preserved in
[the full-workload evidence](evidence/mandelbrot-full-2026-10-15.json); sources,
generated output and the prior audit's C++ reference are retained under ignored
`build/mandelbrot-ddfa2b2` and `build/mandelbrot-audit`. This diagnostic follows
the existing local audit and is not yet a standalone clean-checkout benchmark
runner. The smaller append runner above is versioned/reproducible separately.

## Qualification

Only three newly introduced tests run for this slice:

- `wio_std_builders_style`: scoped source-style/Result guard for builders/binary,
  including no compact fn bodies or legacy attachments/Option-return helpers,
  blank lines after blocks, control-flow spacing and proposed 120-column width.
  This is not a complete syntax-aware language formatter.
- `wio_vm_string_capacity`: reserved storage address/capacity across repeated
  append/clear, value-copy independence, reallocating self-append, failed mutation
  rollback, immutable-target rejection and non-string access rejection.
- `wio_std_builders_consistency`: real native and bytecode+native-bridge runs of
  the same source, snapshots/self-append, reserve/clear, UTF-8/scalar/NUL behavior,
  endian reads, maximal varint, truncation/overflow errors, transactional reads,
  Result unwrap/propagation, bulk byte paths and binary frame consumption.

Local Windows Debug final view-borrow check: **3/3 passed in 24.02 seconds**.
Clean platform CI for `ddfa2b2` also builds the real Wio CLI and passes:

| Platform/configuration | New tests | CTest duration | Evidence |
| --- | --- | ---: | --- |
| Windows / Debug | 3/3 passed | 46.32 s | [job](https://github.com/tynes0/wio/actions/runs/38673339571/job/116221295524) |
| Linux / Release | 3/3 passed | 15.68 s | [job](https://github.com/tynes0/wio/actions/runs/38673339571/job/116221295530) |
| macOS / Release | 3/3 passed | 13.54 s | [job](https://github.com/tynes0/wio/actions/runs/38673339571/job/116221295267) |

The [complete run](https://github.com/tynes0/wio/actions/runs/38673339571)
is successful: **9/9 new test executions**. These are correctness test durations,
not remote performance measurements. The older corpus is not rerun, per the
scoped testing policy; diagnostic reference validation remains 693 IDs, now
with 49 committed std Result construction sites. Other platforms/configurations
and universal speed/zero-allocation guarantees are not inferred from this matrix.

## Still open

Managed parameter/temporary promotion and copy/retain elimination; numeric
formatting directly into buffers; ordinary string `+=`; broader text/string
checked retrieval; ByteBuffer/BytePool; remaining std Result/Try/style migration;
complete-app/performance budgets and broad VM/compiler-throughput passes.
No release or final performance-budget closure is claimed.
