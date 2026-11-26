# N03 formatting/builders closure

2026-10-18. This closes the **implementation scope of N03**, not the N14 native
performance budget, the VM optimization pass or all v1 correctness work.
Cross-platform qualification is tracked separately from local correctness.

## Completed remaining surfaces

- Object/interface hooks inside arrays, nested arrays and dictionaries now use
  generated ordinary WIR iterator/call/append functions. Stateful hooks run once,
  in traversal order; dictionary keys render before their values. Builtin-only
  containers retain their existing fast path. Helper identities are interned per
  concrete type and the transformation is idempotent.
- Synchronous `ToString() -> string` works in async/coroutine callers, before
  and after suspension. Formatting is materialized before coroutine frame/CFG
  lowering, not by calling arbitrary Wio code from the VM ValueFormatter.
- Concrete generic-owner methods are specialized, including native method
  declarations. A native ToString method retains its original public WIR method
  identity/virtual slot and forwards through a private explicit-receiver native
  ABI adapter. The adapter owns its object claim, rather than treating implicit
  self as a mutable handle cell. A nested native-to-Wio method call is tested.
- Native POD `operator<<` formatting has a generated, registered native thunk.
  Scalar, view and POD-array interpolation now work in VM as well as native C++.
  NUL/Unicode, seek/high-water output, partial badbit output and translated native
  exceptions retain the prior stream behavior. This is compiled bridge code,
  not C++ reflection or a VM stream implementation.
- The builder pins an interpolation operand's **emitted type** after loading a
  ref/view, not the source expression's reference type. Owned POD copies are
  distinct from destruction: trivially-copyable native PODs may create an owned
  SSA claim without a destructor. Typed/Lowered/bytecode verifiers agree on this
  bounded copy rule; primitives and nontrivial forged PODs are not admitted.
- Open generic owners/signatures do not produce native registry thunks that
  refer to nonexistent C++ classes/functions. Existing concrete registry
  entries remain roots; this is not registry dead-code elimination.

## Ownership and native cost bounds

Container calls receive a separate owned argument claim. They cannot consume
the caller's original SSA value and later clean it twice. Container snapshots
also stabilize traversal while hooks execute. This can copy container storage;
it is not a zero-copy claim. Further proven borrowed/private representation work
belongs to N05/N08, without weakening these semantics.

A bare native/container formatter call writes directly to the interpolation's
result instead of building a second string. A helper moves its private completed
string buffer out of its local place. Already-rendered single strings with
affixes use the same checked total-size preflight as multiple strings. Private
integer/bool fragment storage is excluded from that owning-string path.

The native stream capacity guard from the preceding slice remains. Moving an
oversized native reserve into a long-lived short result is not unconditionally
better than a compact copy. Global type/ABI identities and public SDK signatures
are unchanged; new native adapters are internal registry bindings. No bytecode
instruction or serialized layout is added. The Release verifier must include
the owned-POD copy rule; old runners can reject that newly valid program.

## Indexed/richer formatting audit

The existing console indexed API is retained: D/X/base formats, fixed/scientific
precision, grouping/currency/percent/custom sections, alignment, repeated/reordered
arguments, escaped braces and counted strings. It is not replaced by the display
interpolation policy (console bool spelling, for example, remains independent).

The hot integral/floating paths no longer zero all 1024 scratch bytes before
conversion. An explicit internal scratch tag opts into write-before-commit
storage; the ordinary StackText constructor **still zeroes its buffer**. Only
audited numeric paths use the tag. Every successful View exposes exactly the
bytes written before Resize; errors never expose the unused tail. Stack frame
capacity and heap policy do not change. This is not a blanket uninitialized
buffer API or a claim that every richer format is faster.

The poison-backed scratch gate compares ordinary and tagged buffers across
integer extrema, zero/sign, D/G/X/B, fixed/scientific, grouping/currency/percent,
custom literal/number sections, large precision, infinities and NaNs. It also
checks malformed braces, missing indexes, excessive precision and embedded NUL.
`Q` without numeric placeholders is a valid literal custom section, not an error.

## Frozen display contract

An implicit user formatter is public, unambiguous, zero-argument and synchronous,
returning string. A synchronous hook may be called from async code. An async
ToString is **not implicitly awaited**: await it explicitly and format its
result. Uninstantiated open generics are not executable runtime types. Private,
ambiguous or wrong-signature declarations are not guessed. No new nullable,
opaque/any/tuple formatter protocol or precision syntax for interpolation is
invented in this sprint.

Native methods remain subject to the existing method-only hosted-object ABI
qualification (see `WirNativeObjectEmitter`), and native components must satisfy
the existing standard-layout/trivially-copyable POD contract. This does not make
arbitrary C++ object layouts, public-field native mirrors or other external member
dispatch automatically bridgeable. A Wio-bodied ToString has no such native
boundary requirement merely because it reads its own public fields.

