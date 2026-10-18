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
This implementation's own platform CI is pending publication/qualification.

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
