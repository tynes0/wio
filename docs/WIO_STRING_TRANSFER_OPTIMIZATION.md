# N03: direct string result storage at single-value edges

2026-10-17. Follow-up to `da385fc`. N03 remains in progress; N04, the heavy
VM pass and compiler-throughput pass have not started.

## Cost removed

Shared formatting-call lowering produces ordinary WIR calls and null/call/merge
blocks. The C++ backend previously stored a fresh hook result in its own optional
string, moved it into an edge temporary, then moved that temporary into the
destination block parameter. Short-string moves can copy the inline character
buffer; this is not always a free pointer transfer.

```cpp
// Before: producer storage, parallel-edge staging, destination storage.
source = call_ToString();
{
    std::string edge = std::move(*source);
    destination = std::move(edge);
    goto merge;
}

// After, only for an independently proven single-value transfer.
destination = call_ToString();
{
    goto merge;
}
```

`WirCppStringTransferPlan` proves the forwarding separately from emission. The
emitter aliases the now-unobservable producer storage to the block parameter,
omits the producer's C++ declaration and skips that edge's redundant transfers.
The destination's optional state, explicit cleanup and later ownership remain
unchanged. This does not remove all optional values or reuse freed heap buffers.

This is not a Counter/ToString-name optimization. It applies to eligible fresh
string constants, interpolation and synchronous value-returning free/member/
extension/virtual/interface/native calls. Public ABI and virtual dispatch remain
unchanged. No Typed/Lowered WIR instruction, block, identity or VM opcode changes.
Producer source-line mapping remains at the original call/creation instruction.

## Proof boundary and safety

- Synchronous functions only. Async/coroutine frame storage retains its existing
  representation. Most functions have no candidate and skip the use index.
- A fresh **owned builtin string** producer must be immediately followed by a
  Jump whose sole target carries that exact value as its only argument. The
  destination must have one owned parameter of the same string type.
- The source has exactly one whole-function use, counting operands, cleanup,
  borrow-origin metadata and all CFG-edge arguments. Any extra use rejects it.
  No source address, reference, capture or explicit source cleanup can survive.
- Loads/copies/moves and borrowed/reference-derived results are not fresh
  producers for this proof. Text, components/native resources and other value
  types retain their representations and move/destruction behavior.
- No effect, cleanup, condition, user operation or second parallel assignment
  can occur between production and transfer. Multiple-argument edges, swaps and
  self-loop argument transport retain the simultaneous temporary-based path.
- The full RHS call/interpolation completes before destination assignment. The
  old destination therefore remains intact during user hooks/native execution
  and if production throws. No callback, check or formatting operation is moved.
- The removed staging operations are standard builtin-string moves, not user
  move constructors. No retained native resource release is elided. Destination
  cleanup stays explicit, and failed proofs keep the established path.

Candidate discovery is block-local; the use scan counts only candidate IDs,
not a second full value/type index. The plan does not mutate the module. Storage
aliases target block parameters, never other producer IDs, preventing cycles.
This is shared representation proof work with N05, not its closure or renumbering.

## Validation and measurement protocol

The new plan gate covers forwarding and generated-storage shape, plus negative
async/coroutine, borrowed source/destination, Text, load/copy, metadata, extra
use/cleanup, intervening effect and parallel-edge cases. The synthetic emitter
shape fixture explicitly bypasses module-contract verification; the Wio
integration fixtures use the full verified compiler pipeline.

The expanded Wio gate adds repeated short/long/NUL strings, independent snapshots
after mutation, native string-returning branches, ordered hooks, Text fallback,
and native string-return exceptions at O0/O2 and in VM. Existing null/view/
interface/generic/borrow/callback-expiry gates remain. Known KI-007/KI-008/KI-009
boundaries are not silently closed by this native-only storage change.

Before binaries are copied from the preceding final build without rebuilding
them. Frozen Wio/C++/C#/header/project sources and 10,001-iteration inputs remain
identical before/after. The Wio controller owns all warmups, A/A controls,
rotating/reversed comparison rounds and separate final native/VM repetitions.
Python prepares serial builds and independent fingerprints, not timing samples.
Native/C++ are O2 with contraction disabled, C# is .NET 9 Release and the Release
VM measurement host is unchanged. All heavy work is serial; no trimming,
retry-to-pass, priority/power/security changes or unrelated process kills.

The user's existing dirty std/playground files remain untouched. Declared
artifact hashes are evidence, not a clean full-transitive-source qualification.
Diagnostic global-new/delete windows, process peak RSS, managed current-thread
bytes and generated/executable sizes remain separate accounting scopes.

## Observations

Comparison medians, **fresh frozen before -> after**, milliseconds:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 0.9110 -> 0.7787 | 54.1125 -> 53.3768 | 0.4349 -> 0.4407 | 1.0483 -> 1.2135 |
| Ordered hooks + builtins | 2.4496 -> 2.1290 | 75.0477 -> 75.2952 | 1.3791 -> 1.3564 | 2.4885 -> 2.9222 |