Source operand expressions finish evaluation before the rendering phase;
rendering preserves left-to-right hook order. Null handle handling remains the
existing null/call/merge WIR path. Strings are independent owning snapshots.
JSON keeps its independent round-trip numeric policy.

## Validation and measurement boundary

Only new/expanded N03 gates are run locally, not the full historical corpus.
Six gates pass in **323.90 seconds**: scalar-fragment IR, formatting-call IR,
shared formatting/native reentry/async semantics, generated native POD streams,
indexed scratch/error contracts and the float/container format gate. After the
last verifier restriction to standard-layout trivial component PODs, the two
affected gates pass again in **23.47 seconds**. The final negative protocol cases
reject forged nontrivial/layout/object/RC metadata, without widening primitives.
The six-gate report and separate final verifier report are retained; this is not
a claim that the whole historical suite was rerun on every intermediate binary.

The initial failed gate and pre-transfer passing candidate remain in
`docs/evidence/n03-closure-*`; discovered generic-owner/view/ownership defects
were corrected, not hidden by retaining the old VM-negative assertions.
The final selected gate, platform CI and measurement supplement record the
qualification state for the selected implementation.

Evidence: [six gates](evidence/n03-closure-final-tests-2026-10-18.xml),
[final verifier/POD recheck](evidence/n03-closure-verifier-tests-2026-10-18.xml),
[shared calls](evidence/n03-closure-final-calls-2026-10-18.json),
[final POD/native processes](evidence/n03-closure-verifier-pod-2026-10-18.json),
[indexed scratch](evidence/n03-closure-final-indexed-2026-10-18.json),
[float/container processes](evidence/n03-closure-final-format-2026-10-18.json).
The preceding `c70c58c` passed all three platform jobs (CI 39145272357).
This implementation's own [CI 39149670456](https://github.com/tynes0/wio/actions/runs/39149670456)
also passes all three jobs: Windows Debug, Ubuntu Release and macOS 15 Release.
It qualifies production source `29fabf1`; the following measurement/report commit
does not change compiler/runtime sources. This is the scoped backend-closure
workflow, not a claim that every historical test ran. The retained
[CI snapshot](evidence/n03-closure-ci-2026-10-18.json) pins the full source SHA.

The indexed benchmark uses Wio `console::Format`, specialized C++ `to_chars`
output and C# invariant `string.Format`, with independent Python byte checksums.
Native/VM sampling is controlled by Wio `wio-bench`, including A/A and separate
final Wio repetitions. C++ is a specialized lower bound, not a parser-equivalent
indexed-format implementation; C# also allocates UTF-8 for hashing. Native heap
windows and whole-process RSS remain separate metrics.

The native-POD fixture reuses the preceding slice's frozen five workload sources.
Its old VM WVM1062 failures remain historical missing observations, never zero
times or speedup denominators. KI-007/KI-009 remain independent defects; fixing
formatting does not close them or the overall N14 native/C++ budget.

## Final measurement supplement

Both new after sessions completed with **84/84 correct samples** each, including
warmups and A/A controls. Affinity, unchanged artifact hashes and unchanged power
profile checks pass. Both sessions have `timingQualified: false`; neither is an
accepted speedup or regression measurement. The controller's nonzero exit for
timing qualification must not be mistaken for incorrect workload output. Every
sample is retained, including slower observations; no favorable rerun was used.

Comparison medians, milliseconds per complete workload (five samples):

| Workload | Wio native before -> after | Wio VM before -> after | C++ before -> after | C# before -> after |
| --- | ---: | ---: | ---: | ---: |
| Indexed integer, 10001 calls | 1.7441 -> 1.7596 | 76.1142 -> 75.7173 | 0.5945 -> 0.5864 | 1.6801 -> 1.7112 |
| Indexed float, 10001 calls | 1.8739 -> 1.8330 | 79.8034 -> 89.4204 | 0.7329 -> 0.7132 | 2.6959 -> 2.5991 |
| Native POD bare, 2001 calls | 0.9553 -> 0.9337 | unsupported -> 17.9344 | 1.0136 -> 0.8129 | 2.1958 -> 1.9440 |
| Native POD affix, 2001 calls | 22.8379 -> 20.3668 | unsupported -> 55.2488 | 23.7684 -> 19.8339 | 34.0841 -> 24.9267 |

The reference source files are unchanged between each pair. Moving reference
times are measurements, not reference optimizations. In particular, the higher
indexed-float VM comparison time is retained, not explained away or omitted.
POD affix has a 70.3% relative range and a 36.8% native A/A difference; it cannot
support the tempting apparent speedup claim. Indexed native integer/float ranges
are 11.0%/10.5%, with A/A differences 5.2%/7.5%. These are not N14 acceptance.

