# N02 string/text/concat closure

2026-10-16. N02's production optimization pass is complete within the proof
boundaries below. N01 residual profiling, N03 formatting/builders, N05 Place
representation, other std operations, VM speed and compiler throughput remain
separate work. This is not a universal zero-overhead or whole-language parity
claim. In particular, KI-007 remains an open pre-existing correctness defect.

## Implemented paths

1. `value = value + suffix` now reuses a proven mutable direct cell, as `+=`
   already could in the first slice. Stable local loads and literal evaluation
   may separate the LHS load from the addition. Calls and unknown effects cannot.
2. `value = value + a + b + c` uses a transactional multi-append. It computes all
   lengths and reserves before the first write; failure leaves the destination
   unchanged. Repeated self operands use the original byte length, including NUL.
3. Pure concat trees, including `a + (b + c) + d`, produce one checked-reserve
   result buffer. Leaf evaluation stays at its original instructions. Interior
   sums and their now-unnecessary cleanup disappear. Shared/observed interiors
   are not silently consumed.
4. Outside those pure trees, independently owned sole-use operands are passed as
   rvalues. Existing cleanup remains and destroys valid moved-from values.
   Borrowed values, aliased snapshot-copy elisions, phi/cross-block definitions
   and parameters are not inferred to be consumable.
5. `Text` has lvalue/rvalue concat overloads backed by the corresponding standard
   string operations, with explicit self-alias guards. Ordinary lvalue concat
   no longer copies a prefix and then reallocates it. No UTF-8 revalidation is
   needed when joining already validated Text values.
6. Identical-place direct field/global cells can use append reuse. Projected
   proxy/array/dictionary references and unknown ref/view parameters retain the
   general path. This is not arbitrary alias equivalence across two FieldPlaces.

No language syntax, WIR opcodes, bytecode ownership or native ABI layouts changed.
Async/coroutine functions keep their established emission/storage model. Proof
and variadic-emission work is bounded to 64 concat nodes per selected group;
longer expressions remain supported by fallback/partial groups and rvalue reuse.
Borrow metadata and CFG edge arguments count as observations, not just operands.
Only builtin String/Text with matching operand/result types enters these helpers.

Generated examples (schematic names):

```cpp
destination.read() += suffix;
wio::runtime::AppendConcat(destination.read(), a, destination.read(), b);
result = wio::runtime::Concat(a, b, c, d);
result = std::move(ownedLastUse) + evaluatedSuffix;
```

## Measurement sequence and scope

Baseline is `689654b`, which already includes the first local `+=` optimization;
it is **not** the original legacy generator. Four identical frozen Wio/C++/C#
workload sources were built before the production edits. After the first actual
optimization, an intermediate build was measured. The final tree-flattening
change was then built and measured separately. No rejected sample was trimmed
and no run was repeated until it passed timing gates.

The Wio-written `wio-bench` collected each session: one external warmup, five
comparison samples, native exact-command A/A controls, then three fresh final
native/VM samples plus separate warmups/controls. Every process also does two
in-process warmups. All work runs serially, CPU 2 pinned before launch, unchanged
observed power profile, Windows x64, GCC 15.2 C++20 O2 `-ffp-contract=off`, .NET
9.0 Release (tiering disabled), Release VM host. No system settings changed.

Each workload uses 2001 runtime-selected repetitions and independent full-byte
FNV fingerprints. String source contains NUL; text contains emoji, combining
marks and Japanese. C# stores UTF-16 and allocates a UTF-8 array for the hash;
native lanes hash their UTF-8 bytes directly. Literal interning, allocator/GC,
checked Place/native-view transport and process initialization are not claimed
identical. C++ uses the **same high-level expressions**, not a separately tuned
`+=`/reserve implementation. Do not interpret faster Wio rows as beating the
fastest possible C++. The target's tuned-C++ performance budget stays N14 work.

All **504/504** process outputs are correct across before/intermediate/final;
all three sessions passed their endpoint artifact, affinity and observed-power
checks. **All three sessions fail overall timing qualification.** Some lanes
pass their individual coarse gates, but range/drift/control failures elsewhere
remain. Reported medians below are descriptive, not qualified speedup factors.

Endpoint hashes cover declared artifacts, not the entire transitive repository.
The final manifest additionally fingerprints the compiler planning/emission
sources and new concat helper; baseline/intermediate declarations are narrower.
Build command logs and frozen generated C++/native/bridge/bytecode remain under
the three `build/n02-concat-{before,after,final}-2026-10-16` directories. The exact
measurement controller was copied to `build/n02-concat-frozen-controller-2026-10-16.exe`
(SHA256 `2f0bf7c1ce69f1c359a9643acbde998451c8a3bde3e73d61b51177bb138431bc`)
before rebuilding the installed build-tree tool with the new optimizer.

## Plain runtime medians

Milliseconds; concat + initialization + hash + local cleanup, not process
startup, input parsing or report serialization:

