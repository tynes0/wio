# N03: bounded scalar field projections

Subsequent work: [bounded private receiver entries](WIO_RECEIVER_OPTIMIZATION.md).
Historical measurements below remain unchanged.

2026-10-17. Follow-up to the native string borrow transport pass (`318b610`).
N03 remains in progress; this is not closure of the native performance budget,
N05, the heavy VM pass or compile-time optimization.

## Cost removed

Even a simple `self.count += 1` previously constructed a general `Place<u64>`
for the field. That representation supports shared cells, proxy reader/writer
functions, cached proxy values and callback lifetime validation. Most of those
features are unnecessary for a primitive field used immediately in a call-free
interval. Constructing/destructing those fields also enlarges the generated
method and its temporaries, even though `Place::borrow` itself does not allocate.

The C++ storage plan now proves a bounded direct-field interval and emits a
one-word pointer for that internal SSA projection. Reads and writes become
ordinary dereferences. The receiver still goes through the **same checked
`value_base`** when its field address is formed. No Wio source reference gains a
longer lifetime, and no public/native/SDK function signature changes.

Conceptually:

```cpp
// Before: general proxy/callback-capable field transport.
auto field = Place<std::uint64_t>::borrow(checkedReceiver._f0);
field.write(WrappingAdd(field.read(), 1ULL));

// After: only when the complete interval is proven closed.
auto* field = std::addressof(checkedReceiver._f0);
*field = WrappingAdd(*field, 1ULL);
```

This is general code generation, not a Counter/ToString/benchmark-name special
case. Object and component fields both qualify. Owned strings, containers,
objects, enums and native POD fields are not promoted by this proof.

## Proof boundary

- Synchronous functions only; async/coroutine frames retain their representation.
- Explicit Wio object/component layout and matching primitive field type/index.
  Native layouts/bindings and ambiguous/missing layouts retain `Place`.
- All uses must be trivial loads or scalar stores to that exact field in the
  same basic block. References, captures, returned addresses, metadata escapes
  and CFG-edge transport reject promotion.
- From field definition to last use, only primitive constants/arithmetic and
  that field's own loads/stores are allowed. Calls, release, other getters or
  writers, projections, branches and unknown instructions are hard barriers.
  An unrelated primitive getter can be a proxy; it is not assumed pure.
- At most 32 instructions from projection to last use. This bounds proof work and the lifetime
  interval; a longer window simply retains the existing representation. It is
  not a source-language restriction. Functions without candidates skip the
  auxiliary value-type index.

Established receiver access/lifetime checks remain; an `ObjectBorrow` also keeps
its existing null check. No arbitrary call, suspension, receiver release or
borrowed-field escape can occur before the last access in a promoted interval.
Failure to prove that fact retains the original
per-access checked representation. Null checks, arithmetic semantics and source
mapping remain; there is no global switch disabling borrow/lifetime checks.

## Second slice: one reservation for rendered string parts

After the field-only comparison, a second native-only emission change preflights
interpolation containing at least two already-rendered string operands. It sums
their byte lengths and literal segments with checked arithmetic, reserves before
appending the prefix, then appends in the existing order. The new leaf header
`formatted_size.h` supplies the checked size fold; no streams, formatting calls,
temporary string array or dynamic metadata are involved.

Only actual String operands qualify. User hooks have already run as ordinary WIR
calls before this final interpolation; arbitrary objects, numeric facets and
containers are never invoked during sizing. A single operand retains its current
path rather than adding sizing work to every scalar conversion. Text results
keep the existing final UTF-8 validation. Repeated operands and embedded NUL use
real byte lengths, not C-string scanning. No shared WIR/VM expansion is added.

The local pre-reserve buffer is not visible externally. Size overflow/maximum
length is rejected before summation can wrap; allocation errors do not mutate
source strings. This does not promise an STL-independent exact capacity policy
or remove all allocations from a long output. Integer locale/floating formatting
contracts are unchanged, not weakened to improve the benchmark.

## New relevant validation

The new synthetic storage-plan unit tests promotion for object/component fields
and rejection of calls, native calls, receiver release, unknown proxy getters/
writers, borrows, captures, metadata escapes, CFG transport, native layouts,
long windows and async frames. They test the emission proof, not arbitrary
malformed IR's acceptance by the WIR verifier.