Separate final Wio phase medians (three samples), also milliseconds:

| Workload | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Indexed integer | 1.7439 -> 1.7459 | 75.2389 -> 75.6961 |
| Indexed float | 1.9810 -> 1.9127 | 79.1216 -> 79.2651 |
| Native POD bare | 1.3615 -> 0.9299 | unsupported -> 19.9861 |
| Native POD affix | 23.1462 -> 20.6412 | unsupported -> 44.1672 |

The old POD session has 64 correct supported samples and 20 WVM1062 rejections;
its aggregate `outputsCorrect` is false. The new VM observations demonstrate a
previously missing path, not a speedup against a working old VM implementation.
No VM interpreter dispatch optimization is part of this change.

### Memory and size

Separate native diagnostic images report identical before/after heap windows:

| Workload | Allocations = frees | Requested = freed bytes | Peak tracked bytes | End live bytes |
| --- | ---: | ---: | ---: | ---: |
| Indexed integer | 10001 | 310031 | 31 | 0 |
| Indexed float | 9201 | 285231 | 31 | 0 |
| Native POD bare | 6003 | 1546663 | 774 | 0 |
| Native POD affix | 16008 | 81110425 | 24659 | 0 |

All have zero tracked allocation failures. These replacement-new/delete windows
are not malloc traffic, VM heap accounting or whole-process RSS. This slice
removes numeric scratch initialization, not heap allocations or the 1 KiB stack
reservation itself. Native-POD bridge functionality does not claim new native
heap savings over the preceding selected bounded-stream implementation.

Comparison median whole-process peak RSS, KiB (before -> after):

| Workload | Wio native | Wio VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Indexed integer | 9232 -> 9216 | 21316 -> 21304 | 5192 -> 5192 | 28468 -> 28432 |
| Indexed float | 9268 -> 9248 | 21336 -> 21328 | 5224 -> 5228 | 27748 -> 27700 |
| Native POD bare | 9316 -> 9308 | unsupported -> 20336 | 5288 -> 5288 | 34252 -> 34260 |
| Native POD affix | 9616 -> 9604 | unsupported -> 20644 | 5576 -> 5576 | 36632 -> 36628 |

Tiny RSS differences are observations, not evidence of allocator savings.
Indexed native.exe grows 6383973 -> 6386609 bytes (+2636); POD native.exe grows
6397400 -> 6403203 (+5803). Generated frozen C++ grows 3258789 -> 3259018 and
3263276 -> 3266468 bytes respectively. POD bytecode grows 2515049 -> 2516027
bytes to carry the new formatter binding and instructions. Functionality and
validation have size costs; no universal binary-size improvement is claimed.

### Provenance and retained evidence

The indexed **before** is a controlled runtime-before artifact prepared after
initial hook-lowering edits but before numeric scratch and final codegen edits.
Its recorded Git HEAD is c70c58c with a dirty working tree, **not a clean c70c58c
compiler rebuild**. This no-hook fixture isolates the indexed runtime path; do
not use it as a pristine whole-compiler baseline. The POD before is the prior
selected bounded-stream fixture. Each after build copies the same five frozen
workload files from its own before directory; their hashes agree. The indexed
reference files were later mechanically formatted in the repository, so the
frozen pair, not current repository whitespace, is the comparison input.

After artifacts use production sources from `29fabf16e3fdf0252b12d843caff4577d186b365`:
Debug compiler, Release VM, GCC 15.2 generated code at O2 with FP contraction off,
and .NET 9 Release with tiering disabled. Debug compiler/controller configuration
does not make the timed generated program Debug. Artifact identities, workload
hashes and size changes are retained in the [artifact manifest](evidence/n03-closure-artifacts-2026-10-18.json).
Build commands and controller results retain native/VM bridge, runtime and
environment details; no local timing overlaps another build or correctness gate.

- Indexed: [before](evidence/n03-closure-indexed-before-2026-10-18.json),
  [after](evidence/n03-closure-indexed-after-2026-10-18.json),
  [after build](evidence/n03-closure-indexed-build-after-2026-10-18.json),
  [before heap](evidence/n03-closure-indexed-memory-before-2026-10-18.json),
  [after heap](evidence/n03-closure-indexed-memory-after-2026-10-18.json).
- Native POD: [previous selected before](evidence/n03-generated-stream-after-2026-10-18.json),
  [after](evidence/n03-closure-pod-after-2026-10-18.json),
  [after build](evidence/n03-closure-pod-build-after-2026-10-18.json),
  [after heap](evidence/n03-closure-pod-memory-after-2026-10-18.json).

N03 implementation is complete for the frozen contract. The separate VM
performance pass, KI-007/KI-009 and N14 budget acceptance remain open; N04 has
not been started as part of this closure.