| Workload | Wio native before | Intermediate | Final native | Final VM comparison | C++ comparison | C# comparison |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Growing string assignment | 10.7072 | 0.1698 | 0.1658 | 14.4782 | 14.2495 | 12.7903 |
| Growing text assignment | 13.1609 | 0.1655 | 0.1594 | 60.9384 | 12.5712 | 9.3719 |
| Six-part string chain | 0.8140 | 0.7368 | 0.5975 | 9.8646 | 0.6087 | 1.0885 |
| Six-part text chain | 1.1478 | 0.7819 | 0.6077 | 27.4102 | 0.6503 | 1.2050 |

The distinct **final Wio-only phase**, not pooled into that comparison:

| Workload | Native median ms | Release VM median ms |
| --- | ---: | ---: |
| Growing string | 0.1650 | 14.9966 |
| Growing text | 0.1581 | 64.7604 |
| String chain | 0.6015 | 9.4453 |
| Text chain | 0.6041 | 27.8803 |

No VM interpreter optimization is claimed: VM execution remains on its old
algorithms, with a fresh sidecar/bridge for each build. Its timing variation is
not attributed to native concat planning. This is not a Mandelbrot workload;
no new Mandelbrot speedup or cross-date comparison is claimed by these tables.

## Separate allocation/memory diagnostics

Opt-in global-new/delete windows, ordinary/aligned allocations included by the
existing benchmark allocator. These are requested bytes, **not** RSS, copied
bytes, malloc/custom allocator traffic, managed allocations or production time.

| Workload | Allocations before | Intermediate | Final | Requested bytes before → final | Peak live requested before → final |
| --- | ---: | ---: | ---: | ---: | ---: |
| Growing string | 2005 | 16 | 16 | 90,137,227 → 184,468 | 180,228 → 138,423 |
| Growing text | 4013 | 24 | 24 | 252,130,575 → 172,534 | 420,199 → 129,306 |
| String chain | 12009 | 8007 | 4005 | 1,769,009 → 602,426 | 633 → 482 |
| Text chain | 22038 | 8031 | 4029 | 3,897,124 → 615,484 | 1441 → 784 |

All windows end at **zero tracked live bytes/blocks**, allocations equal frees
and requested bytes equal freed bytes. Chain results now need one payload buffer;
the remaining per-iteration Place allocation is N05 work, not a hidden payload
copy. Common source construction/Unicode validation allocations are included.

Buffer reuse trades retained spare capacity for fewer copies/allocations. Multi-
append requests geometric growth (at least 1.5x when growing), bounded by max_size,
so STL reserve policies cannot accidentally restore quadratic exact-size growth.
There is no automatic shrink. These final-after-cleanup windows do not measure
retained result capacity. The first-slice retained-capacity tradeoff remains in
[local append evidence](WIO_LOCAL_APPEND_REUSE.md); its different workload counts
must not be compared as though they were this fixture's totals.

Comparison process peak RSS medians in the final session: Wio native 9.05–9.14
MiB, VM 17.55–19.57 MiB, C++ 5.08–5.10 MiB, C# 27.69–46.88 MiB. These whole-root-
process high-water marks include loaded code/metadata/runtime/GC and cannot be
subtracted to infer payload peak memory. Exact samples remain in the raw report.

## Validation and retained failures

Only the three new CTests ran locally; final run **3/3 passed in 19.01 seconds**:
selection/ownership/type/CFG/effect proofs, runtime SSO/growth/NUL/text/self-alias/
allocation-failure behavior, and actual Wio native O0/O2 + VM. Wio also checks
snapshot retention, function-return chains, call order, field append, reference
fallback and explicit snapshot across mutation. The final native-only barrier
fixture preserves the current behavior documented by **KI-007**; this must not
be reported as native/VM parity for that defective expression.

An early runtime test exposed MSVC's unique-rvalue alias constraint; the Text
overloads now guard that case explicitly. Another test reproduced the existing
borrowed-LHS mutation discrepancy; the shared test now requests an explicit
snapshot and the discrepancy stays open in the known-issues register. The first
memory-preparation parser rejected blank lines around otherwise correct JSON;
preparation was fixed without rerunning builds or discarding its rejected log.
All development failures remain under `build/n02-concat-*`. Three-OS CI is wired
for the new tests; local Windows results are not Linux/macOS evidence.

The Wio-written benchmark controller itself was rebuilt with the final compiler
and O2/static native settings. Its strict validation of the final manifest and
reporting of the saved final results both passed. The measurement controller
above remains frozen separately; this smoke check did not replace timing samples.

Raw evidence:

- [Before](evidence/n02-concat-before-2026-10-16.json), [intermediate](evidence/n02-concat-intermediate-2026-10-16.json), [final](evidence/n02-concat-final-2026-10-16.json).
- Allocation [before](evidence/n02-concat-memory-before-2026-10-16.json), [intermediate](evidence/n02-concat-memory-intermediate-2026-10-16.json), [final](evidence/n02-concat-memory-final-2026-10-16.json).
- [Final build commands](evidence/n02-concat-build-final-2026-10-16.json), [Wio test process records](evidence/n02-concat-validation-2026-10-16.json).
- [Fixtures and reproduction](../tests/performance/concat_reuse/README.md).

N02 closes this bounded optimization scope, not every possible string operation
or safety fallback. Next is **N03: formatting/builders**, before N04 bytes/I/O;
the fixed N01–N14 numbering and the separate VM/compiler passes do not change.