The expanded Wio gate runs native O0/O2 and VM, including component extensions,
integer/bool/f64 fields, loop mutation, and a scalar field passed by `ref` to a
separate function (which must keep its ordinary transport). Existing focused
hook-order/interface/generic/NUL/Unicode and native string borrow checks also
remain in that gate. Known WVM1062 container hooks and WVM1003 indirect native
function values stay explicit negative boundaries, not parity claims.

The first fixture used `public fn` inside an extension; Wio requires `public ref
fn`/`public view fn` there. That authoring failure is retained and corrected,
not treated as a compiler optimization defect. The field-only two-test stage
passed in 77.99 seconds. The final three relevant tests passed in 78.79 seconds:
the checked-size unit covers zero, exact limit, aggregate maximum and overflow;
Wio adds long/repeated/NUL string parts, source snapshots and Unicode Text
results. No old functional corpus ran locally.

## Measurement protocol

Before artifacts are frozen binaries from `318b610`, copied without rebuilding
them. Their build logs retain the original paths. Reuse preparation only collects
allocation diagnostics and writes the manifest. The before generated C++ SHA256
is `a979c38b00006f8b2c9b5c926860e6cda61da81e0dc13e7b9a44f3fdc589310c`.

Wio/C++/C#/header/project workload sources and 10,001-iteration inputs are
identical before/after. The Wio-owned controller is unchanged (SHA256
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`).
C++/native are O2, C# is .NET 9 Release, and the VM Release host is unchanged.
Builds are serial and finish before sampling. Keep all warmups, A/A controls,
reversed/rotating comparison rounds and the distinct repeated final native/VM
phase, without retry-to-pass or trimming. No power/priority/security changes or
unrelated process kills are made. Python owns preparation, not timing samples.

Global-new/delete diagnostic windows, root-process peak RSS, generated/native
artifact size and C# current-thread managed bytes are separate metrics. Do not
equate them. This projection change targets stack/transport cost; it does not
claim additional heap savings from field borrows that already allocated nothing.
The pre-existing dirty std/playground files remain untouched. Declared hashes
are retained, not a clean full-transitive-source qualification.

## Observations: before -> scalar fields -> fields + reserve

Comparison medians, milliseconds:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 1.5623 -> 1.0518 -> 1.0735 | 56.0445 -> 57.1366 -> 53.6896 | 0.4400 -> 0.4266 -> 0.4363 | 1.0750 -> 1.3115 -> 1.0448 |
| Ordered hooks + builtins | 4.0313 -> 2.9288 -> 2.7163 | 78.1656 -> 78.6215 -> 74.5209 | 1.3805 -> 1.3588 -> 1.3683 | 2.4912 -> 2.5256 -> 2.4154 |

Each stage also has its own separate final native/VM phase:

| Case | Native before -> fields -> final | VM before -> fields -> final |
| --- | ---: | ---: |
| Single hook | 1.5398 -> 1.0278 -> 1.1589 | 54.8921 -> 56.7637 -> 55.7139 |
| Ordered hooks | 3.9173 -> 3.1329 -> 2.8653 | 80.7837 -> 78.4215 -> 76.8435 |

All three full sessions have **84/84 correct outputs** (252/252 together), applied
affinity, unchanged declared endpoint artifacts and unchanged endpoint power
profiles. All remain globally **timingQualified=false**. In the final session,
ordered C++/C# comparison ranges are 0.31/0.28; single native final range is 0.25,
and final VM ranges are 0.62/0.29. No sample is trimmed or replaced, and no
qualified speedup percentage or no-regression promise is accepted.

The field slice lowers native medians in both phases. The reserve slice further
lowers the long-case native observations and deterministic allocation traffic,
but the single-case final native observation rises from 1.0278 to 1.1589 ms.
That rise is not hidden or accepted as zero cost. The single case does not enter
the new reserve branch; the two modes share one compiled kernel, and code/layout
effects versus measurement variation are not isolated by these unqualified
sessions. C++ still has a substantial gap; the 5-10% native target is not met.
No VM performance gain is claimed from native-only emission changes.

The previous dated session had native medians 1.5767/4.0452 ms; the present fresh
frozen-binary baseline is 1.5623/4.0313 ms. Earlier pre-transport observations
were 2.3763/5.2681 ms. Keep those historical sessions separate, not selectively
interchangeable denominators.

Native global-new/delete diagnostics, before -> fields -> final:

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2 -> 2 -> 2 | 64 -> 64 -> 64 | 64 -> 64 -> 64 | 0 / 0 |
| Ordered hooks | 15004 -> 15004 -> 10003 | 615156 -> 615156 -> 315097 | 156 -> 156 -> 97 | 0 / 0 |

The field slice does not save additional heap allocations; its benefit is less
general field transport. Pre-reserve removes the extra long-output growth.
Final comparison root-process peak RSS medians (KiB), native/VM/C++/C#:
10300/22204/5884/29780 and 10312/22204/5888/36700. Before native RSS was
10308/10316 KiB. These small process shifts do not measure the field temporary's
stack footprint or prove a whole-process memory reduction. C# current-thread
managed allocations remain 1589768/3940040 bytes, a different scope.

This Windows x64 build also reduces static artifact sizes:

| Artifact bytes | Before | Fields | Fields + reserve |
| --- | ---: | ---: | ---: |
| Generated C++ | 3305077 | 3264457 | 3265016 |
| Unstripped native executable | 6565203 | 6405511 | 6407351 |

The reserve slice has a small code-size cost relative to fields alone, while the
combined result remains smaller than before. Intermediate generated C++ SHA256:
`756d1660ccb7c1841a6006ba83e25745c9c4c671135206d9b5a52e2bb2f9383c`;
final measured generated C++:
`08e6e3aa48144ad551495347734ec3861a21194195763d37ce85edf8b574fb91`.

## Post-measurement warning-only annotation

An additional GCC `-Wall -Wextra -Werror` check exposed an unused lambda warning
for the empty checked-size pack, which generated multi-string interpolation does
not use. Final source adds only `[[maybe_unused]]` to that lambda. Strict GCC
compilation/execution now passes; the two C++ tests recheck passed in 1.44 seconds.
O2 assembly for the complete checked-size unit is byte-identical before/after
that annotation (SHA256
`8c49b866eef2906ed849153dea2bb0c42fda031fef787b7622fc0b5ab0fa7bcc`).
The timing artifacts above remain the pre-annotation frozen binaries, not
replaced by a favorable rerun. The measured helper header SHA256 was
`9ba4b0549dc22dbaf51c57699f291b8210f43a6b720410a5d3c07ddd3f6da355`;
final annotated header is
`72827ed12b73c5e067fe07d179a14b20eb35c0dfb8d3123c56a8582ff9319ea6`.
This warning-only code-generation check does not upgrade timing qualification
or prove every unmeasured configuration identical.

## Remaining N03 work

Measure and remove remaining receiver `Place<Ref<T>>`/view-copy and owned-string
merge transport, then isolate integer locale/fallback costs without silently
changing formatting semantics. Broader scalar-field opportunities (for example
proven closed local loads inside the interval) currently retain the safe
fallback. Richer formatting, legacy Format and KI-008 hooks remain; KI-009 is not
fixed here. N04 has not started; N01/N14 acceptance and the fixed N05 storage
scope are not renumbered or declared complete by these two slices.

## Evidence

- [Before session](evidence/n03-field-before-2026-10-17.json),
  [build](evidence/n03-field-build-before-2026-10-17.json),
  [allocation window](evidence/n03-field-memory-before-2026-10-17.json).
- [Field-only session](evidence/n03-field-intermediate-2026-10-17.json),
  [build](evidence/n03-field-build-intermediate-2026-10-17.json),
  [allocation window](evidence/n03-field-memory-intermediate-2026-10-17.json).
- [Final session](evidence/n03-field-after-2026-10-17.json),
  [build](evidence/n03-field-build-after-2026-10-17.json),
  [allocation window](evidence/n03-field-memory-after-2026-10-17.json).
- [Three focused tests](evidence/n03-field-tests-2026-10-17.xml),
  [Wio native/VM process evidence](evidence/n03-field-validation-2026-10-17.json),
  [field-only tests](evidence/n03-field-intermediate-tests-2026-10-17.xml),
  [post-annotation C++ tests](evidence/n03-field-post-annotation-tests-2026-10-17.xml).
- [Initial extension fixture error](evidence/n03-field-fixture-failure-2026-10-17.xml),
  [process record](evidence/n03-field-fixture-failure-2026-10-17.json),
  [strict-warning check](evidence/n03-field-strict-warning-2026-10-17.txt).