Separate final repeated native/VM phase, milliseconds:

| Case | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Single hook | 0.8710 -> 0.8260 | 53.0716 -> 54.0693 |
| Ordered hooks | 2.3850 -> 2.1240 | 75.1299 -> 75.7055 |

Both complete sessions retain **84/84 correct outputs** (168/168 together),
applied affinity, unchanged declared endpoint artifacts and unchanged endpoint
power profiles. Both remain globally **timingQualified=false**. Final candidate
native comparison ranges are 0.04/0.02 with A/A differences 0.06/0.00; final
single native drift is 0.13 and C# single comparison range is 0.25. Those noisy
samples are not trimmed/replaced and no accepted speedup percentage is claimed.
The runner exits 1 for rejected timing qualification here, not workload failure.

Native medians are lower in both phases, but the remaining C++ gap and
1.05-1.10x performance budget are not closed. No VM speedup is attributed to
this C++-only storage decision. C# managed current-thread allocation observations
remain 1589768/3940040 bytes, not comparable to native global-new accounting.

Historical context is separate: the preceding dated receiver/cold-integer
session observed native 0.8473/2.6165 ms (separate final 1.1231/2.7891). The
current fresh baseline reuses those binaries but observes 0.9110/2.4496 (final
0.8710/2.3850). Earlier pre-string-transport observations were 2.3763/5.2681.
Those are dated observations, not interchangeable paired denominators.

## Memory and code-size evidence

Heap diagnostics are unchanged, before -> after:

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2 -> 2 | 64 -> 64 | 64 -> 64 | 0 / 0 |
| Ordered hooks | 10003 -> 10003 | 315097 -> 315097 | 97 -> 97 | 0 / 0 |

The measured Work function loses six optional-string declarations, and its six
mutually exclusive null/call edges no longer construct `_edge0`. This is actual
emission, not a claim that every WIR string became unboxed. In this exact GCC
O2 Windows build, Work's prologue stack reservation falls from **0x408 (1032)
to 0x2b8 (696) bytes**, with the same eight saved general registers: 336 fewer
reserved bytes in that function. This includes spill/layout/call-space choices,
not all caller/callee frames or a whole-program peak stack measurement. See
[the actual prologue excerpts](evidence/n03-transfer-stack-prologues-2026-10-17.json).

Final comparison process peak RSS medians (KiB), native/VM/C++/C#:
9208/21328/5200/28620 and 9212/21324/5200/35532. Before native RSS is 9236/9240;
these small shifts are not a strong whole-process memory-reduction claim.

| Artifact bytes | Before | After |
| --- | ---: | ---: |
| Generated C++ | 3265926 | 3264419 |
| Unstripped Windows native executable | 6401165 | 6398093 |

The raw source-size difference includes a five-byte output-folder path change;
it is not all removed instructions. Final generated source SHA256 is
`7b1fd9ad1dabfe3bc5598d917c608cd0e06d8e2089c7ae564189fe44c24376f1`.
The executable is 3072 bytes smaller; artifact size is not heap use or speed.

## Gates and retained evidence

The new plan/emitter gate and expanded Wio integration gate pass **2/2 in 86.41
seconds**. The new helper and unit also pass GCC C++20 syntax checking. An
initial test-authoring typo called nonexistent TypeTable::textType; the fixture
uses the existing intern API instead. Its diagnostic excerpt is retained, not
misrepresented as a production compiler regression. No old full corpus ran.
The preceding commit's three-platform CI passed; this change needs its own CI.

- Before: [all samples](evidence/n03-transfer-before-2026-10-17.json),
  [original build provenance](evidence/n03-transfer-build-before-2026-10-17.json),
  [memory](evidence/n03-transfer-memory-before-2026-10-17.json).
- After: [all samples](evidence/n03-transfer-after-2026-10-17.json),
  [build](evidence/n03-transfer-build-after-2026-10-17.json),
  [memory](evidence/n03-transfer-memory-after-2026-10-17.json).
- Correctness: [CTest](evidence/n03-transfer-tests-2026-10-17.xml),
  [Wio/native/VM processes](evidence/n03-transfer-validation-2026-10-17.json),
  [initial fixture diagnostic excerpt](evidence/n03-transfer-initial-fixture-error-2026-10-17.json),
  [GCC syntax](evidence/n03-transfer-gcc-syntax-2026-10-17.json).

N03 remains open. Next cost attribution is the remaining formatting-result to
local-cell transfer, independently materialized primitive parts, live locale
work and richer/legacy formatting. Do not defer formatting across user hooks
or revive the rejected VM-expanding streaming path without a valid cost model.
N04 has not started; KI-007/KI-008/KI-009 and N01/N14 acceptance stay explicit.
